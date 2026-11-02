#include "HostCode.hpp"
#include "Logger.hpp"
#include "EventManager.hpp"
#include "SocketManager.hpp"
#include "TimerManager.hpp"
#include "Exceptions.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <fstream>
#include <sstream>
#include <cctype>

using namespace std;


namespace HostCode
{

static const string Prefix = "cccaster:";

static string joinedCodeStr;

string normalize ( const string& text )
{
    string s = trimmed ( text );

    if ( s.compare ( 0, Prefix.size(), Prefix ) == 0 )
        s = s.substr ( Prefix.size() );

    // 4 digits, 1000-9999
    if ( s.size() == 4 && s[0] >= '1' && s[0] <= '9'
            && isdigit ( ( unsigned char ) s[1] ) && isdigit ( ( unsigned char ) s[2] ) && isdigit ( ( unsigned char ) s[3] ) )
        return s;

    return "";
}

string maskAddress ( const string& addrPort, const string& code )
{
    const string label = "<code " + ( code.empty() ? string ( "?" ) : code ) + ">";
    const size_t i = addrPort.find_last_of ( ':' );
    const size_t b = addrPort.find ( ']' );

    // "ip:port" or "[v6]:port" keeps the port; anything else is replaced whole
    if ( i != string::npos && ( b == string::npos || b < i ) && addrPort.find ( ':' ) == i )
        return label + addrPort.substr ( i );
    if ( b != string::npos && i == b + 1 )
        return label + addrPort.substr ( i );
    return label;
}

// Read a JSON string value starting at the opening quote; returns the position after the closing quote
static size_t readJsonString ( const string& json, size_t i, string& out )
{
    out.clear();
    for ( ++i; i < json.size(); ++i )
    {
        if ( json[i] == '"' )
            return i + 1;
        if ( json[i] == '\\' && i + 1 < json.size() )
        {
            ++i;
            switch ( json[i] )
            {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'u': i += 4; out += '?'; break;
                default: out += json[i]; break;
            }
            continue;
        }
        out += json[i];
    }
    return string::npos;
}

bool parseReply ( const string& json, Reply& reply )
{
    reply = Reply();

    size_t i = json.find ( '{' );
    if ( i == string::npos )
        return false;

    for ( ++i; i < json.size(); )
    {
        while ( i < json.size() && ( isspace ( ( unsigned char ) json[i] ) || json[i] == ',' ) )
            ++i;
        if ( i >= json.size() || json[i] == '}' )
            return true;
        if ( json[i] != '"' )
            return false;

        string key, str;
        i = readJsonString ( json, i, key );
        if ( i == string::npos )
            return false;
        while ( i < json.size() && ( isspace ( ( unsigned char ) json[i] ) || json[i] == ':' ) )
            ++i;
        if ( i >= json.size() )
            return false;

        if ( json[i] == '"' )
        {
            i = readJsonString ( json, i, str );
            if ( i == string::npos )
                return false;
        }
        else
        {
            const size_t j = json.find_first_of ( ",}", i );
            if ( j == string::npos )
                return false;
            str = trimmed ( json.substr ( i, j - i ) );
            i = j;
        }

        if ( key == "code" ) reply.code = str;
        else if ( key == "secret" ) reply.secret = str;
        else if ( key == "proto" ) reply.proto = str;
        else if ( key == "addr" ) reply.addr = str;
        else if ( key == "room" ) reply.room = str;
        else if ( key == "stream" ) reply.stream = str;
        else if ( key == "error" ) reply.error = str;
        else if ( key == "ttl" ) reply.ttl = atoi ( str.c_str() );
        else if ( key == "retryAfterMs" ) reply.retryAfterMs = ( uint32_t ) strtoul ( str.c_str(), 0, 10 );
    }

    return false;
}

string registryUrlFrom ( const string& fileContents )
{
    stringstream ss ( fileContents );
    string line;
    while ( getline ( ss, line ) )
    {
        line = trimmed ( line );
        if ( line.empty() || line[0] == '#' )
            continue;
        if ( line.compare ( 0, 7, "http://" ) != 0 )
            line = "http://" + line;
        return line;
    }
    return HOST_CODE_DEFAULT_URL;
}

string registryUrl()
{
    ifstream in ( HOST_CODE_LIST );
    if ( ! in.good() )
        return HOST_CODE_DEFAULT_URL;
    stringstream ss;
    ss << in.rdbuf();
    return registryUrlFrom ( ss.str() );
}

string urlEncode ( const string& str )
{
    static const char *hex = "0123456789ABCDEF";
    string out;
    for ( unsigned char c : str )
    {
        if ( isalnum ( c ) || c == '-' || c == '_' || c == '.' || c == '~' )
            out += ( char ) c;
        else
            out += { '%', hex[c >> 4], hex[c & 15] };
    }
    return out;
}

static string withQuery ( const string& url, const string& query )
{
    return url + ( url.find ( '?' ) == string::npos ? "?" : "&" ) + query;
}

const string& joinedCode() { return joinedCodeStr; }

void clearJoinedCode() { joinedCodeStr.clear(); }

string displayAddress ( const IpAddrPort& address )
{
    if ( ! joinedCodeStr.empty() )
        return "code " + joinedCodeStr;
    return address.str();
}


// Blocking resolve: one HttpGet on a private event loop (the same pattern MainUi uses for the update check)
namespace
{
struct Resolver : public HttpGet::Owner, public Timer::Owner
{
    bool done = false, ok = false;
    int status = -1;
    string data;
    TimerPtr guard;

