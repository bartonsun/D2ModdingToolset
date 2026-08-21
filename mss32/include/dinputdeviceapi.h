#ifndef DINPUTDEVICEAPI_H
#define DINPUTDEVICEAPI_H

namespace game::DinputDeviceApi {

constexpr int kCreateDeviceVtbl = 3;
constexpr int kGetDeviceStateVtbl = 9;
constexpr int kGetDeviceDataVtbl = 10;
constexpr unsigned kDiMouseStateBytes = 16;
constexpr unsigned kDiMouseState2Bytes = 20;
constexpr unsigned kRgbButtonsOffset = 12;
constexpr unsigned char kButtonDown = 0x80;
constexpr int kPulseFrames = 6;
constexpr unsigned kPulseMs = 180;
constexpr unsigned kDimofsButton0 = 12;
constexpr unsigned kDiVersion7 = 0x0700;
constexpr unsigned kDiVersion8 = 0x0800;

struct Guid
{
    unsigned long d1;
    unsigned short d2;
    unsigned short d3;
    unsigned char d4[8];
};

const char* pulsePath();
const char* stackMoveCmdPath();
const char* visitSiteCmdPath();
const char* trainerUiCmdPath();
const char* endTurnCmdPath();
bool isMouseStateSize(unsigned cbData);
const Guid& guidSysMouse();
const Guid& iidDirectInput8A();
const Guid& iidDirectInput7A();

} // namespace game::DinputDeviceApi

#endif
