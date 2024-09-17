::
:: This file is part of the modding toolset for Disciples 2.
:: (https://github.com/VladimirMakeev/D2ModdingToolset)
:: Copyright (C) 2020 Vladimir Makeev.
::
:: This program is free software: you can redistribute it and/or modify
:: it under the terms of the GNU General Public License as published by
:: the Free Software Foundation, either version 3 of the License, or
:: (at your option) any later version.
::
:: This program is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU General Public License for more details.
::
:: You should have received a copy of the GNU General Public License
:: along with this program.  If not, see <http://www.gnu.org/licenses/>.
::


:: Call developer console
CALL VsDevCmd
:: Build Detours library, ignore build errors. We only need detours.lib
cd %1
nmake > nul 2>&1
:: Copy built library into project folder
if exist %1\lib.X86\detours.lib (
xcopy %1\lib.X86\detours.* "%~dp0\mss32\Lib\" /i /y
) else (
echo "Could not find detours.lib, please check for build errors."
exit 1
)
exit 0