    void finish() { if ( ! done ) { done = true; guard.reset(); EventManager::get().stop(); } }

    void httpResponse ( HttpGet *, int code, const string& d, uint32_t remainingBytes ) override
    {
        if ( remainingBytes )
            return;
        status = code;
        data = d;
        ok = true;
        finish();
    }
    void httpFailed ( HttpGet * ) override { finish(); }
    void httpProgress ( HttpGet *, uint32_t, uint32_t ) override {}
    void timerExpired ( Timer * ) override { finish(); }
};
}

bool resolve ( const string& codeText, IpAddrPort& address, string& error )
{
    const string code = normalize ( codeText );
    joinedCodeStr.clear();

    if ( code.empty() )
    {
        error = "Not a host code";
        return false;
    }

    LOG ( "Resolving host code %s", code );

    Resolver r;
    {
        TimerManager::get().initialize();
        SocketManager::get().initialize();

        shared_ptr<HttpGet> get;
        try
        {
            get.reset ( new HttpGet ( &r, withQuery ( registryUrl(), "action=resolve&code=" + urlEncode ( code ) ),
                                      HOST_CODE_HTTP_TIMEOUT ) );
            get->quiet = true; // the reply holds the host's address
            r.guard.reset ( new Timer ( &r ) );
            r.guard->start ( HOST_CODE_HTTP_TIMEOUT * 2 );
            get->start();
            if ( ! r.done )
                EventManager::get().start();
        }
        catch ( const Exception& exc )
        {
            LOG ( "Host code resolve failed: %s", exc.user );
        }
        get.reset();
        r.guard.reset();
    }

    Reply reply;
    if ( ! r.ok || ! parseReply ( r.data, reply ) )
    {
        error = "Cannot reach the host code server";
        LOG ( "Host code %s: registry unreachable (status=%d)", code, r.status );
        return false;
    }

    if ( r.status == 429 || reply.error == "throttled" )
    {
        error = format ( "Too many code lookups, try again in %u seconds", ( reply.retryAfterMs + 999 ) / 1000 );
        LOG ( "Host code %s: throttled", code );
        return false;
    }

    if ( r.status == 200 && reply.proto == "pc" )
    {
        error = format ( "Code %s is a PovertyCaster room, not a CCCaster host", code );
        LOG ( "Host code %s: a PovertyCaster room", code );
        return false;
    }

    if ( r.status != 200 || reply.proto != "cc" || reply.addr.empty() )
    {
        error = format ( "No one is hosting with code %s", code );
        LOG ( "Host code %s: not found (status=%d)", code, r.status );
        return false;
    }

    try
    {
        address = reply.addr;
    }
    catch ( ... )
    {
        error = format ( "No one is hosting with code %s", code );
        LOG ( "Host code %s: bad address in reply", code );
        return false;
    }

    // From here on the address never reaches the log or the UI
    Logger::get().redact ( address.addr, "<code " + code + ">" );
    joinedCodeStr = code;

    LOG ( "Host code %s -> %s", code, maskAddress ( address.str(), code ) );
    return true;
}

// Best-effort blocking GET for the 'end' call: the host is stopping, its event loop may already be gone
static void blockingGet ( const string& url, uint32_t timeoutMs )
{
    if ( url.compare ( 0, 7, "http://" ) != 0 )
        return;

    string rest = url.substr ( 7 );
    const size_t slash = rest.find ( '/' );
    const string hostPort = rest.substr ( 0, slash );
    const string path = ( slash == string::npos ? "/" : rest.substr ( slash ) );

    string host = hostPort, port = "80";
    const size_t colon = hostPort.find_last_of ( ':' );
    if ( colon != string::npos && hostPort.find ( ']' ) == string::npos )
    {
        host = hostPort.substr ( 0, colon );
        port = hostPort.substr ( colon + 1 );
    }

    addrinfo hints = {}, *res = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ( getaddrinfo ( host.c_str(), port.c_str(), &hints, &res ) != 0 || ! res )
        return;

    SOCKET s = socket ( res->ai_family, res->ai_socktype, res->ai_protocol );
    if ( s == INVALID_SOCKET )
    {
        freeaddrinfo ( res );
        return;
    }

    u_long nonBlocking = 1;
    ioctlsocket ( s, FIONBIO, &nonBlocking );
    connect ( s, res->ai_addr, ( int ) res->ai_addrlen );
    freeaddrinfo ( res );

    fd_set wfds;
    FD_ZERO ( &wfds );
    FD_SET ( s, &wfds );
    timeval tv = { ( long ) ( timeoutMs / 1000 ), ( long ) ( ( timeoutMs % 1000 ) * 1000 ) };

    if ( select ( 0, 0, &wfds, 0, &tv ) == 1 )
    {
        nonBlocking = 0;
        ioctlsocket ( s, FIONBIO, &nonBlocking );
        const string req = "GET " + path + " HTTP/1.1\r\nHost: " + hostPort + "\r\nConnection: close\r\n\r\n";
        send ( s, req.c_str(), ( int ) req.size(), 0 );

        // wait briefly for the reply so the request is processed before the socket closes
        fd_set rfds;
        FD_ZERO ( &rfds );
        FD_SET ( s, &rfds );
        timeval tv2 = { ( long ) ( timeoutMs / 1000 ), ( long ) ( ( timeoutMs % 1000 ) * 1000 ) };
        if ( select ( 0, &rfds, 0, 0, &tv2 ) == 1 )
        {
            char buf[256];
            recv ( s, buf, sizeof ( buf ), 0 );
        }
    }

    closesocket ( s );
}

} // namespace HostCode


