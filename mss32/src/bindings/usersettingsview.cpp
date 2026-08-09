#include "usersettingsview.h"

#include "usersettings.h"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace bindings {

using namespace hooks;

void UserSettingsView::bind(sol::state& lua)
{
    lua.new_usertype<Color>("Color", "r", &Color::r, "g", &Color::g, "b", &Color::b);

    lua.new_usertype<Hotkey>("Hotkey", "key", &Hotkey::key, "ctrl", &Hotkey::ctrl, "shift",
                             &Hotkey::shift, "alt", &Hotkey::alt);

    lua.new_usertype<Hotkeys>("Hotkeys", "openSelectedObject", &Hotkeys::openSelectedObject,
                              "quickSave", &Hotkeys::quickSave);

    lua.new_usertype<UnitEncyclopedia>(
        "UnitEncyclopedia", "detailedUnitDescription", &UnitEncyclopedia::detailedUnitDescription,
        "detailedAttackDescription", &UnitEncyclopedia::detailedAttackDescription,
        "displayDynamicUpgradeValues", &UnitEncyclopedia::displayDynamicUpgradeValues,
        "displayBonusHp", &UnitEncyclopedia::displayBonusHp, "displayBonusXp",
        &UnitEncyclopedia::displayBonusXp, "displayInfiniteAttackIndicator",
        &UnitEncyclopedia::displayInfiniteAttackIndicator, "displayCriticalHitTextInAttackName",
        &UnitEncyclopedia::displayCriticalHitTextInAttackName, "updateOnShiftKeyPress",
        &UnitEncyclopedia::updateOnShiftKeyPress, "updateOnCtrlKeyPress",
        &UnitEncyclopedia::updateOnCtrlKeyPress, "updateOnAltKeyPress",
        &UnitEncyclopedia::updateOnAltKeyPress);

    lua.new_usertype<MovementDisplay>("MovementDisplay", "textColor", &MovementDisplay::textColor,
                                      "outlineColor", &MovementDisplay::outlineColor, "show",
                                      &MovementDisplay::show, "showMovementAfterAction",
                                      &MovementDisplay::showMovementAfterAction,
                                      "previewWhileWaiting", &MovementDisplay::previewWhileWaiting);

    lua.new_usertype<Lobby::Server>("LobbyServer", "ip", &Lobby::Server::ip, "port",
                                    &Lobby::Server::port);

    lua.new_usertype<Lobby::Client>("LobbyClient", "port", &Lobby::Client::port);

    lua.new_usertype<Lobby>("Lobby", "server", &Lobby::server, "client", &Lobby::client);

    lua.new_usertype<UserSettings>(
        "UserSettings",

        "showBanners", &UserSettings::showBanners, 
        "showResources", &UserSettings::showResources,
        "showLandConverted", &UserSettings::showLandConverted,
        "carryOverItemsMax", &UserSettings::carryOverItemsMax,

        "customSortOrder", &UserSettings::customSortOrder,

        "hotkeys", &UserSettings::hotkeys,

        "unitEncyclopedia", &UserSettings::unitEncyclopedia,

        "movementDisplay", &UserSettings::movementDisplay,

        "lobby", &UserSettings::lobby,

        "hasSection",
        [](const UserSettings&, const std::string& name) {
            auto env = userSettingsEnvironment();

            if (!env)
                return false;

            auto object = (*env)[name];
            return object.valid() && object.get_type() == sol::type::table;
        },

        "getSectionNames",
        [](const UserSettings&) {
            std::vector<std::string> names;

            auto env = userSettingsEnvironment();

            if (!env)
                return names;

            for (const auto& kv : *env) {

                sol::object key = kv.first;

                if (key.get_type() == sol::type::string
                    && kv.second.get_type() == sol::type::table) {
                    names.push_back(key.as<std::string>());
                }
            }

            return names;
        },

        "getValue",
        [](const UserSettings&, const std::string& path, sol::this_state ts) -> sol::object {
            lua_State* L = ts;

            spdlog::info("current lua {:p}", static_cast<void*>(L));
            const auto* env = userSettingsEnvironment();

            if (!env || path.empty())
                return sol::make_object(L, sol::nil);

            sol::object current = *env;

            std::string token;

            auto advance = [&](const std::string& part) -> bool {
                if (part.empty())
                    return false;

                if (current.get_type() != sol::type::table)
                    return false;

                sol::table table = current;

                char* end = nullptr;
                long index = std::strtol(part.c_str(), &end, 10);

                sol::object next;

                if (*end == '\0')
                    next = table.get<sol::object>(static_cast<int>(index));
                else
                    next = table.get<sol::object>(part);

                if (!next.valid() || next == sol::lua_nil)
                    return false;

                current = std::move(next);
                return true;
            };

            for (size_t i = 0; i < path.size();) {
                token.clear();

                while (i < path.size() && path[i] != '.' && path[i] != '[')
                    token += path[i++];

                if (!token.empty()) {
                    if (!advance(token))
                        return sol::make_object(L, sol::nil);
                }

                if (i < path.size() && path[i] == '[') {
                    ++i;

                    token.clear();

                    while (i < path.size() && path[i] != ']')
                        token += path[i++];

                    if (i >= path.size())
                        return sol::make_object(L, sol::nil);

                    ++i;

                    if (!advance(token))
                        return sol::make_object(L, sol::nil);
                }

                if (i < path.size() && path[i] == '.')
                    ++i;
            }

            return current;
        });

    lua.set_function("getUserSettings", []() -> const UserSettings& { return userSettings(); });
}

} // namespace bindings
