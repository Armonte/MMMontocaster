#include "ProcessManager.hpp"
#include "TcpSocket.hpp"
#include "Constants.hpp"
#include "Exceptions.hpp"
#include "ErrorStringsExt.hpp"
#include "GameConfigInstance.hpp"

#include <windows.h>
#include <direct.h>

using namespace std;


void ProcessManager::writeGameInput ( uint8_t player, uint16_t direction, uint16_t buttons )
{
    if ( direction == 5 || direction < 0 || direction > 9 )
        direction = 0;

    ASSERT ( direction >= 0 );
    ASSERT ( direction <= 9 );

    // LOG ( "player=%d; direction=%d; buttons=%04x", player, direction, buttons );

    char *const baseAddr = * ( char ** ) g_gameConfig.getInputBufferPtrAddr();

    switch ( player )
    {
        case 1:
            ( * ( uint16_t * ) ( baseAddr + g_gameConfig.getP1DirectionOffset() ) ) = direction;
            ( * ( uint16_t * ) ( baseAddr + g_gameConfig.getP1ButtonsOffset() ) ) = buttons;
            break;

        case 2:
            ( * ( uint16_t * ) ( baseAddr + g_gameConfig.getP2DirectionOffset() ) ) = direction;
            ( * ( uint16_t * ) ( baseAddr + g_gameConfig.getP2ButtonsOffset() ) ) = buttons;
            break;

        default:
            ASSERT_IMPOSSIBLE;
            break;
    }
}

MsgPtr ProcessManager::getRngState ( uint32_t index ) const
{
    RngState *rngState = new RngState ( index );

    rngState->rngState0 = *g_gameConfig.getRngState0Addr();
    rngState->rngState1 = *g_gameConfig.getRngState1Addr();
    rngState->rngState2 = *g_gameConfig.getRngState2Addr();
    copy ( g_gameConfig.getRngState3Addr(), g_gameConfig.getRngState3Addr() + g_gameConfig.getRngState3Size(), rngState->rngState3.begin() );

    return MsgPtr ( rngState );
}

void ProcessManager::setRngState ( const RngState& rngState )
{
    LOG ( "rngState=%s", rngState.dump() );

    *g_gameConfig.getRngState0Addr() = rngState.rngState0;
    *g_gameConfig.getRngState1Addr() = rngState.rngState1;
    *g_gameConfig.getRngState2Addr() = rngState.rngState2;

    copy ( rngState.rngState3.begin(), rngState.rngState3.end(), g_gameConfig.getRngState3Addr() );
}

void ProcessManager::connectPipe()
{
    LOG ( "Listening on IPC socket" );

    _ipcSocket = TcpSocket::listen ( this, 0 );

    LOG ( "ipcSocket=%08x", _ipcSocket.get() );

    LOG ( "Creating pipe" );

    _pipe = CreateFile (
                NAMED_PIPE,                              // name of the pipe
                GENERIC_READ | GENERIC_WRITE,            // 2-way pipe
                FILE_SHARE_READ | FILE_SHARE_WRITE,      // R/W sharing mode
                0,                                       // default security
                OPEN_EXISTING,                           // open existing pipe
                FILE_ATTRIBUTE_NORMAL,                   // default attributes
                0 );                                     // no template file

    if ( _pipe == INVALID_HANDLE_VALUE ) {
        _pipe = CreateFile (
                NAMED_PIPE2,                             // name of the pipe
                GENERIC_READ | GENERIC_WRITE,            // 2-way pipe
                FILE_SHARE_READ | FILE_SHARE_WRITE,      // R/W sharing mode
                0,                                       // default security
                OPEN_EXISTING,                           // open existing pipe
                FILE_ATTRIBUTE_NORMAL,                   // default attributes
                0 );                                     // no template file
    }
    if ( _pipe == INVALID_HANDLE_VALUE )
        THROW_WIN_EXCEPTION ( GetLastError(), "CreateFile failed", ERROR_PIPE_START );

    LOG ( "Pipe created" );

    DWORD bytes;

    if ( ! WriteFile ( _pipe, & ( _ipcSocket->address.port ), sizeof ( _ipcSocket->address.port ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "WriteFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( _ipcSocket->address.port ) )
        THROW_EXCEPTION ( "wrote %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( _ipcSocket->address.port ) );

    _processId = GetCurrentProcessId();

    LOG ( "processId=%08x", _processId );

    if ( ! WriteFile ( _pipe, &_processId, sizeof ( _processId ), &bytes, 0 ) )
        THROW_WIN_EXCEPTION ( GetLastError(), "WriteFile failed", ERROR_PIPE_RW );

    if ( bytes != sizeof ( _processId ) )
        THROW_EXCEPTION ( "wrote %d bytes, expected %d", ERROR_PIPE_RW, bytes, sizeof ( _processId ) );
}
