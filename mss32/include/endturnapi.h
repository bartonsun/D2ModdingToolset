#ifndef ENDTURNAPI_H
#define ENDTURNAPI_H

namespace game {

struct CPhaseGame;

namespace EndTurnApi {

struct Api
{
    using SendEndTurnMsg = void(__thiscall*)(CPhaseGame* thisptr);
    SendEndTurnMsg sendEndTurnMsg;

    using StratInterfEndTurn = void(__thiscall*)(void* thisptr, int pressed, void* button);
    StratInterfEndTurn stratInterfEndTurn;
};

Api& get();

} // namespace EndTurnApi

} // namespace game

#endif
