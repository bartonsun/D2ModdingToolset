/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Vladimir Makeev.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "menuphasehooks.h"
#include "log.h"
#include "mempool.h"
#include "menucustomlobby.h"
#include "menucustomnewskirmishmulti.h"
#include "menuphase.h"
#include "menurandomscenariomulti.h"
#include "menurandomscenariosingle.h"
#include "midgard.h"
#include "netcustomservice.h"
#include "originalfunctions.h"
#include "scenariotemplates.h"
#include <fmt/format.h>

namespace hooks {

game::CMenuBase* __stdcall createMenuCustomLobbyCallback(game::CMenuPhase* menuPhase)
{
    auto menu = (CMenuCustomLobby*)game::Memory::get().allocate(sizeof(CMenuCustomLobby));
    return new (menu) CMenuCustomLobby(menuPhase);
}

// TODO: The same for CMenuLoadSkirmishMulti
game::CMenuBase* __stdcall createMenuCustomNewSkirmishMultiCallback(game::CMenuPhase* menuPhase)
{
    auto menu = (CMenuCustomNewSkirmishMulti*)game::Memory::get().allocate(
        sizeof(CMenuCustomNewSkirmishMulti));
    return new (menu) CMenuCustomNewSkirmishMulti(menuPhase);
}

game::CMenuPhase* __fastcall menuPhaseCtorHooked(game::CMenuPhase* thisptr,
                                                 int /*%edx*/,
                                                 int a2,
                                                 int a3)
{
    getOriginalFunctions().menuPhaseCtor(thisptr, a2, a3);

    loadScenarioTemplates();

    return thisptr;
}

void __fastcall menuPhaseDtorHooked(game::CMenuPhase* thisptr, int /*%edx*/, char flags)
{
    freeScenarioTemplates();

    getOriginalFunctions().menuPhaseDtor(thisptr, flags);
}

void __fastcall menuPhaseSwitchPhaseHooked(game::CMenuPhase* thisptr,
                                           int /*%edx*/,
                                           game::MenuTransition transition)
{
    using namespace game;

    auto& midgardApi = CMidgardApi::get();
    auto& menuPhase = CMenuPhaseApi::get();

    auto data = thisptr->data;
    while (true) {
        switch (data->currentPhase) {
        default:
            logDebug("transitions.log", "Current is unknown - doing nothing");
            break;
        case MenuPhase::Session: {
            logDebug("transitions.log", "Current is Session");
            auto midgard = midgardApi.instance();
            midgardApi.setClientsNetProxy(midgard, thisptr);

            data->currentPhase = MenuPhase::Session2LobbyJoin;
            if (static_cast<int>(data->currentPhase) > static_cast<int>(MenuPhase::Last)) {
                // TODO: wtf is this check
                return;
            }
            continue;
        }
        case MenuPhase::Unknown: {
            logDebug("transitions.log", "Current is Unknown");
            data->currentPhase = transition == MenuTransition::Unknown2NewSkirmish
                                     ? MenuPhase::Unknown2NewSkirmish
                                     : MenuPhase::Unknown2LobbyHost;
            if (static_cast<int>(data->currentPhase) > static_cast<int>(MenuPhase::Last)) {
                // TODO: wtf is this check
                return;
            }
            continue;
        }
        case MenuPhase::Credits2Main:
            logDebug("transitions.log", "Current is Credits2Main");
            data->currentPhase = MenuPhase::Back2Main;
            menuPhase.switchToMain(thisptr);
            break;
        case MenuPhase::Back2Main:
            logDebug("transitions.log", "Current is Back2Main");
            menuPhase.switchToMain(thisptr);
            break;
        case MenuPhase::Main:
            logDebug("transitions.log", "Current is Main");
            menuPhase.transitionFromMain(thisptr, transition);
            break;
        case MenuPhase::Main2Single:
            logDebug("transitions.log", "Current is Main2Single");
            menuPhase.switchToSingle(thisptr);
            break;
        case MenuPhase::Single:
            logDebug("transitions.log", "Current is Single");
            menuPhase.transitionFromSingle(thisptr, transition);
            break;
        case MenuPhase::Protocol: {
            logDebug("transitions.log", "Current is Protocol");
            if (transition == MenuTransition::Protocol2CustomLobby) {
                logDebug("transitions.log", "Show Protocol2CustomLobby");
                auto data = thisptr->data;
                menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase,
                                                  &data->interfManager, &data->currentMenu,
                                                  MenuPhase::Protocol2CustomLobby,
                                                  CMenuCustomLobby::transitionFromProtoName);
            } else {
                menuPhase.transitionFromProto(thisptr, transition);
            }
            break;
        }
        case MenuPhase::Protocol2CustomLobby:
        case MenuPhase::Back2CustomLobby: {
            logDebug("transitions.log", "Show CustomLobby");
            CMenuPhaseApi::Api::CreateMenuCallback tmp = createMenuCustomLobbyCallback;
            auto* callback = &tmp;
            menuPhase.showMenu(thisptr, &data->currentPhase, &data->interfManager,
                               &data->currentMenu, &data->transitionAnimation,
                               MenuPhase::CustomLobby, nullptr, &callback);
            break;
        }
        case MenuPhase::CustomLobby: {
            logDebug("transitions.log", "Current is CustomLobby");
            break;
        }
        case MenuPhase::Hotseat:
            logDebug("transitions.log", "Current is Hotseat");
            menuPhase.transitionFromHotseat(thisptr, transition);
            break;
        case MenuPhase::Single2RaceCampaign:
            logDebug("transitions.log", "Current is Single2RaceCampaign");
            menuPhase.switchToRaceCampaign(thisptr);
            break;
        case MenuPhase::RaceCampaign:
            logDebug("transitions.log", "Current is RaceCampaign");
            menuPhase.transitionGodToLord(thisptr, transition);
            break;
        case MenuPhase::Single2CustomCampaign:
            logDebug("transitions.log", "Current is Single2CustomCampaign");
            menuPhase.switchToCustomCampaign(thisptr);
            break;
        case MenuPhase::CustomCampaign:
            logDebug("transitions.log", "Current is CustomCampaign");
            menuPhase.transitionNewQuestToGod(thisptr, transition);
            break;
        case MenuPhase::Single2NewSkirmish:
            logDebug("transitions.log", "Current is Single2NewSkirmish");
            if (data->networkGame && getNetService()) {
                logDebug("transitions.log", "Show CMenuCustomNewSkirmishMulti");
                CMenuPhaseApi::Api::CreateMenuCallback
                    tmp = createMenuCustomNewSkirmishMultiCallback;
                auto* callback = &tmp;
                menuPhase.showMenu(thisptr, &data->currentPhase, &data->interfManager,
                                   &data->currentMenu, &data->transitionAnimation,
                                   MenuPhase::NewSkirmishMulti, nullptr, &callback);
            } else {
                menuPhase.switchToNewSkirmish(thisptr);
            }
            break;
        case MenuPhase::NewSkirmishSingle:
            logDebug("transitions.log", "Current is NewSkirmishSingle");
            if (transition == MenuTransition::NewSkirmish2RaceSkirmish) {
                menuPhase.transitionFromNewSkirmish(thisptr);
            } else if (transition == MenuTransition::NewSkirmishSingle2RandomScenarioSingle) {
                menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase,
                                                  &data->interfManager, &data->currentMenu,
                                                  MenuPhase::NewSkirmishSingle2RandomScenarioSingle,
                                                  "TRANS_NEWQUEST2RNDSINGLE");
            } else if (transition == MenuTransition::NewSkirmishMulti2RandomScenarioMulti) {
                menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase,
                                                  &data->interfManager, &data->currentMenu,
                                                  MenuPhase::NewSkirmishMulti2RandomScenarioMulti,
                                                  "TRANS_HOST2RNDMULTI");
            } else {
                logError("mssProxyError.log", "Invalid menu transition from NewSkirmishSingle");
                return;
            }
            break;
        case MenuPhase::NewSkirmish2RaceSkirmish:
            logDebug("transitions.log", "Current is NewSkirmish2RaceSkirmish");
            menuPhase.switchToRaceSkirmish(thisptr);
            break;
        case MenuPhase::RaceSkirmish:
            logDebug("transitions.log", "Current is RaceSkirmish");
            menuPhase.transitionGodToLord(thisptr, transition);
            break;
        case MenuPhase::RaceSkirmish2Lord:
            logDebug("transitions.log", "Current is RaceSkirmish2Lord");
            menuPhase.switchToLord(thisptr);
            break;
        case MenuPhase::Single2LoadSkirmish:
            logDebug("transitions.log", "Current is Single2LoadSkirmish");
            menuPhase.switchToLoadSkirmish(thisptr);
            break;
        case MenuPhase::Single2LoadCampaign:
            logDebug("transitions.log", "Current is Single2LoadCampaign");
            menuPhase.switchToLoadCampaign(thisptr);
            break;
        case MenuPhase::Single2LoadCustomCampaign:
            logDebug("transitions.log", "Current is Single2LoadCustomCampaign");
            menuPhase.switchToLoadCustomCampaign(thisptr);
            break;
        case MenuPhase::Main2Intro:
            logDebug("transitions.log", "Current is Main2Intro");
            menuPhase.switchIntroToMain(thisptr);
            break;
        case MenuPhase::Main2Credits:
            logDebug("transitions.log", "Current is Main2Credits");
            menuPhase.transitionToCredits(thisptr, transition);
            break;
        case MenuPhase::Credits:
            logDebug("transitions.log", "Current is Credits");
            menuPhase.switchIntroToMain(thisptr);
            break;
        case MenuPhase::Main2Proto:
            logDebug("transitions.log", "Current is Main2Proto");
            menuPhase.switchToProtocol(thisptr);
            break;
        case MenuPhase::Protocol2Multi:
            logDebug("transitions.log", "Current is Protocol2Multi");
            menuPhase.switchToMulti(thisptr);
            break;
        case MenuPhase::Multi:
            logDebug("transitions.log", "Current is Multi");
            menuPhase.transitionFromMulti(thisptr, transition);
            break;
        case MenuPhase::Protocol2Hotseat:
            logDebug("transitions.log", "Current is Protocol2Hotseat");
            menuPhase.switchToHotseat(thisptr);
            break;
        case MenuPhase::Hotseat2NewSkirmishHotseat:
            logDebug("transitions.log", "Current is Hotseat2NewSkirmishHotseat");
            menuPhase.switchToNewSkirmishHotseat(thisptr);
            break;
        case MenuPhase::Hotseat2LoadSkirmishHotseat:
            logDebug("transitions.log", "Current is Hotseat2LoadSkirmishHotseat");
            menuPhase.switchToLoadSkirmishHotseat(thisptr);
            break;
        case MenuPhase::NewSkirmishHotseat: {
            logDebug("transitions.log", "Current is NewSkirmishHotseat");
            if (transition == MenuTransition::NewSkirmishHotseat2HotseatLobby) {
                menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase,
                                                  &data->interfManager, &data->currentMenu,
                                                  MenuPhase::NewSkirmishHotseat2HotseatLobby,
                                                  "TRANS_NEW2HSLOBBY");
            } else if (transition == MenuTransition::NewSkirmishHotseat2RandomScenarioHotseat) {
                // Reuse animation when transitioning from CMenuNewSkirmishHotseat
                // to CMenuRandomScenarioSingle
                menuPhase
                    .showFullScreenAnimation(thisptr, &data->currentPhase, &data->interfManager,
                                             &data->currentMenu,
                                             MenuPhase::NewSkirmishHotseat2RandomScenarioHotseat,
                                             "TRANS_NEWQUEST2RNDSINGLE");
            } else {
                logError("mssProxyError.log", "Invalid menu transition from NewSkirmishHotseat");
                return;
            }

