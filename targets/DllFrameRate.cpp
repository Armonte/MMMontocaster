#include "DllFrameRate.hpp"
#include "TimerManager.hpp"
#include "Constants.hpp"
#include "ProcessManager.hpp"
#include "DllAsmHacks.hpp"

#include <d3dx9.h>

using namespace std;
using namespace DllFrameRate;


namespace DllFrameRate
{

double desiredFps = 60.0;

double actualFps = 60.0;

bool isEnabled = false;


void enable()
{
    if ( isEnabled )
        return;

    // TODO find an alternative because this doesn't work on Wine
    WRITE_ASM_HACK ( AsmHacks::disableFpsLimit );
    WRITE_ASM_HACK ( AsmHacks::disableFpsCounter );

    isEnabled = true;

    LOG ( "Enabling FPS control!" );
}

}


void PresentFrameEnd ( IDirect3DDevice9 *device )
{
    if ( !isEnabled || *CC_SKIP_FRAMES_ADDR )
        return;

    // Safety check: ensure TimerManager is initialized before using it
    // If not initialized, framerate limiting cannot work properly
    if ( !TimerManager::get().isInitialized() )
        return;

    try
    {
        static uint64_t last1f = 0, last5f = 0, last30f = 0, last60f = 0;
        static uint8_t counter = 0;
        static bool initialized = false;

        // Initialize timing variables on first call
        if ( !initialized )
        {
            uint64_t now = TimerManager::get().getNow ( true );
            last1f = last5f = last30f = last60f = now;
            initialized = true;
        }

        ++counter;

        uint64_t now = TimerManager::get().getNow ( true );

        /**
         * The best timer resolution is only in milliseconds, and we need to make
         * sure the spacing between frames is as close to even as possible.
         *
         * What this code does is check every 30f, 5f, and 1f how many milliseconds have
         * passed since the last check and make sure we are close to or under the desired FPS.
         */
        const uint64_t frameTime1f = static_cast<uint64_t>( 1000 / desiredFps );
        const uint64_t frameTime5f = static_cast<uint64_t>( ( 5 * 1000 ) / desiredFps );
        const uint64_t frameTime30f = static_cast<uint64_t>( ( 30 * 1000 ) / desiredFps );
        
        // Safety: prevent infinite loops if timer is broken
        const uint64_t maxWaitTime = 1000; // Max 1 second wait per frame
        uint64_t loopStartTime = now;
        uint64_t loopIterations = 0;
        const uint64_t maxIterations = 1000000; // Safety limit

        if ( counter % 30 == 0 )
        {
            while ( now - last30f < frameTime30f && loopIterations < maxIterations )
            {
                now = TimerManager::get().getNow ( true );
                ++loopIterations;
                // Safety check: if we've been waiting too long, break out
                if ( now - loopStartTime > maxWaitTime )
                    break;
            }

            last30f = now;
        }
        else if ( counter % 5 == 0 )
        {
            while ( now - last5f < frameTime5f && loopIterations < maxIterations )
            {
                now = TimerManager::get().getNow ( true );
                ++loopIterations;
                // Safety check: if we've been waiting too long, break out
                if ( now - loopStartTime > maxWaitTime )
                    break;
            }

            last5f = now;
        }
        else
        {
            while ( now - last1f < frameTime1f && loopIterations < maxIterations )
            {
                now = TimerManager::get().getNow ( true );
                ++loopIterations;
                // Safety check: if we've been waiting too long, break out
                if ( now - loopStartTime > maxWaitTime )
                    break;
            }
        }

        last1f = now;

        if ( counter >= 60 )
        {
            now = TimerManager::get().getNow ( true );

            actualFps = 1000.0 / ( ( now - last60f ) / 60.0 );

            *CC_FPS_COUNTER_ADDR = uint32_t ( actualFps + 0.5 );

            counter = 0;
            last60f = now;
        }
    }
    catch ( ... )
    {
        // If any exception occurs during framerate limiting, log it but continue
        // The outer catch block in D3DHook.cc will also handle the exception.
        // We don't disable framerate limiting here to prevent it from randomly
        // disabling due to transient issues (e.g., timing edge cases).
        LOG ( "PresentFrameEnd: Exception caught in framerate limiting, continuing" );
        // Exception will propagate to outer catch block in D3DHook.cc
        throw;
    }
}
