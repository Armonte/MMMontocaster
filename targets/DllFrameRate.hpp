#pragma once

#include <cstdint>


namespace DllFrameRate
{

extern double desiredFps;

extern double actualFps;

void enable();

void limitFPS();

// Stage-background animation rollback decouple (CE). When active, the game DRAWS the stage every frame
// but never ADVANCES it (render-only patch at 0x4238BE); cccaster advances it once per real displayed
// frame via tickStageBackground() from the present hook, so the stage plays at a clean 60fps regardless
// of rollback re-sim / skip-frames. Set true by DllMain at netplay start (CE only -- raw CE addresses).
extern bool stageAnimDecoupleActive;

// Advance the stage background one tick (replicates Background_UpdateAndRender's update half, no render).
void tickStageBackground();

}

extern "C" __attribute__((visibility("default"))) void setDesiredFPS(double desiredFps_);
