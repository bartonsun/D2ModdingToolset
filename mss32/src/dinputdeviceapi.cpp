#include "dinputdeviceapi.h"

namespace game::DinputDeviceApi {

const char* pulsePath()
{
    return "C:\\d2-lbutton.pulse";
}

const char* stackMoveCmdPath()
{
    return "C:\\d2-stackmove.cmd";
}

const char* visitSiteCmdPath()
{
    return "C:\\d2-visitsite.cmd";
}

const char* trainerUiCmdPath()
{
    return "C:\\d2-trainerui.cmd";
}

const char* endTurnCmdPath()
{
    return "C:\\d2-endturn.cmd";
}

bool isMouseStateSize(unsigned cbData)
{
    return cbData == kDiMouseStateBytes || cbData == kDiMouseState2Bytes;
}

const Guid& guidSysMouse()
{
    static const Guid guid{0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
    return guid;
}

const Guid& iidDirectInput8A()
{
    static const Guid guid{0xBF798030, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
    return guid;
}

const Guid& iidDirectInput7A()
{
    static const Guid guid{0x9A4CB684, 0x236D, 0x11D3, {0x8E, 0x9D, 0x00, 0xC0, 0x4F, 0x68, 0x44, 0xAE}};
    return guid;
}

} // namespace game::DinputDeviceApi
