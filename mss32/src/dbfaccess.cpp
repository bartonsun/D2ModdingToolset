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

#include "dbfaccess.h"
#include "dbf/dbffile.h"
#include "midgardid.h"
#include "utils.h"
#include <functional>
#include <spdlog/spdlog.h>

namespace utils {

template <typename T>
using Convertor = std::function<bool(T&, const DbfRecord&, const DbfColumn&)>;

template <typename T>
bool dbRead(T& result,
            const DbfFile& database,
            size_t row,
            const std::string& columnName,
            const Convertor<T>& convertor)
{
    if (row >= database.recordsTotal()) {
        return false;
    }

    DbfRecord record;
    if (!database.record(record, row)) {
        return false;
    }

    const DbfColumn* column{database.column(columnName)};
    if (!column) {
        return false;
    }

    return convertor(result, record, *column);
}

bool dbRead(game::CMidgardID& id,
            const DbfFile& database,
            size_t row,
            const std::string& columnName)
{
    auto convertId = [](game::CMidgardID& id, const DbfRecord& record, const DbfColumn& column) {
        std::string idString;
        if (!record.value(idString, column)) {
            return false;
        }

        const auto& idApi = game::CMidgardIDApi::get();
        game::CMidgardID tmpId{};
        idApi.fromString(&tmpId, idString.c_str());
        if (tmpId == game::invalidId) {
            return false;
        }

        id = tmpId;
        return true;
    };

    return dbRead<game::CMidgardID>(id, database, row, columnName, convertId);
}

bool dbRead(int& result, const DbfFile& database, size_t row, const std::string& columnName)
{
    auto convertInt = [](int& result, const DbfRecord& record, const DbfColumn& column) {
        int tmp{};
        if (!record.value(tmp, column)) {
            return false;
        }

        result = tmp;
        return true;
    };

    return dbRead<int>(result, database, row, columnName, convertInt);
}

bool dbValueExists(const std::filesystem::path& dbfFilePath,
                   const std::string& columnName,
                   const std::string& value)
{
    DbfFile dbf;
    if (!dbf.open(dbfFilePath)) {
        spdlog::error("Could not open {:s}", dbfFilePath.filename().string());
        return false;
    }

    for (std::uint32_t i = 0; i < dbf.recordsTotal(); ++i) {
        DbfRecord record;
        if (!dbf.record(record, i)) {
            spdlog::error("Could not read record {:d} from {:s}", i,
                          dbfFilePath.filename().string());
            return false;
        }

        if (record.isDeleted()) {
            continue;
        }

        std::string text;
        if (record.value(text, columnName) && hooks::trimSpaces(text) == value)
            return true;
    }

    return false;
}

} // namespace utils
