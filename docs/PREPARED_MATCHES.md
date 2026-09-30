# Подготовленные матчи MSS

Нужны поддерживающий сервер и MSS. Автогенерация и native save проверены только
для Russobit; обычные ручные режимы других изданий не меняются. ОХ и тестовый
драйвер в этот набор изменений не входят.

## Создание и вход

Хост получает штатное Yes/No только в свободном лобби. Диалог показывает участников,
расы/лордов и условия; текст измеряется native шрифтом, длинные условия разбиваются
на страницы без обрезания. «Нет» откладывает попытку, а не отменяет матч.

После «Да» клиент выбирает последнюю локальную версию шаблона по имени и семейству.
Варианты Duo/Trinity и авторские ответвления не смешиваются; неизвестный суффикс
требует точного basename, одинаковые старшие версии дают ошибку неоднозначности.
Параметры проверяются по выбранному Lua; недопустимые значения не округляются.
MD5 примера сайта не ограничивает выбор. Каталог и исходные байты закреплены до
перезапуска клиента; скачивания и перезаписи Lua нет.

Preview/Retry/Accept/Cancel используют обычный генератор. Retry и `111` заново
исполняют Lua с принятыми параметрами и расами. Внешние require/dofile-ресурсы
шаблона не архивируются. Accept создаёт обычную комнату, выставляет расу хоста и
при необходимости переключает лорда штатным BTN_LORD, сохраняя портрет.

Гостю предлагается вход лишь после создания комнаты и при разрешённых приглашениях
в профиле. До показа и после согласия проверяется свежий список комнат: ID, хост,
свободное место. Дальше используется обычный Join с FilesHash, TemplateHash и паролем.
Гость сам выбирает расу/лорда. Accepted означает начало входа, не его завершение.

## Идентичность и отмена

Попытка имеет preparationId, gameId, attemptId и revision; они передаются отдельными
room properties. Повторы не создают вторую комнату или новый popup. Решённые
приглашения сохраняются в ограниченном кеше по попытке, комнате и получателю.

Cancel до CreateRoom запрещает принятие preview на UI-потоке; старый worker
завершается до следующей генерации. Если CreateRoom уже отправлен, клиент ждёт
его ответ. Поздняя отмена не уничтожает созданную комнату и не прерывает выбор лорда.
Callback старого диалога не действует после logout/login. Вызовы native UI выполняются
из обработчика главного потока, после завершения доставки сетевых событий.

## Wire

Существующие packet IDs не перенумеровываются. Prepared использует
`ID_USER_PACKET_ENUM + 16`; новый capability не вводится, старые клиенты игнорируют
неизвестные команды. Источник пакетов — только аутентифицированный lobby GUID.

Все числа big-endian, строки имеют u16 длину в байтах. Identity: три ASCII ID
`[A-Za-z0-9_-]` длиной 1–64 и ненулевая revision u32. Payload начинается с
version=1 и opcode. Полные поля Offer/Status определены в
[preparedmatchprotocol.h](../mss32/include/preparedmatchprotocol.h),
порядок сериализации — в [кодеке](../mss32/src/preparedmatchprotocol.cpp).

| Opcode | Направление | Содержимое после version/opcode |
| --- | --- | --- |
| Offer=0 / Status=1 / Cancel=2 | core → MSS / MSS → core / core → MSS | Предложение генерации / статус / отмена по identity |
| JoinOffer=3 | core → MSS | identity, room u32, host UTF-8 (1–192 байта), title (0–256), recipient (1–192) |
| JoinStatus=4 | MSS → core | identity, room u32, state u8, detail (0–128) |
| JoinCancel=5 | core → MSS | identity, room u32 |

Room ID 0 допустим, UINT32_MAX запрещён. JoinState: Received=0, Busy=1, Prompt=2,
Accepted=3, Declined=4, Unavailable=5. Native race: Empire0, Undead1, Legions2,
Clans3, Elves5; random=-1. Lord: Mage0, Warrior1, Thief2.
Усечения, лишние байты, неизвестные значения и чужой recipient отвергаются.

## FilesHash при входе в лобби

После login MSS сообщает прежний общий FilesHash пакетом `ID_USER_PACKET_ENUM + 17`:
version=1 (u8), затем ровно 32 ASCII lowercase hex, без имени и длины строки.
Core связывает хеш с авторизованным соединением. Это заявление совместимости,
не доказательство отсутствия читов. Список файлов и обычная проверка Join не меняются.

Login и ручной Host/Join используют один фоновый расчёт и кеш. Ошибка оставляет
хеш неизвестным; повтор расчёта возможен по явному Host/Join. После успешной отправки
RELIABLE_ORDERED повторов нет; локальный отказ постановки допускает три попытки
с интервалом в секунду. Logout отменяет публикацию, уничтожение service ждёт worker.

## Проверки

В MSVC x86 developer shell из корня, с submodules соответствующей версии:

```powershell
.\tests\run-prepared-match-protocol.ps1 -OutputDirectory .\artifacts\prepared-tests -DependenciesRoot .
.\tests\run-client-compatibility.ps1 -OutputDirectory .\artifacts\compatibility-tests
.\tests\run-item-potion-fields.ps1 -OutputDirectory .\artifacts\potion-tests
.\tests\run-turn-script-optional.ps1
```

`CoreRoot` у первого скрипта необязателен: добавляет проверки обоих настоящих кодеков.
`run-scenario-template-recipe.ps1` использует RSG и Lua objects успешной Release-сборки.
Заголовки и библиотека D2RSG должны принадлежать одному commit; ABI между версиями
менялся. Сборка CI пересобирает ScenarioGenerator из закреплённого submodule.

Native-причины hooks: [NATIVE_LOBBY.md](NATIVE_LOBBY.md).
Игровой прогон: [PREPARED_MATCHES_ACCEPTANCE_RU.md](PREPARED_MATCHES_ACCEPTANCE_RU.md).
Консольные тесты и компиляция не заменяют эту приёмку.
