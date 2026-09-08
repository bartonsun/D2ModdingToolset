#include "attackparams.h"
#include "attackview.h"
#include "unitview.h"
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#include <midunit.h>
#include <game.h>
#include <gameutils.h>
#include <unordered_set>
#include <battlemsgdata.h>

namespace bindings {

void AttackHitParamsView::bind(sol::state& lua)
{
    auto attackHitParams = lua.new_usertype<AttackHitParamsView>("AttackHitParams");

    attackHitParams["SetTarget"] = &AttackHitParamsView::setTarget;
    attackHitParams["SetDamage"] = &AttackHitParamsView::setDamage;
    attackHitParams["SetCritDamage"] = &AttackHitParamsView::setCritDamage;
    attackHitParams["SetDrain"] = &AttackHitParamsView::setDrain;
    attackHitParams["SetDrainHealPercent"] = &AttackHitParamsView::setDrainHealPercent;
    attackHitParams["SetHeal"] = &AttackHitParamsView::setHeal;
    attackHitParams["SetAttackLevel"] = &AttackHitParamsView::setAttackLevel;
    attackHitParams["SetAttackCount"] = &AttackHitParamsView::setAttackCount;
    attackHitParams["SetFastRetreat"] = &AttackHitParamsView::setFastRetreat;
    attackHitParams["SetIsRevive"] = &AttackHitParamsView::setIsRevive;
    attackHitParams["SetMiss"] = &AttackHitParamsView::setMiss;
    attackHitParams["SetIsLong"] = &AttackHitParamsView::setIsLong;

    attackHitParams["attacker"] = sol::property(&AttackHitParamsView::getAttacker);
    attackHitParams["target"] = sol::property(&AttackHitParamsView::getTarget);
    attackHitParams["attack"] = sol::property(&AttackHitParamsView::getAttack);
    attackHitParams["attackType"] = sol::readonly_property(&AttackHitParamsView::attackClass);
    attackHitParams["damage"] = sol::readonly_property(&AttackHitParamsView::damage);
    attackHitParams["critDamage"] = sol::readonly_property(&AttackHitParamsView::critDamage);
    attackHitParams["drain"] = sol::readonly_property(&AttackHitParamsView::drain);
    attackHitParams["drainHealPercent"] = sol::readonly_property(&AttackHitParamsView::drainHealPercent);
    attackHitParams["heal"] = sol::readonly_property(&AttackHitParamsView::heal);
    attackHitParams["attackLevel"] = sol::readonly_property(&AttackHitParamsView::attackLevel);
    attackHitParams["attackCount"] = sol::readonly_property(&AttackHitParamsView::attackCount);
    attackHitParams["fastRetreat"] = sol::readonly_property(&AttackHitParamsView::fastRetreat);
    attackHitParams["isRevive"] = sol::readonly_property(&AttackHitParamsView::isRevive);
    attackHitParams["isTemp"] = sol::readonly_property(&AttackHitParamsView::isTemp);
    attackHitParams["itemId"] = sol::readonly_property(&AttackHitParamsView::itemId);
    attackHitParams["miss"] = sol::readonly_property(&AttackHitParamsView::miss);
    attackHitParams["isLong"] = sol::readonly_property(&AttackHitParamsView::isLong);

    attackHitParams["modifiers"] = &AttackHitParamsView::getModifiers;
    attackHitParams["AddModifier"] = &AttackHitParamsView::addModifier;
    attackHitParams["RemoveModifier"] = &AttackHitParamsView::removeModifier;
}


UnitView AttackHitParamsView::getAttacker() const
{
    return UnitView{attacker};
}

IdView AttackHitParamsView::getItemId() const
{
    return IdView{itemId};
}

UnitView AttackHitParamsView::getTarget() const
{
    return UnitView{target};
}
AttackView AttackHitParamsView::getAttack() const
{
    return AttackView{attack};
}

std::string AttackHitParamsView::getAttackClass() const
{
    return attackClass;
}

void AttackHitParamsView::setTarget(sol::object value)
{
    using namespace game;

    if (value.is<IdView>())
    {
        auto& unit = value.as<IdView>();
        CMidUnit* cUnit = gameFunctions().findUnitById(hooks::getObjectMap(), &unit.id);
        target = cUnit;
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetTarget (expected IdView, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetTarget (expected IdView, got %s)", typeName);
}

void AttackHitParamsView::setDamage(sol::object value)
{
    if (value.is<int>()) {
        damage = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetDamage (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetDamage (expected int, got %s)", typeName);
}

void AttackHitParamsView::setCritDamage(sol::object value)
{
    if (value.is<int>()) {
        critDamage = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetCritDamage (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetCritDamage (expected int, got %s)", typeName);
}

void AttackHitParamsView::setDrain(sol::object value)
{
    if (value.is<int>()) {
        drain = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetDrain (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetDrain (expected int, got %s)", typeName);
}

void AttackHitParamsView::setDrainHealPercent(sol::object value)
{
    if (value.is<int>()) {
        drainHealPercent = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetDrainHealPercent (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetDrainHealPercent (expected int, got %s)", typeName);
}

void AttackHitParamsView::setHeal(sol::object value)
{
    if (value.is<int>()) {
        heal = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetHeal (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetHeal (expected int, got %s)", typeName);
}

void AttackHitParamsView::setAttackLevel(sol::object value)
{
    if (value.is<int>()) {
        attackLevel = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetAttackLevel (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetAttackLevel (expected int, got %s)", typeName);
}

void AttackHitParamsView::setAttackCount(sol::object value)
{
    if (value.is<int>()) {
        attackCount = value.as<int>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetAttackCount (expected int, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetAttackCount (expected int, got %s)",
               typeName);
}

void AttackHitParamsView::setFastRetreat(sol::object value)
{
    if (value.is<bool>()) {
        fastRetreat = value.as<bool>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetFastRetreat (expected bool, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetFastRetreat (expected bool, got %s)", typeName);
}

void AttackHitParamsView::setIsRevive(sol::object value)
{
    if (value.is<bool>()) {
        isRevive = value.as<bool>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetIsRevive (expected bool, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetIsRevive (expected bool, got %s)", typeName);
}

void AttackHitParamsView::setIsTemp(sol::object value)
{
    if (value.is<bool>()) {
        isTemp = value.as<bool>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetIsTemp (expected bool, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetIsTemp (expected bool, got %s)", typeName);
}

void AttackHitParamsView::setMiss(sol::object value)
{
    if (value.is<bool>()) {
        miss = value.as<bool>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetMiss (expected bool, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetMiss (expected bool, got %s)", typeName);
}

void AttackHitParamsView::setIsLong(sol::object value)
{
    if (value.is<bool>()) {
        isLong = value.as<bool>();
        return;
    }
    int luaType = static_cast<int>(value.get_type());
    const char* typeName = lua_typename(value.lua_state(), luaType);
    spdlog::debug("attempt to call SetIsLong (expected bool, got {})", typeName);
    luaL_error(value.lua_state(), "attempt to call SetIsLong (expected bool, got %s)", typeName);
}

sol::table AttackHitParamsView::getModifiers(sol::this_state L)
{
    sol::state_view lua(L);
    sol::table result = lua.create_table();
    for (const auto& id : modifierIds) {
        result.add(IdView(id));
    }
    return result;
}

void AttackHitParamsView::addModifier(const std::string& modifierId)
{
    IdView idView{modifierId};
    for (const auto& id : modifierIds) {
        if (*id == idView.id) {
            return;
        }
    }
    modifierIds.push_back(&idView.id);
}

void AttackHitParamsView::removeModifier(const std::string& modifierId)
{
    IdView idView{modifierId};
    modifierIds.erase(std::remove_if(modifierIds.begin(), modifierIds.end(),
                                     [&idView](const game::CMidgardID* id) {
                                         return *id == idView.id;
                                     }),
                      modifierIds.end());
}

} // namespace bindings
