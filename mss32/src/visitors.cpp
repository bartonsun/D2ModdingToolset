/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#include "visitors.h"
#include "version.h"
#include <array>

namespace game::VisitorApi {

// clang-format off
std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::ChangeUnitHp)0x5e88f4,
        (Api::ChangeUnitXp)0x5e8942,
        (Api::UpgradeUnit)0x5e963d,
        (Api::ForceUnitMax)0x5e972a,
        (Api::AddUnitToGroup)0x5e8bf8,
        (Api::ExchangeItem)0x5e86cc,
        (Api::TransformUnit)0x5e968e,
        (Api::UndoTransformUnit)0x5e96df,
        (Api::ExtractUnitFromGroup)0x5e8d72,
        (Api::ReinsertUnitIntoGroup)0x5e8dc0,
        (Api::SwapUnitPosition)0x5e8855,
        (Api::ReviveUnit)0x5e88a9,
        (Api::CreateItem)0x5e8630,
        (Api::DestroyItem)0x5e867e,
        (Api::MerchantAddItem)0x5e8f52,
        (Api::MerchantDelItem)0x5e8fa3,
        (Api::CreateRod)0x5e99f0,
        (Api::DestroyRod)0x5e9a3e,
        (Api::MoveStack)0x5e839c,
        (Api::ChangeStackMoveAllowance)0x5e8B0e,
        (Api::ChangeMapTerrain)0x5e81d8,
        (Api::RunKillStack)0x5e8e5c,
        (Api::CreateStackDestroyed)0x5e8264,
        (Api::OverlayUnit)0x5E8A21,
        (Api::PlayerSetAttitude)nullptr,
        (Api::SetStackSrcTemplate)0x5e9ef8,
        (Api::MerchantAddBuyCategory)nullptr,
        (Api::CreateSite)nullptr,
        (Api::ChangeSiteInfo)nullptr,
        (Api::ChangeSiteImage)nullptr,
        (Api::ChangeSiteAiPriority)nullptr,
        (Api::EraseStack)0x5e8e11,
        (Api::ChangeStackLeader)0x5E8AC0,
    },
    // Russobit
    Api{
        (Api::ChangeUnitHp)0x5e88f4,
        (Api::ChangeUnitXp)0x5e8942,
        (Api::UpgradeUnit)0x5e963d,
        (Api::ForceUnitMax)0x5e972a,
        (Api::AddUnitToGroup)0x5e8bf8,
        (Api::ExchangeItem)0x5e86cc,
        (Api::TransformUnit)0x5e968e,
        (Api::UndoTransformUnit)0x5e96df,
        (Api::ExtractUnitFromGroup)0x5e8d72,
        (Api::ReinsertUnitIntoGroup)0x5e8dc0,
        (Api::SwapUnitPosition)0x5e8855,
        (Api::ReviveUnit)0x5e88a9,
        (Api::CreateItem)0x5e8630,
        (Api::DestroyItem)0x5e867e,
        (Api::MerchantAddItem)0x5e8f52,
        (Api::MerchantDelItem)0x5e8fa3,
        (Api::CreateRod)0x5e99f0,
        (Api::DestroyRod)0x5e9a3e,
        (Api::MoveStack)0x5e839c,
        (Api::ChangeStackMoveAllowance)0x5e8B0e,
        (Api::ChangeMapTerrain)0x5e81d8,
        (Api::RunKillStack)0x5e8e5c,
        (Api::CreateStackDestroyed)0x5e8264,
        (Api::OverlayUnit)0x5E8A21,
        (Api::PlayerSetAttitude)nullptr,
        (Api::SetStackSrcTemplate)0x5e9ef8,
        (Api::MerchantAddBuyCategory)nullptr,
        (Api::CreateSite)nullptr,
        (Api::ChangeSiteInfo)nullptr,
        (Api::ChangeSiteImage)nullptr,
        (Api::ChangeSiteAiPriority)nullptr,
        (Api::EraseStack)0x5e8e11,
        (Api::ChangeStackLeader)0x5E8AC0,
    },
    // Gog
    Api{
        (Api::ChangeUnitHp)0x5e75f3,
        (Api::ChangeUnitXp)0x5e7641,
        (Api::UpgradeUnit)0x5e833c,
        (Api::ForceUnitMax)0x5e8429,
        (Api::AddUnitToGroup)0x5e78f7,
        (Api::ExchangeItem)0x5e73cb,
        (Api::TransformUnit)0x5e838d,
        (Api::UndoTransformUnit)0x5e83de,
        (Api::ExtractUnitFromGroup)0x5e7a71,
        (Api::ReinsertUnitIntoGroup)0x5e7abf,
        (Api::SwapUnitPosition)0x5e7554,
        (Api::ReviveUnit)0x5e75a8,
        (Api::CreateItem)0x5e732f,
        (Api::DestroyItem)0x5e737d,
        (Api::MerchantAddItem)0x5e7c51,
        (Api::MerchantDelItem)0x5e7ca2,
        (Api::CreateRod)0x5e86ef,
        (Api::DestroyRod)0x5e873d,
        (Api::MoveStack)0x5e709b,
        (Api::ChangeStackMoveAllowance)0x5e780d,
        (Api::ChangeMapTerrain)0x5e6ed7,
        (Api::RunKillStack)0x5e7b5b,
        (Api::CreateStackDestroyed)0x5e6f63,
        (Api::OverlayUnit)0x5e7720,
        (Api::PlayerSetAttitude)nullptr,
        (Api::SetStackSrcTemplate)0x5e8bf7,
        (Api::MerchantAddBuyCategory)nullptr,
        (Api::CreateSite)nullptr,
        (Api::ChangeSiteInfo)nullptr,
        (Api::ChangeSiteImage)nullptr,
        (Api::ChangeSiteAiPriority)nullptr,
        (Api::EraseStack)0x5e7b10,
        (Api::ChangeStackLeader)0x5e77bf,
    },
    // Scenario Editor
    Api{
        (Api::ChangeUnitHp)0,
        (Api::ChangeUnitXp)0,
        (Api::UpgradeUnit)0,
        (Api::ForceUnitMax)0,
        (Api::AddUnitToGroup)0,
        (Api::ExchangeItem)0,
        (Api::TransformUnit)0,
        (Api::UndoTransformUnit)0,
        (Api::ExtractUnitFromGroup)0,
        (Api::ReinsertUnitIntoGroup)0,
        (Api::SwapUnitPosition)0x4ea8d8,
        (Api::ReviveUnit)0,
        (Api::CreateItem)0x4ea79d,
        (Api::DestroyItem)0x4ea7eb,
        (Api::MerchantAddItem)0x4eb040,
        (Api::MerchantDelItem)0x4eb091,
        (Api::CreateRod)0,
        (Api::DestroyRod)0x4eb7c2,
        (Api::MoveStack)0x4ea111,
        (Api::ChangeStackMoveAllowance)0, //TODO
        (Api::ChangeMapTerrain)0x4e9d30, //?
        (Api::RunKillStack)0, //TODO
        (Api::CreateStackDestroyed)0, //TODO
        (Api::OverlayUnit)0, //?
        (Api::PlayerSetAttitude)0x4e9baa,
        (Api::SetStackSrcTemplate)0,
        (Api::MerchantAddBuyCategory)0x4eb0df,
        (Api::CreateSite)0x4eac6f,
        (Api::ChangeSiteInfo)0x4eadb9,
        (Api::ChangeSiteImage)0x4eae6a,
        (Api::ChangeSiteAiPriority)0x4eaeb8,
        (Api::EraseStack)0x4e9987,
        (Api::ChangeStackLeader)0x4eaa5c,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::VisitorApi