void HostCodeKeeper::start ( uint16_t port )
{
    _url = HostCode::registryUrl();
    _port = port;
    _code.clear();
    _secret.clear();
    _stopped = false;
    _recreating = false;

    create();
}

void HostCodeKeeper::create()
{
    LOG ( "Registering host code (port %u)", _port );
    _lastCreate = TimerManager::get().getNow ( true );
    request ( format ( "action=host&proto=cc&port=%u", _port ) );
}

void HostCodeKeeper::request ( const string& query )
{
    _httpGet.reset();
    try
    {
        _httpGet.reset ( new HttpGet ( this, HostCode::withQuery ( _url, query ), HOST_CODE_HTTP_TIMEOUT ) );
        _httpGet->quiet = true; // the request and the reply hold the keep-alive secret
        _httpGet->start();
    }
    catch ( const Exception& exc )
    {
        LOG ( "Host code request failed: %s", exc.user );
        _httpGet.reset();
        if ( _code.empty() && owner )
            owner->hostCodeFailed ( this );
    }
}

void HostCodeKeeper::httpResponse ( HttpGet *httpGet, int status, const string& data, uint32_t remainingBytes )
{
    if ( remainingBytes || _stopped )
        return;

    // Parse before dropping the HttpGet: data belongs to it
    HostCode::Reply reply;
    const bool parsed = HostCode::parseReply ( data, reply );
    const uint32_t bytes = ( uint32_t ) data.size();

    _httpGet.reset();

    if ( status == 200 && parsed && ! reply.code.empty() )
    {
        const bool isNew = ( reply.code != _code );
        _recreating = false;
        if ( ! reply.secret.empty() )
            _secret = reply.secret;
        _code = reply.code;

        if ( ! _timer )
            _timer.reset ( new Timer ( this ) );
        _timer->start ( HOST_CODE_KEEPALIVE_MS );

        if ( isNew )
        {
            LOG ( "Host code: %s", _code );
            if ( owner )
                owner->hostCodeReady ( this, _code );
        }
        return;
    }

    LOG ( "Host code registry answered %d '%s' (%u bytes)", status, reply.error, bytes );

    if ( ! _code.empty() && status == 403 )
    {
        // The registry forgot our code (restart / expiry): register again, the UI shows the new code.
        // Never faster than every HOST_CODE_MIN_CREATE_MS, so a sick registry isn't hammered.
        _code.clear();
        _secret.clear();
        _recreating = true;
        const uint64_t since = TimerManager::get().getNow ( true ) - _lastCreate;
        if ( ! _timer )
            _timer.reset ( new Timer ( this ) );
        _timer->start ( since >= HOST_CODE_MIN_CREATE_MS ? 1 : HOST_CODE_MIN_CREATE_MS - since );
        return;
    }

    if ( _code.empty() && _recreating )
    {
        // a failed re-create: try again later (the old code is gone; the UI keeps showing it until a new one comes)
        if ( _timer )
            _timer->start ( HOST_CODE_KEEPALIVE_MS / 4 );
        return;
    }

    if ( _code.empty() )
    {
        if ( owner )
            owner->hostCodeFailed ( this );
        return;
    }

    // keep-alive hiccup: try again next tick
    if ( _timer )
        _timer->start ( HOST_CODE_KEEPALIVE_MS );
}