            break;
        }
        case MenuPhase::NewSkirmishHotseat2HotseatLobby:
            logDebug("transitions.log", "Current is NewSkirmishHotseat2HotseatLobby");
            menuPhase.switchToHotseatLobby(thisptr);
            break;
        case MenuPhase::Multi2Session:
            logDebug("transitions.log", "Current is Multi2Session");
            menuPhase.switchToSession(thisptr);
            break;
        case MenuPhase::NewSkirmish2LobbyHost:
            logDebug("transitions.log", "Current is NewSkirmish2LobbyHost");
            menuPhase.switchToLobbyHostJoin(thisptr);
            break;
        case MenuPhase::WaitInterf:
            logDebug("transitions.log", "Current is WaitInterf");
            menuPhase.switchToWait(thisptr);
            break;
        case MenuPhase::NewSkirmishSingle2RandomScenarioSingle: {
            logDebug("transitions.log", "Current is NewSkirmishSingle2RandomScenarioSingle");
            // Create random scenario single menu window during fullscreen animation
            CMenuPhaseApi::Api::CreateMenuCallback tmp = createMenuRandomScenarioSingle;
            auto* callback = &tmp;
            logDebug("transitions.log", "Try to transition to RandomScenarioSingle");
            menuPhase.showMenu(thisptr, &data->currentPhase, &data->interfManager,
                               &data->currentMenu, &data->transitionAnimation,
                               MenuPhase::RandomScenarioSingle, nullptr, &callback);
            break;
        }
        case MenuPhase::RandomScenarioSingle: {
            logDebug("transitions.log", "Current is RandomScenarioSingle");
            menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase, &data->interfManager,
                                              &data->currentMenu,
                                              MenuPhase::NewSkirmish2RaceSkirmish,
                                              "TRANS_RNDSINGLE2GOD");
            break;
        }
        case MenuPhase::NewSkirmishHotseat2RandomScenarioHotseat: {
            logDebug("transitions.log", "Current is NewSkirmishHotseat2RandomScenarioHotseat");
            // Create random scenario single menu window during fullscreen animation.
            // Reuse CMenuRandomScenarioSingle for hotseat games
            CMenuPhaseApi::Api::CreateMenuCallback tmp = createMenuRandomScenarioSingle;
            auto* callback = &tmp;
            menuPhase.showMenu(thisptr, &data->currentPhase, &data->interfManager,
                               &data->currentMenu, &data->transitionAnimation,
                               MenuPhase::RandomScenarioHotseat, nullptr, &callback);
            break;
        }
        case MenuPhase::RandomScenarioHotseat: {
            logDebug("transitions.log", "Current is RandomScenarioHotseat");
            // CMenuRandomScenarioSingle state for hotseat game mode.
            menuPhase.showFullScreenAnimation(thisptr, &data->currentPhase, &data->interfManager,
                                              &data->currentMenu,
                                              MenuPhase::NewSkirmishHotseat2HotseatLobby,
                                              "TRANS_RND2HSLOBBY");
            break;
        }
        case MenuPhase::NewSkirmishMulti2RandomScenarioMulti: {
            logDebug("transitions.log", "Current is NewSkirmishMulti2RandomScenarioMulti");

            // Create random scenario multi menu window during fullscreen animaation
            CMenuPhaseApi::Api::CreateMenuCallback tmp = createMenuRandomScenarioMulti;
            auto* callback = &tmp;
            logDebug("transitions.log", "Try to transition to RandomScenarioMulti");
            menuPhase.showMenu(thisptr, &data->currentPhase, &data->interfManager,
                               &data->currentMenu, &data->transitionAnimation,
                               MenuPhase::RandomScenarioMulti, nullptr, &callback);
            break;
        }
        case MenuPhase::RandomScenarioMulti: {
            logDebug("transitions.log", "Current is RandomScenarioMulti");
            menuPhase.transitionFromNewSkirmish(thisptr);
            break;
        }
        }

        break;
    }
}

void __fastcall menuPhaseBackToMainOrCloseGameHooked(game::CMenuPhase* thisptr,
                                                     int /*%edx*/,
                                                     bool showIntroTransition)
{
    using namespace game;

    // Back to main if a lobby user is not logged in
    auto service = getNetService();
    if (!service || !service->loggedIn()) {
        getOriginalFunctions().menuPhaseBackToMainOrCloseGame(thisptr, showIntroTransition);
        return;
    }

    const auto& midgardApi = CMidgardApi::get();
    const auto& menuPhaseApi = CMenuPhaseApi::get();

    // Close the current scenario file
    menuPhaseApi.openScenarioFile(thisptr, nullptr);

    // Clear network state while preserving net service - because it holds connection with the
    // lobby server. GameSpy does the same.
    auto data = thisptr->data;
    midgardApi.clearNetworkState(data->midgard);

    // Back to lobby screen
    menuPhaseApi.showFullScreenAnimation(thisptr, &data->currentPhase, &data->interfManager,
                                         &data->currentMenu, MenuPhase::Back2CustomLobby,
                                         CMenuCustomLobby::transitionFromBlackName);
}

} // namespace hooks
