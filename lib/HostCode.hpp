#pragma once

// Host codes: a short code instead of an IP address.
//
// A host registers with a small HTTP registry and gets a 4-digit code that lives only while it keeps
// hosting. Anyone types the code into the join / spectate box (or runs "cccaster:<code>"); the registry answers the
// host's address and the normal connect path takes over unchanged (SmartSocket, tunnel fallback, old peers).
// Pasting an IP address still works exactly as before.
//
// Registry: GET <url>?action=host&port=P&proto=cc -> {"code","secret","ttl"}; the same call plus &code=&secret= is the
// keep-alive; action=end&code=&secret= drops it; action=resolve&code=C -> {"proto":"cc","addr":"ip:port"}.
// The registry URL is HOST_CODE_DEFAULT_URL unless codes_server.txt (beside relay_list.txt) names another one.
//
// The resolved address is never shown or logged: the UI shows "code NNNN" and the log redacts it (Logger::redact).

#include "HttpGet.hpp"
#include "Timer.hpp"
#include "IpAddrPort.hpp"

#include <string>
#include <memory>


#define HOST_CODE_LIST          "codes_server.txt"
#define HOST_CODE_DEFAULT_URL   "http://watch.meltyblood.org/hc"
#define HOST_CODE_KEEPALIVE_MS  ( 20000 )
#define HOST_CODE_HTTP_TIMEOUT  ( 3000 )


namespace HostCode
{

// The ONE code-format validator: returns the normalized code, or "" if the text is not a code.
// A code is 4 digits, 1000-9999 (the same codes PovertyCaster uses; one registry allocates both).
// An optional "cccaster:" prefix is allowed. Changing the format means changing only this function.
std::string normalize ( const std::string& text );

inline bool isCode ( const std::string& text ) { return ! normalize ( text ).empty(); }

// "1.2.3.4:46318" -> "<code 4821>:46318" for display / logs (the port is not secret, the address is)
std::string maskAddress ( const std::string& addrPort, const std::string& code );

// A parsed registry reply (flat JSON object of strings and numbers)
struct Reply
{
    std::string code, secret, proto, addr, room, stream, error;
    int ttl = 0;
    uint32_t retryAfterMs = 0;
};

// Parse a registry reply; false if it is not a JSON object
bool parseReply ( const std::string& json, Reply& reply );

// The registry URL from the contents of codes_server.txt (first line that is not blank or '#'), or the default
std::string registryUrlFrom ( const std::string& fileContents );

// The registry URL in use (reads codes_server.txt from the working directory, like relay_list.txt)
std::string registryUrl();

// Percent-encode a query value
std::string urlEncode ( const std::string& str );

// Resolve a code to an address, blocking (runs its own event loop; call it before a session starts, never inside one).
// On success sets address, registers the address with Logger::redact and remembers the code for display.
bool resolve ( const std::string& code, IpAddrPort& address, std::string& error );

// The code the current session was joined with, or "" if it was joined by address
const std::string& joinedCode();
void clearJoinedCode();

// What the UI may show for the address we connect to: "code NNNN" when joined by code, else the address
std::string displayAddress ( const IpAddrPort& address );

} // namespace HostCode


// Keeps a host's code alive while hosting. Lives in the host's MainApp (its event loop drives the requests).
class HostCodeKeeper
    : private HttpGet::Owner
    , private Timer::Owner
{
public:

    struct Owner
    {
        // A code was assigned (or replaced, if the registry forgot the old one)
        virtual void hostCodeReady ( HostCodeKeeper *keeper, const std::string& code ) = 0;

        // The registry cannot be reached or refused us: fall back to showing the IP address
        virtual void hostCodeFailed ( HostCodeKeeper *keeper ) = 0;
    };

    Owner *owner = 0;

    HostCodeKeeper ( Owner *owner ) : owner ( owner ) {}

    ~HostCodeKeeper() { stop(); }

    // Register the hosting port and keep the code alive
    void start ( uint16_t port );

    // End the code now (best effort, short blocking request), stop the keep-alive
    void stop();

    const std::string& code() const { return _code; }

private:

    std::string _url, _code, _secret;

    uint16_t _port = 0;

    bool _stopped = true;

    std::shared_ptr<HttpGet> _httpGet;

    TimerPtr _timer;

    void request ( const std::string& query );

    void httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes ) override;

    void httpFailed ( HttpGet *httpGet ) override;

    void httpProgress ( HttpGet *httpGet, uint32_t receivedBytes, uint32_t totalBytes ) override {}

    void timerExpired ( Timer *timer ) override;
};
