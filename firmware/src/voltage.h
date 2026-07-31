#pragma once

// Pin-touching half of the voltmeter. The pure conversion maths lives in
// voltage_math.h so it can be unit tested on the host.
#include "voltage_math.h"

void voltageInit();

// Powers the divider, averages NUM_SAMPLES readings, and powers it back down
// again — unless a manual display window is open, in which case the gauge is
// left on. Blocks for roughly 300 ms, so call it only when a reading is
// actually needed.
float voltageRead();

// Keeps the analog gauge powered for `ms` so a person standing at the device
// can read it. Restarts the window if one is already open.
void voltageDisplayFor(unsigned long ms);

// Powers the gauge down once the display window expires. Call from loop().
void voltageTick();

// True while a display window is open.
bool voltageDisplayActive();