void HostCodeKeeper::httpFailed ( HttpGet *httpGet )
{
    _httpGet.reset();

    if ( _stopped )
        return;

    LOG ( "Host code registry unreachable" );

    if ( _code.empty() && _recreating )
    {
        if ( _timer )
            _timer->start ( HOST_CODE_KEEPALIVE_MS / 4 );
        return;
    }

    if ( _code.empty() )
    {
        if ( owner )
            owner->hostCodeFailed ( this );
        return;
    }

    if ( _timer )
        _timer->start ( HOST_CODE_KEEPALIVE_MS / 4 );
}

void HostCodeKeeper::timerExpired ( Timer *timer )
{
    if ( _stopped )
        return;

    if ( _code.empty() )
    {
        if ( _recreating )
            create();
        return;
    }

    request ( format ( "action=host&proto=cc&port=%u&code=%s&secret=%s",
                       _port, HostCode::urlEncode ( _code ), HostCode::urlEncode ( _secret ) ) );
}

void HostCodeKeeper::stop()
{
    if ( _stopped )
        return;
    _stopped = true;

    _timer.reset();
    _httpGet.reset();

    if ( ! _code.empty() && ! _secret.empty() )
    {
        LOG ( "Ending host code %s", _code );
        HostCode::blockingGet ( HostCode::withQuery ( _url, "action=end&code=" + HostCode::urlEncode ( _code )
                                + "&secret=" + HostCode::urlEncode ( _secret ) ), 500 );
    }

    _code.clear();
    _secret.clear();
}
