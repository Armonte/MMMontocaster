#include "Main.hpp"
#include "MainUi.hpp"
#include "Pinger.hpp"
#include "ExternalIpAddress.hpp"
#include "SmartSocket.hpp"
#include "UdpSocket.hpp"
#include "Constants.hpp"
#include "Exceptions.hpp"
#include "Algorithms.hpp"
#include "CharacterSelect.hpp"
#include "SpectatorManager.hpp"
#include "NetplayStates.hpp"

#include <windows.h>
#include <ws2tcpip.h>

#include <winsock2.h>
#include <wininet.h>


void ___log(const char* msg)
{
	const char* ipAddress = "127.0.0.1";
	unsigned short port = 17474;
	int msgLen = strlen(msg);
	const char* message = msg;
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) 
	{
		return;
	}
	SOCKET sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sendSocket == INVALID_SOCKET) 
	{
		WSACleanup();
		return;
	}
	sockaddr_in destAddr;
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ipAddress, &destAddr.sin_addr) <= 0) 
	{
		closesocket(sendSocket);
		WSACleanup();
		return;
	}
	int sendResult = sendto(sendSocket, message, strlen(message), 0, (sockaddr*)&destAddr, sizeof(destAddr));
	if (sendResult == SOCKET_ERROR) 
	{
		closesocket(sendSocket);
		WSACleanup();
		return;
	}
	closesocket(sendSocket);
	WSACleanup();
}

void log(const char* format, ...) {
	static char buffer[1024]; // no more random char buffers everywhere.
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 1024, format, args);
	___log(buffer);
	va_end(args);
}


using namespace std;

#define PING_INTERVAL ( 1000/60 )

#define NUM_PINGS ( 10 )


extern vector<option::Option> opt;

extern MainUi ui;

extern string lastError;

static Mutex uiMutex;

static CondVar uiCondVar;


// static string getClipboard()
// {
//     const char *buffer = "";

//     if ( OpenClipboard ( 0 ) )
//     {
//         HANDLE hData = GetClipboardData ( CF_TEXT );
//         buffer = ( const char * ) GlobalLock ( hData );
//         if ( buffer == 0 )
//             buffer = "";
//         GlobalUnlock ( hData );
//         CloseClipboard();
//     }
//     else
//     {
//         LOG ( "OpenClipboard failed: %s", WinException::getLastError() );
//     }

//     return string ( buffer );
// }

static void setClipboard ( const string& str )
{
    if ( OpenClipboard ( 0 ) )
    {
        HGLOBAL clipbuffer = GlobalAlloc ( GMEM_DDESHARE, str.size() + 1 );
        char *buffer = ( char * ) GlobalLock ( clipbuffer );
        strcpy ( buffer, LPCSTR ( str.c_str() ) );
        GlobalUnlock ( clipbuffer );
        EmptyClipboard();
        SetClipboardData ( CF_TEXT, clipbuffer );
        CloseClipboard();
    }
    else
    {
        LOG ( "OpenClipboard failed: %s", WinException::getLastError() );
    }
}

