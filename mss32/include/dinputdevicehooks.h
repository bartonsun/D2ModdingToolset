#ifndef DINPUTDEVICEHOOKS_H
#define DINPUTDEVICEHOOKS_H

namespace hooks {

bool driveAutomationEnabled();
bool installUser32MouseHooks();
bool installDinputDeviceHooks();
void startDinputMouseHookThread();
bool mousePulseActive();
bool mousePulseHeld();
void mousePulseClient(int* x, int* y);
bool pulseWantsStay();
void resetInjectPhase();
void installUser32MessageHooks();
void fireGameMapClick();
void requestGameMapClick();

} // namespace hooks

#endif
