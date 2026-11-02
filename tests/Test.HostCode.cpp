#ifndef RELEASE

#include "Test.hpp"
#include "HostCode.hpp"
#include "IpAddrPort.hpp"
#include "Logger.hpp"
#include "Exceptions.hpp"

#include <gtest/gtest.h>

using namespace std;


// The one validator: 4 digits; never an address or a 5-digit port
TEST ( HostCode, Parse )
{
    EXPECT_EQ ( "4821", HostCode::normalize ( "4821" ) );
    EXPECT_EQ ( "4821", HostCode::normalize ( "  4821\r\n" ) );
    EXPECT_EQ ( "4821", HostCode::normalize ( "cccaster:4821" ) );
    EXPECT_EQ ( "1000", HostCode::normalize ( "1000" ) );
    EXPECT_EQ ( "9999", HostCode::normalize ( "9999" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "0999" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "48210" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "46318" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "482" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "48 1" ) );
    EXPECT_EQ ( "", HostCode::normalize ( ":4821" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "127.0.0.1:4821" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "1.2.3.4" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "cccaster:" ) );

    // only 4 digits: no letters (PovertyCaster uses the same 4-digit codes)
    EXPECT_EQ ( "", HostCode::normalize ( "K7Q" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "48a1" ) );
    EXPECT_EQ ( "", HostCode::normalize ( "234" ) );

    EXPECT_TRUE ( HostCode::isCode ( "4821" ) );
    EXPECT_FALSE ( HostCode::isCode ( "46318" ) );
}

// Addresses are masked, ports kept
TEST ( HostCode, Mask )
{
    EXPECT_EQ ( "<code 4821>:46318", HostCode::maskAddress ( "203.0.113.7:46318", "4821" ) );
    EXPECT_EQ ( "<code 4821>:7000", HostCode::maskAddress ( "[2001:db8::5]:7000", "4821" ) );
    EXPECT_EQ ( "<code 4821>", HostCode::maskAddress ( "2001:db8::5", "4821" ) );
    EXPECT_EQ ( "<code 4821>", HostCode::maskAddress ( "203.0.113.7", "4821" ) );
    EXPECT_EQ ( "<code ?>:1", HostCode::maskAddress ( "a:1", "" ) );
}

// The log never carries a redacted address
TEST ( HostCode, LogRedaction )
{
    Logger logger;
    EXPECT_EQ ( "to 203.0.113.7:46318", logger.redacted ( "to 203.0.113.7:46318" ) );
    logger.redact ( "203.0.113.7", "<code 4821>" );
    logger.redact ( "203.0.113.7", "<other>" ); // the first registration wins
    logger.redact ( "", "x" );                  // ignored
    EXPECT_EQ ( "to <code 4821>:46318, again <code 4821>", logger.redacted ( "to 203.0.113.7:46318, again 203.0.113.7" ) );
    EXPECT_EQ ( "nothing here", logger.redacted ( "nothing here" ) );
}

// Registry replies
TEST ( HostCode, Reply )
{
    HostCode::Reply r;

    ASSERT_TRUE ( HostCode::parseReply ( "{\"code\":\"4821\",\"secret\":\"0f9a\",\"ttl\":60}", r ) );
    EXPECT_EQ ( "4821", r.code );
    EXPECT_EQ ( "0f9a", r.secret );
    EXPECT_EQ ( 60, r.ttl );

    ASSERT_TRUE ( HostCode::parseReply ( "{ \"proto\" : \"cc\", \"addr\" : \"203.0.113.7:46318\" }", r ) );
    EXPECT_EQ ( "cc", r.proto );
    EXPECT_EQ ( "203.0.113.7:46318", r.addr );
    EXPECT_EQ ( "", r.code );

    ASSERT_TRUE ( HostCode::parseReply ( "{\"proto\":\"pc\",\"room\":\"r-9f2c\",\"stream\":\"pc_ab12\"}", r ) );
    EXPECT_EQ ( "pc", r.proto );
    EXPECT_EQ ( "r-9f2c", r.room );
    EXPECT_EQ ( "pc_ab12", r.stream );
    EXPECT_EQ ( "", r.addr );

    ASSERT_TRUE ( HostCode::parseReply ( "{\"error\":\"throttled\",\"retryAfterMs\":4000}", r ) );
    EXPECT_EQ ( "throttled", r.error );
    EXPECT_EQ ( 4000u, r.retryAfterMs );

    ASSERT_TRUE ( HostCode::parseReply ( "{\"error\":\"say \\\"no\\\"\",\"ok\":true}", r ) );
    EXPECT_EQ ( "say \"no\"", r.error );

    ASSERT_TRUE ( HostCode::parseReply ( "{}", r ) );
    EXPECT_FALSE ( HostCode::parseReply ( "", r ) );
    EXPECT_FALSE ( HostCode::parseReply ( "<html>502 Bad Gateway</html>", r ) );
    EXPECT_FALSE ( HostCode::parseReply ( "{\"code\":\"48", r ) );
    EXPECT_FALSE ( HostCode::parseReply ( "{code:1}", r ) );
}

// Registry URL: the constant, or codes_server.txt's first real line
TEST ( HostCode, RegistryUrl )
{
    EXPECT_EQ ( HOST_CODE_DEFAULT_URL, HostCode::registryUrlFrom ( "" ) );
    EXPECT_EQ ( HOST_CODE_DEFAULT_URL, HostCode::registryUrlFrom ( "# comment\n\n" ) );
    EXPECT_EQ ( "http://127.0.0.1:7720/hc", HostCode::registryUrlFrom ( "# local\r\n127.0.0.1:7720/hc\r\n" ) );
    EXPECT_EQ ( "http://example.org/hc", HostCode::registryUrlFrom ( "http://example.org/hc\nhttp://second/hc\n" ) );
    EXPECT_EQ ( "4821", HostCode::urlEncode ( "4821" ) );
    EXPECT_EQ ( "a%20b%26c%3D", HostCode::urlEncode ( "a b&c=" ) );
}

// Typed addresses still parse exactly as before; nothing an old client could type turns into a code except a bare 4-digit
// number in the join box (to host on a 4-digit port there, type :<port>; the command line keeps <port> = host)
TEST ( HostCode, OldInputStillWorks )
{
    IpAddrPort a ( "203.0.113.7:46318" );
    EXPECT_EQ ( "203.0.113.7", a.addr );
    EXPECT_EQ ( 46318, a.port );
    EXPECT_FALSE ( HostCode::isCode ( "203.0.113.7:46318" ) );

    IpAddrPort h ( "46318" );
    EXPECT_TRUE ( h.addr.empty() );
    EXPECT_EQ ( 46318, h.port );

    IpAddrPort h4 ( ":4821" );
    EXPECT_TRUE ( h4.addr.empty() );
    EXPECT_EQ ( 4821, h4.port );
    EXPECT_FALSE ( HostCode::isCode ( ":4821" ) );
    EXPECT_EQ ( ":4821", h4.str() ); // what the join box shows again for a 4-digit host port

    IpAddrPort n ( "melty.example.org:3939" );
    EXPECT_EQ ( "melty.example.org", n.addr );
    EXPECT_FALSE ( HostCode::isCode ( "melty.example.org:3939" ) );

    EXPECT_THROW ( IpAddrPort ( "203.0.113.7:" ), Exception );

    // a session joined by address shows the address; by code, only the code
    HostCode::clearJoinedCode();
    EXPECT_EQ ( "203.0.113.7:46318", HostCode::displayAddress ( a ) );
}

#endif // NOT RELEASE
