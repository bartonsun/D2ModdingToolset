# Пробный перенос PR8 на stable

Локальный rebase выполнен без переписывания опубликованных PR7/8/10/11.
Игровая DLL не заменялась. Этот перенос ещё не является игровой приёмкой.

## Зафиксированные точки

| Назначение | Ветка / commit |
| --- | --- |
| Исходный PR8 | `codex/prepared-matches`, `9ebb5ce20fe475ef33929c7b7f5c5f4136ddfc4e` |
| Его база PR7 | `codex/lobby-restart`, `47a17f8e9c713e091047ea82bcfcdd79c4189ced` |
| Общая база с stable | `8ab0e925d0aa4aa7fb7cbc5ab386e2192f2f86f6` |
| Проверенная stable Бартона | `649785ee15b8147dd500d975692c03142e87066b` |
| Резерв исходного PR8 | `codex/pr8-before-stable-20260927` |
| Пробная ветка | `codex/prepared-matches-stable-rebase-20260927` |
| Результат переноса исходников | `3ff1ea23389d443a4259aaf3cfe9773b05546339` |
| Новая точка основания prepared-части | `6e9d42c2` — перенесённый эквивалент PR7 |

Перенесены все 33 коммита: 25 коммитов рейтингового лобби/сохранений/рестартов
и 8 коммитов PR8. Только восемь верхних коммитов не являются самостоятельной
реализацией: они используют нижележащую функциональность.

## Конфликт и решение

Первые 32 коммита применились без конфликтов. Последний остановился только на
`mss32/src/turnhooks.cpp`:

- stable использует `sol::protected_function` и `getProtectedScriptFunction`;
- PR8 допускает отсутствие необязательного `Scripts/turn.lua`.

Оставлены обе части: native begin-turn вызывается первым; отсутствующий Lua
пропускается; существующий загружается через защищённый upstream-путь.
Тест `run-turn-script-optional.ps1` адаптирован к этому пути и дополнен двумя
отрицательными проверками отката на незащищённый тип/loader.
Остальные отличия `range-diff` — контекст upstream и окончания файлов.

## D2RSG: обязательная пересборка

stable закрепляет `caaf16cbc838a7c434a7298971d78e351d9d5b53` вместо
`a0af02c644bb573a427f4174eaff626043f06e97`. Это шесть upstream-коммитов:
изменены `MapTemplateSettings`, `MapTemplateContents`, `GroupInfo`, `ZoneOptions`
и внутренние поля `MapGenerator`; добавлены события и stack templates.
Старая библиотека с новыми заголовками ABI-несовместима.

Для локальной проверки получена отдельная точная копия нового D2RSG в
`artifacts/stable-rebase-20260927/dependencies/D2RSG`; общие зависимости не менялись.
Собрана новая Release Win32/v143 `/MT` библиотека: 0 ошибок, 24 предупреждения
upstream; SHA256 `2f44dca783c643fdcbb7c88a3c210278c63863dd461a50d4925ae662f02ec66c`.
MSS и тесты используют этот же набор заголовков и эту библиотеку.

## Проверки

- Prepared protocol, lifecycle, settings, 113 text checks — PASS.
- Core prepared tests и 6165 join cross-wire checks — PASS.
- Client compatibility — PASS.
- MOD_POTION: 8/8 Debug и 8/8 Release — PASS.
- Optional `turn.lua`: source/loader regression и 6 negative controls — PASS.
- Первая полная MSS Release Win32 сборка остановилась на нехватке места C:
  (`C1085`/`FTK1006`, затем ошибки записи tracking logs), а не C++-диагностике.
  Частичный выход не является успешной сборкой. Лог сохранён отдельно перед повтором.
- После завершения компиляторов выполнено обратимое LZX-сжатие собственных
  промежуточных объектов: 563 из старой сборки и 470 из первой попытки новой.
  Файлы не удалялись; старые DLL, RSG и четыре прежних локальных отчёта проверены по SHA256.