struct MainApp
        : public Main
        , public Pinger::Owner
        , public ExternalIpAddress::Owner
        , public KeyboardManager::Owner
        , public Thread
        , public SpectatorManager
{
    IpAddrPort originalAddress;

    ExternalIpAddress externalIpAddress;

    InitialConfig initialConfig;

    bool isInitialConfigReady = false;

    SpectateConfig spectateConfig;

    NetplayConfig netplayConfig;

    Pinger pinger;

    PingStats pingStats;

    bool isBroadcastPortReady = false;

    bool isFinalConfigReady = false;

    bool isWaitingForUser = false;

    bool userConfirmed = false;
    bool isF1Connection = false;  // Track if this is an F1 connection to bypass UI prompts

    SocketPtr uiSendSocket, uiRecvSocket;

    bool isQueueing = false;

    vector<MsgPtr> msgQueue;

    bool isDummyReady = false;

    TimerPtr startTimer;

    IndexedFrame dummyFrame = {{ 0, 0 }};

    bool delayChanged = false;

    bool rollbackDelayChanged = false;

    bool rollbackChanged = false;

    bool startedEventManager = false;

    bool kbCancel = false;

    bool connected = true;

    /* Connect protocol

        1 - Connect / accept ctrlSocket

        2 - Both send and recv VersionConfig

        3 - Both send and recv InitialConfig, then repeat to update names

        4 - Connect / accept dataSocket

        5 - Host pings, then sends PingStats

        6 - Client waits for PingStats, then pings, then sends PingStats

        7 - Both merge PingStats and wait for user confirmation

        8 - Host sends NetplayConfig and waits for ConfirmConfig before starting

        9 - Client confirms NetplayConfig and sends ConfirmConfig before starting

       10 - Reconnect dataSocket in-game, and also don't need ctrlSocket for host-client communications

    */

    void run() override
    {
        try
        {
            if ( clientMode.isNetplay() )
            {
                startNetplay();
            }
            else if ( clientMode.isSpectate() )
            {
                startSpectate();
            }
            else if ( clientMode.isLocal() )
            {
                startLocal();
            }
            else
            {
                ASSERT_IMPOSSIBLE;
            }
        }
        catch ( const Exception& exc )
        {
            lastError = exc.user;
        }
#ifdef NDEBUG
        catch ( const std::exception& exc )
        {
            lastError = format ( "Error: %s", exc.what() );
        }
        catch ( ... )
        {
            lastError = "Unknown error!";
        }
#endif // NDEBUG

        stop();
    }

    void startNetplay()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        _.doDeinit = !EventManager::get().isRunning();

        if ( clientMode.isHost() )
        {
            if ( !ui.isServer() ) {
                externalIpAddress.start();
            }
            updateStatusMessage();
        }
        else
        {
            if ( options[Options::Tunnel] ) {
                if ( ui.isServer() ) {
                    ui.display ( format ( "Trying connection (UDP tunnel)" ) );
                } else {
                    ui.display ( format ( "Trying %s (UDP tunnel)", address ) );
                }
            } else {
                if ( ui.isServer() ) {
                    ui.display ( format ( "Trying connection" ) );
                } else {
                    ui.display ( format ( "Trying %s", address ) );
                }
            }
        }

        if ( clientMode.isHost() )
        {
            serverCtrlSocket = SmartSocket::listenTCP ( this, address.port );
            address.port = serverCtrlSocket->address.port; // Update port in case it was initially 0
            address.invalidate();

            LOG ( "serverCtrlSocket=%08x", serverCtrlSocket.get() );
        }
        else
        {
            // UDP debug for normal client connection comparison
            static SOCKET udpSock = INVALID_SOCKET;
            if (udpSock == INVALID_SOCKET) {
                udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
            }
            if (udpSock != INVALID_SOCKET) {
                struct sockaddr_in debugAddr;
                debugAddr.sin_family = AF_INET;
                debugAddr.sin_port = htons(17474);
                debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char debugMsg[256];
                sprintf(debugMsg, "```NORMAL_CLIENT: Starting normal CCCaster client connection to %s:%d", 
                       address.addr.c_str(), address.port);
                sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            }
            
            ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );
            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT );
        }

        if ( EventManager::get().isRunning() ) {
            while ( connected ) {
                Sleep( 100 );
            }
        } else {
            startedEventManager = true;
            EventManager::get().start();
        }
    }

    void startSpectate()
    {
        AutoManager _ ( this, MainUi::getConsoleWindow(), { VK_ESCAPE } );

        if ( ui.isServer() ) {
            ui.display ( format ( "Trying connection" ) );
        } else {
            ui.display ( format ( "Trying %s", address ) );
        }

        ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );
        LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

        startedEventManager = true;
        EventManager::get().start();
    }

    void startLocal()
    {
        AutoManager _;

        if ( clientMode.isBroadcast() )
            externalIpAddress.start();

        // Open the game immediately
        startGame();

        startedEventManager = true;
        EventManager::get().start();
    }

    void stop ( const string& error = "" )
    {
        if ( ! error.empty() )
            lastError = error;

        LOG( "stop@mainapp " );
        LOG( kbCancel );
        if ( startedEventManager ) {
            LOG( "stopping event manager" );
            EventManager::get().stop();
        }

        ctrlSocket.reset();
        dataSocket.reset();
        serverDataSocket.reset();
        serverCtrlSocket.reset();
        stopTimer.reset();
        startTimer.reset();
        connected = false;
        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    void forwardMsgQueue()
    {
        if ( !procMan.isConnected() || msgQueue.empty() )
            return;

        for ( const MsgPtr& msg : msgQueue )
            procMan.ipcSend ( msg );

        msgQueue.clear();
    }

    void gotVersionConfig ( Socket *socket, const VersionConfig& versionConfig )
    {
        // UDP debug for F1 connection protocol
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[256];
            sprintf(debugMsg, "```MAINAPP: gotVersionConfig from host - clientMode=%d (F1=%s)", 
                   (int)clientMode.value, (clientMode.value == ClientMode::Client) ? "CLIENT" : "OTHER");
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }
        
        const Version RemoteVersion = versionConfig.version;

        LOG ( "LocalVersion='%s'; revision='%s'; buildTime='%s'",
              LocalVersion, LocalVersion.revision, LocalVersion.buildTime );

        LOG ( "RemoteVersion='%s'; revision='%s'; buildTime='%s'",
              RemoteVersion, RemoteVersion.revision, RemoteVersion.buildTime );

        LOG ( "VersionConfig: mode=%s; flags={ %s }", versionConfig.mode, versionConfig.mode.flagString() );

        if ( ! LocalVersion.isSimilar ( RemoteVersion, 1 + options[Options::StrictVersion] ) )
        {
            string local = LocalVersion.code;
            string remote = RemoteVersion.code;

            if ( options[Options::StrictVersion] >= 2 )
            {
                local += " " + LocalVersion.revision;
                remote += " " + RemoteVersion.revision;
            }

            if ( options[Options::StrictVersion] >= 3 )
            {
                local += " " + LocalVersion.buildTime;
                remote += " " + RemoteVersion.buildTime;
            }

            if ( clientMode.isHost() )
                socket->send ( new ErrorMessage ( "Incompatible host version: " + local ) );
            else
                stop ( "Incompatible host version: " + remote );
            return;
        }

        // Switch to spectate mode if the game is already started
        if ( clientMode.isClient() && versionConfig.mode.isGameStarted() ) {
            clientMode.value = ClientMode::SpectateNetplay;
        }

        // Update spectate type
        if ( clientMode.isSpectate() && versionConfig.mode.isBroadcast() ) {
            clientMode.value = ClientMode::SpectateBroadcast;
        }

        if ( clientMode.isSpectate() )
        {
            if ( ! versionConfig.mode.isGameStarted() )
                stop ( "Not in a game yet, cannot spectate!" );

            // Wait for SpectateConfig
            return;
        }

        if ( clientMode.isHost() )
        {
            if ( versionConfig.mode.isSpectate() )
            {
                socket->send ( new ErrorMessage ( "Not in a game yet, cannot spectate!" ) );
                return;
            }

            ctrlSocket = popPendingSocket ( socket );
            LOG ( "ctrlSocket=%08x", ctrlSocket.get() );

            if ( ! ctrlSocket.get() )
                return;

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            try { serverDataSocket = SmartSocket::listenUDP ( this, address.port ); }
            catch ( ... ) { serverDataSocket = SmartSocket::listenUDP ( this, 0 ); }

            initialConfig.dataPort = serverDataSocket->address.port;

            LOG ( "serverDataSocket=%08x", serverDataSocket.get() );
        }

        // FIX: Populate InitialConfig with proper values for F1 connection
        initialConfig.mode = clientMode;  // Set our client mode (should be Client)
        initialConfig.localName = "F1Player";  // Set a player name so host can see us
        initialConfig.winCount = 2;  // Default win count
        
        // Set dataPort to 0 initially - host will tell us the correct port
        initialConfig.dataPort = 0;
        initialConfig.remoteName = "";  // Will be filled by host
        
        // DEBUG: Log what InitialConfig we're sending to the host
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[512];
            sprintf(debugMsg, "```MAINAPP: Sending FIXED InitialConfig - localName='%s' remoteName='%s' dataPort=%d winCount=%d mode=%d", 
                   initialConfig.localName.c_str(), initialConfig.remoteName.c_str(), 
                   initialConfig.dataPort, initialConfig.winCount, (int)initialConfig.mode.value);
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }

        initialConfig.invalidate();
        ctrlSocket->send ( initialConfig );
    }

    void gotInitialConfig ( const InitialConfig& initialConfig )
    {
        // DEBUG: Log what InitialConfig we received from the host
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[512];
            sprintf(debugMsg, "```MAINAPP: Received InitialConfig from host - localName='%s' remoteName='%s' dataPort=%d winCount=%d mode=%d (isReady=%s)", 
                   initialConfig.localName.c_str(), initialConfig.remoteName.c_str(), 
                   initialConfig.dataPort, initialConfig.winCount, (int)initialConfig.mode.value,
                   isInitialConfigReady ? "TRUE" : "FALSE");
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }
        
        if ( ! isInitialConfigReady )
        {
            isInitialConfigReady = true;

            this->initialConfig.mode.flags |= initialConfig.mode.flags;

            this->initialConfig.remoteName = initialConfig.localName;

            if ( this->initialConfig.remoteName.empty() ) {
                if ( ui.isServer() ) {
                    this->initialConfig.remoteName = "Anonymous";
                } else {
                    this->initialConfig.remoteName = ctrlSocket->address.addr;
                }
            }

            this->initialConfig.invalidate();

            ctrlSocket->send ( this->initialConfig );
            return;
        }

        // Update our real localName when we receive the 2nd InitialConfig
        this->initialConfig.localName = initialConfig.remoteName;

        if ( clientMode.isClient() )
        {
            this->initialConfig.mode.flags = initialConfig.mode.flags;
            this->initialConfig.dataPort = initialConfig.dataPort;
            this->initialConfig.winCount = initialConfig.winCount;

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            dataSocket = SmartSocket::connectUDP ( this, { address.addr, this->initialConfig.dataPort },
                                                   ctrlSocket->getAsSmart().isTunnel() );
            LOG ( "dataSocket=%08x", dataSocket.get() );

            ui.display (
                "Connecting to " + this->initialConfig.remoteName
                + "\n\n" + ( this->initialConfig.mode.isTraining() ? "Training" : "Versus" ) + " mode"
                + "\n\nCalculating delay..." );
        }

        LOG ( "InitialConfig: mode=%s; flags={ %s }; dataPort=%u; localName='%s'; remoteName='%s'; winCount=%u",
              initialConfig.mode, initialConfig.mode.flagString(),
              initialConfig.dataPort, initialConfig.localName, initialConfig.remoteName, initialConfig.winCount );
    }

    void gotPingStats ( const PingStats& pingStats )
    {
        // UDP debug for pinger activity
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[256];
            sprintf(debugMsg, "```PINGER: gotPingStats - isHost=%s, starting pinger", 
                   clientMode.isHost() ? "TRUE" : "FALSE");
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }
        
        this->pingStats = pingStats;

        if ( clientMode.isHost() )
        {
            mergePingStats();
            checkDelayAndContinue();
        }
        else
        {
            pinger.start();
        }
    }

    void mergePingStats()
    {
        LOG ( "PingStats (local): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pinger.getStats().getMean(), pinger.getStats().getWorst(),
              pinger.getStats().getStdErr(), pinger.getStats().getStdDev(), pinger.getPacketLoss() );

        LOG ( "PingStats (remote): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );

        pingStats.latency.merge ( pinger.getStats() );
        pingStats.packetLoss = ( pingStats.packetLoss + pinger.getPacketLoss() ) / 2;

        LOG ( "PingStats (merged): latency=%.2f ms; worst=%.2f ms; stderr=%.2f ms; stddev=%.2f ms; packetLoss=%d%%",
              pingStats.latency.getMean(), pingStats.latency.getWorst(),
              pingStats.latency.getStdErr(), pingStats.latency.getStdDev(), pingStats.packetLoss );
    }

    void gotSpectateConfig ( const SpectateConfig& spectateConfig )
    {
        if ( ! clientMode.isSpectate() )
        {
            LOG ( "Unexpected 'SpectateConfig'" );
            return;
        }

        LOG ( "SpectateConfig: %s; flags={ %s }; delay=%d; rollback=%d; winCount=%d; hostPlayer=%u; "
              "names={ '%s', '%s' }", spectateConfig.mode, spectateConfig.mode.flagString(), spectateConfig.delay,
              spectateConfig.rollback, spectateConfig.winCount, spectateConfig.hostPlayer,
              spectateConfig.names[0], spectateConfig.names[1] );

        LOG ( "InitialGameState: %s; stage=%u; isTraining=%u; %s vs %s",
              NetplayState ( ( NetplayState::Enum ) spectateConfig.initial.netplayState ),
              spectateConfig.initial.stage, spectateConfig.initial.isTraining,
              spectateConfig.formatPlayer ( 1, getFullCharaName ),
              spectateConfig.formatPlayer ( 2, getFullCharaName ) );

        this->spectateConfig = spectateConfig;

        ui.spectate ( spectateConfig );

        getUserConfirmation();
    }

    void gotNetplayConfig ( const NetplayConfig& netplayConfig )
    {
        if ( ! clientMode.isClient() )
        {
            LOG ( "Unexpected 'NetplayConfig'" );
            return;
        }

        this->netplayConfig.mode.flags = netplayConfig.mode.flags;

        // These are now set independently
        if ( options[Options::SyncTest] )
        {
            // TODO parse these from SyncTest args
            this->netplayConfig.delay = netplayConfig.delay;
            this->netplayConfig.rollback = netplayConfig.rollback;
            this->netplayConfig.rollbackDelay = netplayConfig.rollbackDelay;
        }

        this->netplayConfig.winCount = netplayConfig.winCount;
        this->netplayConfig.hostPlayer = netplayConfig.hostPlayer;
        this->netplayConfig.sessionId = netplayConfig.sessionId;

        isFinalConfigReady = true;
        startGameIfReady();
    }

    void checkDelayAndContinue()
    {
        const int delay = computeDelay ( this->pingStats.latency.getMean() );
        const int maxDelay = ui.getConfig().getInteger ( "maxRealDelay" );

        if ( delay > maxDelay )
        {
            const string error = MainUi::formatStats ( this->pingStats )
                                 + format ( "\n\nNetwork delay greater than limit: %u", maxDelay );

            if ( clientMode.isHost() )
            {
                if ( ctrlSocket && ctrlSocket->isConnected() )
                {
                    ctrlSocket->send ( new ErrorMessage ( error ) );
                    pushPendingSocket ( this, ctrlSocket );
                }
                resetHost();
            }
            else
            {
                lastError = error;
                stop();
            }
            return;
        }

        getUserConfirmation();
    }

    void getUserConfirmation()
    {
        // Disable keyboard hooks for the UI
        KeyboardManager::get().unhook();

        // Auto-confirm any settings if necessary
        if ( options[Options::Dummy] || options[Options::SyncTest] )
        {
            isWaitingForUser = true;
            userConfirmed = true;

            if ( clientMode.isHost() )
            {
                // TODO parse these from SyncTest args
                netplayConfig.delay = computeDelay ( pingStats.latency.getWorst() ) + 1;
                netplayConfig.rollback = 4;
                netplayConfig.rollbackDelay = 0;
                netplayConfig.hostPlayer = 1;
                netplayConfig.sessionId = generateRandomId();
                netplayConfig.invalidate();

                ctrlSocket->send ( netplayConfig );

                gotConfirmConfig();
            }
            else
            {
                gotUserConfirmation();
            }
            return;
        }

        uiRecvSocket = UdpSocket::bind ( this, 0 );
        uiSendSocket = UdpSocket::bind ( 0, { "127.0.0.1", uiRecvSocket->address.port } );
        isWaitingForUser = true;

        // Unblock the thread waiting for user confirmation
        LOCK ( uiMutex );
        uiCondVar.signal();
    }

    bool autoConfirmF1Connection(const InitialConfig& initialConfig, const PingStats& pingStats)
    {
        // UDP debug for F1 auto-confirmation
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[256];
            sprintf(debugMsg, "```F1_AUTO: Auto-confirming connection with defaults");
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            
            // Calculate delay based on ping (same logic as MainUi::connected)
            const int delay = computeDelay(pingStats.latency.getMean());
            const int worst = computeDelay(pingStats.latency.getWorst());
            const int variance = computeDelay(pingStats.latency.getVariance());
            
            // Create NetplayConfig with reasonable defaults (same as MainUi::connected does)
            netplayConfig.delay = worst + 1;  // Same calculation as MainUi::connected
            netplayConfig.rollback = 3;  // Common default value  
            netplayConfig.rollbackDelay = netplayConfig.delay;  // Same delay for rollback
            netplayConfig.winCount = 2;  // Default win count
            netplayConfig.hostPlayer = 1 + (rand() % 2);  // Random player assignment
            
            // IMPORTANT: Force Versus mode for F1 connections (not Training)
            netplayConfig.mode.value = ClientMode::Client;  // We're the client
            netplayConfig.mode.flags = 0;  // No Training flag = Versus mode
            
            netplayConfig.setNames(initialConfig.localName, initialConfig.remoteName);
            
            char debugMsg2[512];
            sprintf(debugMsg2, "```F1_AUTO: Set delay=%d, rollback=%d, hostPlayer=%d", 
                   netplayConfig.delay, netplayConfig.rollback, netplayConfig.hostPlayer);
            sendto(udpSock, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }
        
        return true;  // Always confirm for F1 connections
    }

    void waitForUserConfirmation()
    {
        // This runs a different thread waiting for user confirmation
        LOCK ( uiMutex );
        LOG( "lockUserMutex");
        while ( uiCondVar.wait ( uiMutex, 5000 ) ) {
            if ( ! EventManager::get().isRunning() || !connected )
                return;
        }
        LOG( "unlockUserMutex");

        if ( ! EventManager::get().isRunning() || !connected )
            return;

        switch ( clientMode.value )
        {
            case ClientMode::Host:
            case ClientMode::Client:
                if (isF1Connection) {
                    // F1 connection: Auto-accept with reasonable defaults
                    userConfirmed = autoConfirmF1Connection(initialConfig, pingStats);
                } else {
                    // Normal connection: Show UI prompts
                    userConfirmed = ui.connected ( initialConfig, pingStats );
                }
                break;

            case ClientMode::SpectateNetplay:
                ui.initialConfig.mode.value = ClientMode::SpectateNetplay;
                userConfirmed = ui.confirm ( "Continue?" );
                break;
            case ClientMode::SpectateBroadcast:
                ui.initialConfig.mode.value = ClientMode::SpectateBroadcast;
                userConfirmed = ui.confirm ( "Continue?" );
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }

        if ( clientMode.value == ClientMode::Client )
            ui.sendConnected();
        // Signal the main thread via a UDP packet
        uiSendSocket->send ( NullMsg );
    }

    void gotUserConfirmation()
    {
        uiRecvSocket.reset();
        uiSendSocket.reset();

        if ( !userConfirmed || !ctrlSocket || !ctrlSocket->isConnected() )
        {
            if ( !ctrlSocket || !ctrlSocket->isConnected() )
                lastError = "Disconnected!";

            stop();
            return;
        }

        switch ( clientMode.value )
        {
            case ClientMode::SpectateNetplay:
            case ClientMode::SpectateBroadcast:
                isQueueing = true;

                ctrlSocket->send ( new ConfirmConfig() );
                startGame();
                break;

            case ClientMode::Host:
                // Waiting again
                KeyboardManager::get().keyboardWindow = MainUi::getConsoleWindow();
                KeyboardManager::get().matchedKeys = { VK_ESCAPE };
                KeyboardManager::get().ignoredKeys.clear();
                KeyboardManager::get().hook ( this );

                netplayConfig = ui.getNetplayConfig();
                netplayConfig.sessionId = generateRandomId();
                netplayConfig.invalidate();

                ctrlSocket->send ( netplayConfig );
                startGameIfReady();
                break;

            case ClientMode::Client:
                // Waiting again
                KeyboardManager::get().keyboardWindow = MainUi::getConsoleWindow();
                KeyboardManager::get().matchedKeys = { VK_ESCAPE };
                KeyboardManager::get().ignoredKeys.clear();
                KeyboardManager::get().hook ( this );

                netplayConfig.delay = ui.getNetplayConfig().delay;
                netplayConfig.rollback = ui.getNetplayConfig().rollback;
                netplayConfig.rollbackDelay = ui.getNetplayConfig().rollbackDelay;

                startGameIfReady();
                break;

            default:
                ASSERT_IMPOSSIBLE;
                break;
        }
    }

    void gotConfirmConfig()
    {
        if ( ! userConfirmed )
        {
            LOG ( "Unexpected 'ConfirmConfig'" );
            return;
        }

        isFinalConfigReady = true;
        startGameIfReady();
    }

    void gotDummyMsg ( const MsgPtr& msg )
    {
        ASSERT ( options[Options::Dummy] );
        ASSERT ( isDummyReady == true );
        ASSERT ( msg.get() != 0 );

        switch ( msg->getMsgType() )
        {
            case MsgType::InitialGameState:
                LOG ( "InitialGameState: %s; stage=%u; isTraining=%u; %s vs %s",
                      NetplayState ( ( NetplayState::Enum ) msg->getAs<InitialGameState>().netplayState ),
                      msg->getAs<InitialGameState>().stage, msg->getAs<InitialGameState>().isTraining,
                      msg->getAs<InitialGameState>().formatCharaName ( 1, getFullCharaName ),
                      msg->getAs<InitialGameState>().formatCharaName ( 2, getFullCharaName ) );
                return;

            case MsgType::RngState:
                return;

            case MsgType::PlayerInputs:
            {
                // TODO log dummy inputs to check sync
                PlayerInputs inputs ( msg->getAs<PlayerInputs>().indexedFrame );
                inputs.indexedFrame.parts.frame += netplayConfig.delay * 2;

                for ( uint32_t i = 0; i < inputs.size(); ++i )
                {
                    const uint32_t frame = i + inputs.getStartFrame();
                    inputs.inputs[i] = ( ( frame % 5 ) ? 0 : COMBINE_INPUT ( 0, CC_BUTTON_A | CC_BUTTON_CONFIRM ) );
                }

                dataSocket->send ( inputs );
                return;
            }

            case MsgType::MenuIndex:
                // Dummy mode always chooses the first retry menu option,
                // since the higher option always takes priority, the host effectively takes priority.
                if ( clientMode.isClient() )
                    dataSocket->send ( new MenuIndex ( msg->getAs<MenuIndex>().index, 0 ) );
                return;

            case MsgType::BothInputs:
            {
                static IndexedFrame last = {{ 0, 0 }};

                const BothInputs& both = msg->getAs<BothInputs>();

                if ( both.getIndex() > last.parts.index )
                {
                    for ( uint32_t i = 0; i < both.getStartFrame(); ++i )
                        LOG_TO ( syncLog, "Dummy [%u:%u] Inputs: 0x%04x 0x%04x", both.getIndex(), i, 0, 0 );
                }

                for ( uint32_t i = 0; i < both.size(); ++i )
                {
                    const IndexedFrame current = {{ i + both.getStartFrame(), both.getIndex() }};

                    if ( current.value <= last.value )
                        continue;

                    LOG_TO ( syncLog, "Dummy [%s] Inputs: 0x%04x 0x%04x",
                             current, both.inputs[0][i], both.inputs[1][i] );
                }

                last = both.indexedFrame;
                return;
            }

            case MsgType::ErrorMessage:
                stop ( msg->getAs<ErrorMessage>().error );
                return;

            default:
                break;
        }

        LOG ( "Unexpected '%s'", msg );
    }

    void startGameIfReady()
    {
        if ( !userConfirmed || !isFinalConfigReady )
            return;

        if ( clientMode.isClient() )
            ctrlSocket->send ( new ConfirmConfig() );

        startGame();
    }

    void startGame()
    {
        KeyboardManager::get().unhook();

        if ( clientMode.isLocal() ) {
            options.set ( Options::SessionId, 1, generateRandomId() );
            netplayConfig.setNames ( "localP1", "localP2");
        }
        else if ( clientMode.isSpectate() )
            options.set ( Options::SessionId, 1, spectateConfig.sessionId );
        else
            options.set ( Options::SessionId, 1, netplayConfig.sessionId );

        if ( clientMode.isClient() && ctrlSocket->isSmart() && ctrlSocket->getAsSmart().isTunnel() )
            clientMode.flags |= ClientMode::UdpTunnel;

        if ( clientMode.isNetplay() )
        {
            netplayConfig.mode.value = clientMode.value;
            
            // F1 FIX: Don't overwrite clientMode.flags for F1 connections 
            // initialConfig.mode.flags comes from HOST and will corrupt our CLIENT mode
            if (!isF1Connection) {
                netplayConfig.mode.flags = clientMode.flags = initialConfig.mode.flags;
            } else {
                // F1 connection: keep our client flags, just copy to netplayConfig
                netplayConfig.mode.flags = clientMode.flags;
                LOG("F1: Preserving clientMode flags=%d, not using host flags", (int)clientMode.flags);
            }
            
            netplayConfig.winCount = initialConfig.winCount;
            netplayConfig.setNames ( initialConfig.localName, initialConfig.remoteName );

            LOG ( "NetplayConfig: %s; flags={ %s }; delay=%d; rollback=%d; rollbackDelay=%d; winCount=%d; "
                  "hostPlayer=%d; names={ '%s', '%s' }", netplayConfig.mode, netplayConfig.mode.flagString(),
                  netplayConfig.delay, netplayConfig.rollback, netplayConfig.rollbackDelay, netplayConfig.winCount,
                  netplayConfig.hostPlayer, netplayConfig.names[0], netplayConfig.names[1] );
        }

        if ( clientMode.isSpectate() )
            clientMode.flags = spectateConfig.mode.flags;

        LOG ( "SessionId '%s'", options.arg ( Options::SessionId ) );

        if ( options[Options::Dummy] )
        {
            ASSERT ( clientMode.value == ClientMode::Client || clientMode.isSpectate() == true );

            ui.display ( format ( "Dummy is ready%s", clientMode.isTraining() ? " (training)" : "" ),
                         false ); // Don't replace last message

            isDummyReady = true;

            // We need to send an IpAddrPort message to indicate our serverCtrlSocket address, here it is a fake
            if ( ctrlSocket && ctrlSocket->isConnected() )
                ctrlSocket->send ( NullAddress );

            // Only connect the dataSocket if isClient
            if ( clientMode.isClient() )
            {
                dataSocket = SmartSocket::connectUDP ( this, address, ctrlSocket->getAsSmart().isTunnel() );
                LOG ( "dataSocket=%08x", dataSocket.get() );
            }

            stopTimer.reset ( new Timer ( this ) );
            stopTimer->start ( DEFAULT_PENDING_TIMEOUT * 2 );

            syncLog.sessionId = ( clientMode.isSpectate() ? spectateConfig.sessionId : netplayConfig.sessionId );

            if ( options[Options::PidLog] )
                syncLog.initialize ( ProcessManager::appDir + SYNC_LOG_FILE, PID_IN_FILENAME );
            else
                syncLog.initialize ( ProcessManager::appDir + SYNC_LOG_FILE, 0 );
            syncLog.logVersion();
            return;
        }

        ui.display ( format ( "Starting %s mode...", getGameModeString() ),
                     clientMode.isNetplay() ); // Only replace last message if netplaying

        // Start game (and disconnect sockets) after a small delay since the final configs are still in flight
        startTimer.reset ( new Timer ( this ) );
        startTimer->start ( 1000 );
    }

    // Pinger callbacks
    void pingerSendPing ( Pinger *pinger, const MsgPtr& ping ) override
    {
        if ( !dataSocket || !dataSocket->isConnected() )
        {
            stop ( "Disconnected!" );
            return;
        }

        ASSERT ( pinger == &this->pinger );

        dataSocket->send ( ping );
    }

    void pingerCompleted ( Pinger *pinger, const Statistics& stats, uint8_t packetLoss ) override
    {
        ASSERT ( pinger == &this->pinger );

        ctrlSocket->send ( new PingStats ( stats, packetLoss ) );

        if ( clientMode.isClient() )
        {
            mergePingStats();
            checkDelayAndContinue();
        }
    }

    // Socket callbacks
    void socketAccepted ( Socket *serverSocket ) override
    {
        LOG ( "socketAccepted ( %08x )", serverSocket );

        if ( serverSocket == serverCtrlSocket.get() )
        {
            LOG ( "serverCtrlSocket->accept ( this )" );

            SocketPtr newSocket = serverCtrlSocket->accept ( this );

            LOG ( "newSocket=%08x", newSocket.get() );

            ASSERT ( newSocket != 0 );
            ASSERT ( newSocket->isConnected() == true );

            newSocket->send ( new VersionConfig ( clientMode ) );

            pushPendingSocket ( this, newSocket );
        }
        else if ( serverSocket == serverDataSocket.get() && ctrlSocket && ctrlSocket->isConnected() && !dataSocket )
        {
            LOG ( "serverDataSocket->accept ( this )" );

            dataSocket = serverDataSocket->accept ( this );
            LOG ( "dataSocket=%08x", dataSocket.get() );

            ASSERT ( dataSocket != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            pinger.start();
        }
        else
        {
            LOG ( "Unexpected socketAccepted from serverSocket=%08x", serverSocket );
            serverSocket->accept ( 0 ).reset();
        }
    }

    void socketConnected ( Socket *socket ) override
    {
        LOG ( "socketConnected ( %08x )", socket );
        
        // UDP debug for F1 connection
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[256];
            sprintf(debugMsg, "```MAINAPP: socketConnected callback - socket=%08x", (unsigned int)socket);
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }

        if ( socket == ctrlSocket.get() )
        {
            LOG ( "ctrlSocket connected!" );
            
            if (udpSock != INVALID_SOCKET) {
                struct sockaddr_in debugAddr;
                debugAddr.sin_family = AF_INET;
                debugAddr.sin_port = htons(17474);
                debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char debugMsg[256];
                strcpy(debugMsg, "```MAINAPP: ctrlSocket CONNECTED! - Sending VersionConfig");
                sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            }

            ASSERT ( ctrlSocket.get() != 0 );
            ASSERT ( ctrlSocket->isConnected() == true );

            ctrlSocket->send ( new VersionConfig ( clientMode ) );
            
            if (udpSock != INVALID_SOCKET) {
                struct sockaddr_in debugAddr;
                debugAddr.sin_family = AF_INET;
                debugAddr.sin_port = htons(17474);
                debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char debugMsg[256];
                strcpy(debugMsg, "```MAINAPP: VersionConfig sent - handshake initiated");
                sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            }
        }
        else if ( socket == dataSocket.get() )
        {
            LOG ( "dataSocket connected!" );

            ASSERT ( dataSocket.get() != 0 );
            ASSERT ( dataSocket->isConnected() == true );

            stopTimer.reset();
        }
        else
        {
            ASSERT_IMPOSSIBLE;
        }
    }

    void socketDisconnected ( Socket *socket ) override
    {
        LOG ( "socketDisconnected ( %08x )", socket );

        if ( socket == ctrlSocket.get() || socket == dataSocket.get() )
        {
            if ( isDummyReady && stopTimer )
            {
                dataSocket = SmartSocket::connectUDP ( this, address );
                LOG ( "dataSocket=%08x", dataSocket.get() );
                return;
            }

            LOG ( "%s disconnected!", ( socket == ctrlSocket.get() ? "ctrlSocket" : "dataSocket" ) );

            // TODO auto reconnect to original host address

            if ( socket == ctrlSocket.get() && clientMode.isSpectate() )
            {
                forwardMsgQueue();
                procMan.ipcSend ( new ErrorMessage ( "Disconnected!" ) );
                return;
            }

            if ( clientMode.isHost() && !isWaitingForUser )
            {
                resetHost();
                return;
            }

            if ( ! ( userConfirmed && isFinalConfigReady ) || isDummyReady )
            {
                if ( lastError.empty() )
                    lastError = ( isInitialConfigReady ? "Disconnected!" : "Timed out!" );

                stop();
            }
            return;
        }

        popPendingSocket ( socket );
    }

    void socketRead ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address ) override
    {
        LOG ( "socketRead ( %08x, %s, %s )", socket, msg, address );
        
        // UDP debug for F1 connection protocol
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET && socket == ctrlSocket.get()) {
            struct sockaddr_in debugAddr;
            debugAddr.sin_family = AF_INET;
            debugAddr.sin_port = htons(17474);
            debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            char debugMsg[256];
            if (msg.get()) {
                sprintf(debugMsg, "```MAINAPP: socketRead from HOST - msgType=%d", (int)msg->getMsgType());
            } else {
                strcpy(debugMsg, "```MAINAPP: socketRead from HOST - NULL message");
            }
            sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
        }

        if ( socket == uiRecvSocket.get() && !msg.get() )
        {
            gotUserConfirmation();
            return;
        }

        if ( ! msg.get() )
            return;

        stopTimer.reset();

        if ( msg->getMsgType() == MsgType::IpAddrPort && socket == ctrlSocket.get() )
        {
            this->address = msg->getAs<IpAddrPort>();
            ctrlSocket = SmartSocket::connectTCP ( this, this->address, options[Options::Tunnel] );
            return;
        }
        else if ( msg->getMsgType() == MsgType::VersionConfig
                  && ( ( clientMode.isHost() && !ctrlSocket ) || clientMode.isClient() ) )
        {
            gotVersionConfig ( socket, msg->getAs<VersionConfig>() );
            return;
        }
        else if ( isDummyReady )
        {
            gotDummyMsg ( msg );
            return;
        }
        else if ( ctrlSocket.get() != 0 )
        {
            if ( isQueueing )
            {
                msgQueue.push_back ( msg );
                forwardMsgQueue();
                return;
            }

            switch ( msg->getMsgType() )
            {
                case MsgType::SpectateConfig:
                    gotSpectateConfig ( msg->getAs<SpectateConfig>() );
                    return;

                case MsgType::InitialConfig:
                    gotInitialConfig ( msg->getAs<InitialConfig>() );
                    return;

                case MsgType::PingStats:
                    gotPingStats ( msg->getAs<PingStats>() );
                    return;

                case MsgType::NetplayConfig:
                    gotNetplayConfig ( msg->getAs<NetplayConfig>() );
                    return;

                case MsgType::ConfirmConfig:
                    gotConfirmConfig();
                    return;

                case MsgType::ErrorMessage:
                    stop ( lastError = msg->getAs<ErrorMessage>().error );
                    return;

                case MsgType::Ping:
                    pinger.gotPong ( msg );
                    return;

                default:
                    break;
            }
        }

        if ( clientMode.isHost() && msg->getMsgType() == MsgType::VersionConfig )
        {
            if ( msg->getAs<VersionConfig>().mode.isSpectate() )
                socket->send ( new ErrorMessage ( "Not in a game yet, cannot spectate!" ) );
            else
                socket->send ( new ErrorMessage ( "Another client is currently connecting!" ) );
        }

        LOG ( "Unexpected '%s' from socket=%08x", msg, socket );
    }

    void smartSocketSwitchedToUDP ( SmartSocket *smartSocket ) override
    {
        if ( smartSocket != ctrlSocket.get() )
            return;

        if ( ui.isServer() ) {
            ui.display ( format ( "Trying connection (UDP tunnel)" ) );
        } else {
            ui.display ( format ( "Trying %s (UDP tunnel)", address ) );
        }
    }

    // ProcessManager callbacks
    void ipcConnected() override
    {
        ASSERT ( clientMode != ClientMode::Unknown );

        // F1 FIX: Skip normal IPC initialization for F1 connections
        // F1 connections handle IPC messages manually after setting up clientMode correctly
        if (isF1Connection) {
            // UDP debug logging for F1 callback skip
            static SOCKET udpSock = INVALID_SOCKET;
            if (udpSock == INVALID_SOCKET) {
                udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
            }
            if (udpSock != INVALID_SOCKET) {
                struct sockaddr_in debugAddr;
                debugAddr.sin_family = AF_INET;
                debugAddr.sin_port = htons(17474);
                debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char debugMsg[256];
                sprintf(debugMsg, "```F1_IPC_CALLBACK: ipcConnected() SKIPPED for F1 - clientMode=%d", (int)clientMode.value);
                sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            }
            
            LOG("F1: ipcConnected() called but skipping - F1 will handle IPC messages manually");
            return;
        }
        
        // UDP debug logging for normal callback
        {
            static SOCKET udpSock = INVALID_SOCKET;
            if (udpSock == INVALID_SOCKET) {
                udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
            }
            if (udpSock != INVALID_SOCKET) {
                struct sockaddr_in debugAddr;
                debugAddr.sin_family = AF_INET;
                debugAddr.sin_port = htons(17474);
                debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                char debugMsg[256];
                sprintf(debugMsg, "```NORMAL_IPC_CALLBACK: ipcConnected() sending clientMode=%d", (int)clientMode.value);
                sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
            }
        }

        procMan.ipcSend ( options );
        procMan.ipcSend ( ControllerManager::get().getMappings() );
        procMan.ipcSend ( clientMode );
        procMan.ipcSend ( new IpAddrPort ( address.getAddrInfo()->ai_addr ) );

        if ( clientMode.isSpectate() )
        {
            procMan.ipcSend ( spectateConfig );
            forwardMsgQueue();
            return;
        }

        ASSERT ( netplayConfig.delay != 0xFF );

        netplayConfig.invalidate();

        procMan.ipcSend ( netplayConfig );

        ui.display ( format ( "Started %s mode", getGameModeString() ) );
    }

    void ipcDisconnected() override
    {
        if ( lastError.empty() )
            lastError = "Game closed!";
        stop();
    }

    void ipcRead ( const MsgPtr& msg ) override
    {
        if ( ! msg.get() )
            return;

        // UDP log for debugging all IPC messages
        static SOCKET udpSock = INVALID_SOCKET;
        if (udpSock == INVALID_SOCKET) {
            udpSock = socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (udpSock != INVALID_SOCKET) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(17474);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            std::string msgStr = "```MAINAPP_IPC: Received message type " + std::to_string((int)msg->getMsgType());
            sendto(udpSock, msgStr.c_str(), msgStr.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
        }

        switch ( msg->getMsgType() )
        {
            case MsgType::ErrorMessage:
                stop ( msg->getAs<ErrorMessage>().error );
                return;

            case MsgType::NetplayConfig:
                netplayConfig = msg->getAs<NetplayConfig>();
                isBroadcastPortReady = true;
                updateStatusMessage();
                return;

            case MsgType::IpAddrPort:
                if ( ctrlSocket && ctrlSocket->isConnected() )
                {
                    ctrlSocket->send ( msg );
                }
                else
                {
                    // F1 connection request - IMMEDIATELY mark as F1 to prevent race condition
                    isF1Connection = true;
                    
                    // F1 connection request - ACTUALLY connect now
                    IpAddrPort targetHost = msg->getAs<IpAddrPort>();
                    LOG ( "F1: Received connection request to %s:%d", targetHost.addr.c_str(), targetHost.port );
                    LOG ( "F1: Current clientMode.value=%d before setting to Client", (int)clientMode.value );
                    
                    // UDP logging for debugging (visible in release builds)
                    static SOCKET udpSock = INVALID_SOCKET;
                    if (udpSock == INVALID_SOCKET) {
                        udpSock = socket(AF_INET, SOCK_DGRAM, 0);
                    }
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(17474);
                        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        std::string msg = "```MAINAPP: F1 connection - isF1Connection set to TRUE before TCP connection";
                        sendto(udpSock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
                        
                        std::string msg2 = "```MAINAPP: F1 connection request received for " + targetHost.addr + ":" + std::to_string(targetHost.port);
                        sendto(udpSock, msg2.c_str(), msg2.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
                    }
                    
                    // Set client mode and target address (like normal UI does)
                    clientMode.value = ClientMode::Client;
                    clientMode.flags = 0;  // Clear any existing flags
                    address = targetHost;
                    LOG ( "F1: Set clientMode.value=%d (Client), flags=%d", (int)clientMode.value, (int)clientMode.flags );
                    
                    // CRITICAL FIX: Initialize pinger for F1 connections (normally done in constructor)
                    pinger.owner = this;
                    pinger.pingInterval = PING_INTERVAL;
                    pinger.numPings = NUM_PINGS;
                    
                    // Manual TCP connection (startNetplay() expects to control MBAA.exe launch)
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr;
                        debugAddr.sin_family = AF_INET;
                        debugAddr.sin_port = htons(17474);
                        debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char msg[256];
                        strcpy(msg, "```MAINAPP: F1 using manual connection - startNetplay() expects to launch MBAA");
                        sendto(udpSock, msg, strlen(msg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                    }
                    
                    // Create TCP connection exactly like startNetplay() does for clients
                    ctrlSocket = SmartSocket::connectTCP ( this, address, options[Options::Tunnel] );
                    LOG ( "ctrlSocket=%08x", ctrlSocket.get() );
                    
                    // Set up timeout exactly like startNetplay() does
                    stopTimer.reset ( new Timer ( this ) );
                    stopTimer->start ( DEFAULT_PENDING_TIMEOUT );
                    
                    // Update UI exactly like startNetplay() does
                    if ( options[Options::Tunnel] ) {
                        ui.display ( format ( "Trying %s (UDP tunnel)", address ) );
                    } else {
                        ui.display ( format ( "Trying %s", address ) );
                    }
                    
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr;
                        debugAddr.sin_family = AF_INET;
                        debugAddr.sin_port = htons(17474);
                        debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char msg[256];
                        sprintf(msg, "```MAINAPP: F1 manual connection initiated - ctrlSocket=%08x", (unsigned int)ctrlSocket.get());
                        sendto(udpSock, msg, strlen(msg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                    }
                }
                return;

            case MsgType::ChangeConfig:
                if ( msg->getAs<ChangeConfig>().value == ChangeConfig::Delay )
                    delayChanged = true;

                if ( msg->getAs<ChangeConfig>().value == ChangeConfig::RollbackDelay )
                    rollbackDelayChanged = true;

                if ( msg->getAs<ChangeConfig>().value == ChangeConfig::Rollback )
                    rollbackChanged = true;

                if ( delayChanged && rollbackChanged )
                    ui.display ( format ( "Input delay was changed to %u\nRollback was changed to %u",
                                          msg->getAs<ChangeConfig>().delay, msg->getAs<ChangeConfig>().rollback ) );
                else if ( delayChanged )
                    ui.display ( format ( "Input delay was changed to %u", msg->getAs<ChangeConfig>().delay ) );
                else if ( rollbackDelayChanged )
                    ui.display ( format ( "P2 Input delay was changed to %u", msg->getAs<ChangeConfig>().rollbackDelay ) );
                else if ( rollbackChanged )
                    ui.display ( format ( "Rollback was changed to %u", msg->getAs<ChangeConfig>().rollback ) );
                return;

            default:
                LOG ( "Unexpected ipcRead ( '%s' )", msg );
                return;
        }
    }

    // Timer callback
    void timerExpired ( Timer *timer ) override
    {
        if ( timer == stopTimer.get() )
        {
            lastError = "Timed out!";
            stop();
        }
        else if ( timer == startTimer.get() )
        {
            startTimer.reset();

            if ( ! clientMode.isSpectate() )
            {
                // We must disconnect the sockets before the game process is created,
                // otherwise Windows say conflicting ports EVEN if they are created later.
                dataSocket.reset();
                serverDataSocket.reset();
                ctrlSocket.reset();
                serverCtrlSocket.reset();
            }

            DWORD val = GetFileAttributes ( ( ProcessManager::appDir + "framestep.dll" ).c_str() );

            bool hasFramestep = true;
            bool loadFramestep = ( GetAsyncKeyState ( VK_F8 ) & 0x8000 ) == 0x8000;
            if ( val == INVALID_FILE_ATTRIBUTES || !loadFramestep )
            {
                hasFramestep = false;
            }

            // F1 connection fix: Don't launch new MBAA.exe if already running
            if ( isF1Connection )
            {
                // UDP debug for F1 connection
                static SOCKET udpSock = INVALID_SOCKET;
                if (udpSock == INVALID_SOCKET) {
                    udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
                }
                if (udpSock != INVALID_SOCKET) {
                    struct sockaddr_in debugAddr;
                    debugAddr.sin_family = AF_INET;
                    debugAddr.sin_port = htons(17474);
                    debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                    
                    char debugMsg[256];
                    sprintf(debugMsg, "```F1_START: Skipping openGame() - MBAA.exe already running");
                    sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                }
                
                // For F1 connections, MBAA.exe is already running, so skip openGame
                // The DLL is already injected and will get the netplay config via IPC
                LOG("F1 connection: Skipping openGame() - using existing MBAA.exe");
                
                // F1 FIX: Set global clientMode to Client mode for F1 connections
                // Don't copy from netplayConfig.mode because invalidate() will reset it
                {
                    static SOCKET udpSock = INVALID_SOCKET;
                    if (udpSock == INVALID_SOCKET) {
                        udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
                    }
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr;
                        debugAddr.sin_family = AF_INET;
                        debugAddr.sin_port = htons(17474);
                        debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char debugMsg[256];
                        sprintf(debugMsg, "```F1_DEBUG: Before setting clientMode - current value=%d", (int)clientMode.value);
                        sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                    }
                }
                
                clientMode = ClientMode ( ClientMode::Client );  // Properly construct ClientMode with value 2
                clientMode.flags = 0;  // Versus mode (no Training flag)
                
                {
                    static SOCKET udpSock = INVALID_SOCKET;
                    if (udpSock == INVALID_SOCKET) {
                        udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
                    }
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr;
                        debugAddr.sin_family = AF_INET;
                        debugAddr.sin_port = htons(17474);
                        debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char debugMsg[256];
                        sprintf(debugMsg, "```F1_DEBUG: After setting clientMode - new value=%d", (int)clientMode.value);
                        sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                    }
                }
                
                // F1 DEBUG: Add hex dump of ClientMode before sending
                LOG("F1: ClientMode before send: value=%d, flags=%d", (int)clientMode.value, (int)clientMode.flags);
                LOG("F1: ClientMode hex dump: %02x %02x %02x %02x %02x %02x %02x %02x",
                    ((uint8_t*)&clientMode)[0], ((uint8_t*)&clientMode)[1],
                    ((uint8_t*)&clientMode)[2], ((uint8_t*)&clientMode)[3],
                    ((uint8_t*)&clientMode)[4], ((uint8_t*)&clientMode)[5],
                    ((uint8_t*)&clientMode)[6], ((uint8_t*)&clientMode)[7]);
                
                // Send config to already-running DLL (same as ipcConnected does)
                // F1 IPC DEBUG: Add comprehensive logging to trace message flow
                {
                    static SOCKET udpSock = INVALID_SOCKET;
                    if (udpSock == INVALID_SOCKET) {
                        udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
                    }
                    if (udpSock != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr;
                        debugAddr.sin_family = AF_INET;
                        debugAddr.sin_port = htons(17474);
                        debugAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char debugMsg[512];
                        sprintf(debugMsg, "```F1_IPC_STATUS: IPC connected=%d, starting message sequence", procMan.isConnected());
                        sendto(udpSock, debugMsg, strlen(debugMsg), 0, (struct sockaddr*)&debugAddr, sizeof(debugAddr));
                    }
                }
                
                // F1 IPC FIX: Send initialization messages directly (they're now working!)
                // Based on debug output, ClientMode messages are reaching the DLL successfully
                // The IPC reset approach was working, just need to avoid killing MBAA.exe
                
                if (!procMan.isConnected()) {
                    LOG("F1_IPC_ERROR: ProcessManager not connected - cannot send IPC messages");
                    ui.display ( format ( "F1 connection failed - no IPC connection" ) );
                    return;
                }
                
                // F1 IPC SIMPLIFIED: Send messages directly (they're working now!)
                // Based on debug output: ClientMode messages reach DLL and get processed
                // Problem was the IPC reset was killing MBAA.exe - now just send messages directly
                
                // Set up config for F1 connection
                netplayConfig.invalidate();
                netplayConfig.mode = ClientMode ( ClientMode::Client );  // Properly construct ClientMode with value 2
                netplayConfig.mode.flags = 0;  // Versus mode (no Training flag)
                
                // Send all initialization messages (same as ipcConnected() callback)
                {
                    static SOCKET udpSock2 = INVALID_SOCKET;
                    if (udpSock2 == INVALID_SOCKET) {
                        udpSock2 = ::socket(AF_INET, SOCK_DGRAM, 0);
                    }
                    if (udpSock2 != INVALID_SOCKET) {
                        struct sockaddr_in debugAddr2;
                        debugAddr2.sin_family = AF_INET;
                        debugAddr2.sin_port = htons(17474);
                        debugAddr2.sin_addr.s_addr = inet_addr("127.0.0.1");
                        
                        char debugMsg2[256];
                        
                        // F1 CRITICAL: Add small delay to ensure DLL is ready
                        Sleep(100);  // 100ms delay for DLL to process
                        sprintf(debugMsg2, "```F1_IPC_WAIT: Waited 100ms, now sending F1-specific messages only");
                        sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                        
                        // F1 FIX: Don't resend Options and ControllerMappings - they were already sent at startup!
                        // Only send what actually changes for F1 connection:
                        // - ClientMode (changes from 6->2)
                        // - IpAddrPort (new connection address)
                        // - NetplayConfig (new network configuration)
                        // procMan.ipcSend ( options );  // SKIP - unchanged from startup
                        // procMan.ipcSend ( ControllerManager::get().getMappings() );  // SKIP - unchanged from startup
                        
                        // Create explicit ClientMode object with correct value FIRST
                        ClientMode f1ClientMode(ClientMode::Client);
                        f1ClientMode.flags = 0;  // Ensure flags are clear
                        
                        // F1 DEBUG: Double-check the ACTUAL object we're sending
                        sprintf(debugMsg2, "```F1_SEND_DEBUG: About to send f1ClientMode=%d, flags=%d, hex=%02x %02x %02x %02x", 
                                (int)f1ClientMode.value, (int)f1ClientMode.flags,
                                ((uint8_t*)&f1ClientMode)[0], ((uint8_t*)&f1ClientMode)[1],
                                ((uint8_t*)&f1ClientMode)[2], ((uint8_t*)&f1ClientMode)[3]);
                        sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                        
                        LOG ( "F1: Sending ClientMode with value=%d, flags=%d", (int)f1ClientMode.value, (int)f1ClientMode.flags );
                        procMan.ipcSend ( f1ClientMode );  // Send explicit Client mode
                        procMan.ipcSend ( new IpAddrPort ( address.getAddrInfo()->ai_addr ) );
                        
                        // F1: Ensure NetplayConfig has correct mode BEFORE sending
                        netplayConfig.mode = f1ClientMode;  // Ensure NetplayConfig has correct mode
                        LOG ( "F1: Sending NetplayConfig with mode.value=%d, delay=%d, rollback=%d",
                              (int)netplayConfig.mode.value, netplayConfig.delay, netplayConfig.rollback );
                        procMan.ipcSend ( netplayConfig );
                        
                        // F1 CRITICAL: Create data socket NOW for F1 client
                        if (clientMode.isClient() && !dataSocket) {
                            sprintf(debugMsg2, "```F1_DATA_SOCKET: Creating UDP data socket for F1 client");
                            sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                            
                            dataSocket = SmartSocket::connectUDP(this, address);
                            LOG("F1: dataSocket=%08x", dataSocket.get());
                            
                            sprintf(debugMsg2, "```F1_DATA_SOCKET: Created dataSocket=%08x", (unsigned int)dataSocket.get());
                            sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                        }
                        
                        // F1 CRITICAL: Send InitialGameState for F1 connections
                        // For F1 connections from training mode, we need to send a default InitialGameState
                        // This tells the DLL what state we're starting from
                        if (clientMode.isClient()) {
                            InitialGameState initialState({ 0, 0 }); // Frame 0, index 0
                            initialState.netplayState = NetplayState::PreInitial; // Start at PreInitial for F1
                            initialState.stage = 0; // Will be selected at CSS
                            initialState.isTraining = 0; // Not training mode
                            
                            // Leave characters unknown - will be selected at CSS
                            initialState.chara[0] = UNKNOWN_POSITION;
                            initialState.chara[1] = UNKNOWN_POSITION;
                            initialState.moon[0] = UNKNOWN_POSITION;
                            initialState.moon[1] = UNKNOWN_POSITION;
                            
                            sprintf(debugMsg2, "```F1_INITIAL_STATE: Sending InitialGameState for F1 client");
                            sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                            
                            LOG("F1: Sending InitialGameState with netplayState=%d", (int)initialState.netplayState);
                            procMan.ipcSend(new InitialGameState(initialState));
                        }
                        
                        sprintf(debugMsg2, "```F1_IPC_SIMPLIFIED: All messages sent - keeping MBAA.exe alive");
                        sendto(udpSock2, debugMsg2, strlen(debugMsg2), 0, (struct sockaddr*)&debugAddr2, sizeof(debugAddr2));
                    }
                }
                
                ui.display ( format ( "Started %s mode", getGameModeString() ) );
            }
            else
            {
                // Normal connection: Open the game and wait for callback to ipcConnected
                procMan.openGame ( ui.getConfig().getInteger ( "highCpuPriority" ),
                                   ( clientMode.isTraining() || clientMode.isReplay() ) && hasFramestep );
            }
        }
        else
        {
            SpectatorManager::timerExpired ( timer );
        }
    }

    // ExternalIpAddress callbacks
    void externalIpAddrFound ( ExternalIpAddress *extIpAddr, const string& address ) override
    {
        LOG ( "External IP address: '%s'", address );
        updateStatusMessage();
    }

    void externalIpAddrUnknown ( ExternalIpAddress *extIpAddr ) override
    {
        LOG ( "Unknown external IP address!" );
        updateStatusMessage();
    }

    // KeyboardManager callback
    void keyboardEvent ( uint32_t vkCode, uint32_t scanCode, bool isExtended, bool isDown ) override
    {
        LOG( "KeyboardEvent in MainApp" );
        if ( vkCode == VK_ESCAPE && !kbCancel ) {
            LOG("Escape");
            kbCancel = true;
            stop();
        }
    }

    // Constructor
    MainApp ( const IpAddrPort& addr, const Serializable& config )
        : Main ( config.getMsgType() == MsgType::InitialConfig
                 ? config.getAs<InitialConfig>().mode
                 : config.getAs<NetplayConfig>().mode )
        , externalIpAddress ( this )
    {
        LOG ( "clientMode=%s; flags={ %s }; address='%s'; config=%s",
              clientMode, clientMode.flagString(), addr, config.getMsgType() );

        options = opt;
        originalAddress = address = addr;

        if ( ! ProcessManager::appDir.empty() )
            options.set ( Options::AppDir, 1, ProcessManager::appDir );

        if ( ui.getConfig().getDouble ( "heldStartDuration" ) > 0 )
        {
            options.set ( Options::HeldStartDuration, 1,
                          format ( "%u", uint32_t ( 60 * ui.getConfig().getDouble ( "heldStartDuration" ) ) ) );
        }

        if ( ui.getConfig().getInteger ( "autoReplaySave" ) > 0 )
        {
            options.set ( Options::AutoReplaySave, 1 );
        }
        if ( ui.getConfig().getInteger ( "frameLimiter" ) > 0 )
        {
            options.set ( Options::FrameLimiter, 1 );
        }
        if ( ! ProcessManager::getIsWindowed() )
        {
            ProcessManager::setIsWindowed ( true );
            options.set ( Options::Fullscreen, 1 );
        }

#ifndef RELEASE
        if ( ! options[Options::StrictVersion] )
            options.set ( Options::StrictVersion, 3 );
#endif

        if ( clientMode.isNetplay() )
        {
            ASSERT ( config.getMsgType() == MsgType::InitialConfig );

            initialConfig = config.getAs<InitialConfig>();

            pinger.owner = this;
            pinger.pingInterval = PING_INTERVAL;
            pinger.numPings = NUM_PINGS;
        }
        else if ( clientMode.isSpectate() )
        {
            ASSERT ( config.getMsgType() == MsgType::InitialConfig );

            initialConfig = config.getAs<InitialConfig>();
        }
        else if ( clientMode.isLocal() )
        {
            ASSERT ( config.getMsgType() == MsgType::NetplayConfig );

            netplayConfig = config.getAs<NetplayConfig>();

            if ( netplayConfig.tournament )
            {
                options.set ( Options::Offline, 1 );
                options.set ( Options::Training, 0 );
                options.set ( Options::Broadcast, 0 );
                options.set ( Options::Spectate, 0 );
                options.set ( Options::Tournament, 1 );
                options.set ( Options::HeldStartDuration, 1, "90" );
            }
        }
        else
        {
            ASSERT_IMPOSSIBLE;
        }

        if ( ProcessManager::isWine() )
        {
            clientMode.flags |= ClientMode::IsWine;
            initialConfig.mode.flags |= ClientMode::IsWine;
            netplayConfig.mode.flags |= ClientMode::IsWine;
        }
    }

    // Destructor
    ~MainApp()
    {
        this->join();

        KeyboardManager::get().unhook();

        procMan.closeGame();

        if ( ! lastError.empty() )
        {
            LOG ( "lastError='%s'", lastError );
            ui.sessionError = lastError;
        }

        syncLog.deinitialize();

        externalIpAddress.owner = 0;
    }

private:

    // Get the current game mode as a string
    const char *getGameModeString() const
    {
        return netplayConfig.tournament
               ? "tournament"
               : ( clientMode.isTraining()
                   ? "training"
                   : "versus" );
    }

    // Update the UI status message
    void updateStatusMessage() const
    {
        if ( isWaitingForUser )
            return;

        if ( clientMode.isBroadcast() && !isBroadcastPortReady )
            return;

        const uint16_t port = ( clientMode.isBroadcast() ? netplayConfig.broadcastPort : address.port );
        if ( ui.isServer() ) {
            ui.display ( format ( "%s at server%s\n",
                                  ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                  ( clientMode.isTraining() ? " (training mode)" : "" ) ) );
        } else if ( externalIpAddress.address.empty() || externalIpAddress.address == ExternalIpAddress::Unknown ) {
            ui.display ( format ( "%s on port %u%s\n",
                                  ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                  port,
                                  ( clientMode.isTraining() ? " (training mode)" : "" ) )
                         + ( externalIpAddress.address.empty()
                             ? "(Fetching external IP address...)"
                             : "(Could not find external IP address!)" ) );
        }
        else
        {
            setClipboard ( format ( "%s:%u", externalIpAddress.address, port ) );
            ui.display ( format ( "%s at %s:%u%s\n(Address copied to clipboard)",
                                  ( clientMode.isBroadcast() ? "Broadcasting" : "Hosting" ),
                                  externalIpAddress.address,
                                  port,
                                  ( clientMode.isTraining() ? " (training mode)" : "" ) ) );
        }
        ui.hostReady();
    }

    // Reset hosting state
    void resetHost()
    {
        ASSERT ( clientMode.isHost() == true );

        LOG ( "Resetting host!" );

        ctrlSocket.reset();
        dataSocket.reset();
        serverDataSocket.reset();

        initialConfig.dataPort = 0;
        initialConfig.remoteName.clear();
        isInitialConfigReady = false;

        netplayConfig.clear();

        pinger.reset();
        pingStats.clear();

        uiSendSocket.reset();
        uiRecvSocket.reset();

        isBroadcastPortReady = isFinalConfigReady = isWaitingForUser = userConfirmed = isF1Connection = false;
    }
};


void runMain ( const IpAddrPort& address, const Serializable& config )
{
    lastError.clear();

    MainApp main ( address, config );

    LOG( "Main Start" );
    main.start();
    LOG( "Main wfuc" );
    main.waitForUserConfirmation();
    LOG( "Main End" );
}

void runFake ( const IpAddrPort& address, const Serializable& config )
{
}