- Повторная полная MSS Release Win32/v143 — PASS: 0 ошибок, 4 прежних C4018.
  DLL: `artifacts/stable-rebase-20260927/out/mss32.dll`, 6 745 600 байт,
  SHA256 `3a6c122a3c75558a10e69979685a6ad59c8fcee4ec6b36cb0e2f34148a4cbae1`.
  Команда линковки в `build.log` подтверждает использование именно нового RSG.
- Scenario template recipe regression — PASS, включая оба локальных Outrunner
  (`Outrunner_kotovasiya2.lua`, `Outrunner-kotovasiya.lua`) с новым RSG и Lua-объектами MSS.
- `git fsck --no-reflogs --no-dangling`, `git diff --check` — PASS.
- Генерация, Retry/Accept, `111`, сохранение/загрузка и два клиента в игре не проверялись.

Установленная игровая DLL не менялась:
SHA256 `43c2f8155262b21c1ad2e5271a2587852955609b3e8c0e338ab93c14348502e6`.
Это прежняя сборка с ОХ/харнесом; заменять её OH-off пробной PR8 ради rebase не нужно.
Сервер/site также не менялись. Перед новой тяжёлой сборкой снова проверить C:
свободное место остаётся ограничивающим фактором этой рабочей машины.

## Независимая проверка и границы переноса

`range-diff`: 33 → 33; 29 коммитов patch-identical. Два различия связаны с EOF,
одно — с upstream-контекстом/include, последнее — с описанным Lua-конфликтом.
`preparedmatch.cpp`, prepared wire и `lobbyrestart.cpp/.h` побайтово совпадают
с исходным PR8. `hideBuildingPopup`, новые upstream-хуки и изменения движения
не вытеснены нашими настройками/хуками восстановления.

Уже в stable `649785ee`, до переноса, есть двойная регистрация
`stratInterfKeyHandler → hookedKeyHandler`, отключённые potion constructors/DOT
hooks и изменение `sendBeforeObjectsChanges` для конца движения стека.
Это отдельные риски upstream для startup/save/move smoke, а не разрешённые
конфликты rebase. В этой пробной ветке они намеренно не менялись.

## Evidence → Finding → Path

**E1.** Git: исходный граф содержит 33 отсутствующих в stable коммита и не содержит
merge-коммитов; rebase сохранил все 33. Проверка из этого репозитория:

```powershell
git range-diff 8ab0e925..9ebb5ce2 649785ee..3ff1ea23
git merge-base --is-ancestor 649785ee 3ff1ea23
git diff --check 649785ee 3ff1ea23
```

**E2.** `turnhooks.cpp` относительно stable добавляет только проверку отсутствующего
файла, не отменяя защищённый Lua-вызов. Проверка: `git diff 649785ee 3ff1ea23 -- mss32/src/turnhooks.cpp`.

**E3.** [Сравнение закреплённых версий D2RSG](https://github.com/Bartonsun/D2RSG/compare/a0af02c644bb573a427f4174eaff626043f06e97...caaf16cbc838a7c434a7298971d78e351d9d5b53)
показывает изменение компоновки структур; локальный `build-rsg.log` подтверждает
сборку новой библиотеки. RSP/targets/логи этой проверки сохранены в
`artifacts/stable-rebase-20260927/`, это локальные, не публикуемые build-артефакты.

**F1 (E1–E3).** Механический перенос небольшой: один разрешённый конфликт.
Основной риск — новый генератор и согласованная пересборка зависимостей,
а не потеря prepared-функциональности при rebase.

**P1.** Резерв исходных refs → отдельная пробная ветка → все 33 коммита на stable →
сохранение обоих Lua-исправлений → pinned D2RSG build → offline/native проверки →
отдельная игровая приёмка. Публикация trial и изменение base/head существующего PR8
требуют отдельного выбора; force-push не выполнялся.
