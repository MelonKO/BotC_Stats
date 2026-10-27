# Просмотр и редактирование ролей в DBConnect — план фичи

Документ описывает фичу «просмотр списка ролей на сервере» по аналогии с вкладкой
«Players» и заранее очерчивает второй этап — редактирование ролей.

Статус: план, реализация не начата.
Ветка этапа 1: `feature/roles-list-view` (ответвлена от `develop`).
Ветка этапа 2: `feature/roles-edit` (ответвляется от `develop` после мержа этапа 1).

## 1. Цель и границы

| Этап | Объём | Затрагиваемые слои |
|------|-------|--------------------|
| 1. Просмотр | Новая вкладка **Roles** в DBConnect: список всех ролей сервера, поиск, сортировка, обновление, состояния загрузки/пусто/ошибка, локализация | только клиент `DBConnect/` + документация |
| 2. Редактирование | Правка роли из списка: название (EN), тип, сторона, описание, переводы | `openapi/`, `core/api/`, `DBConnect/`, документация |

Вне объёма обоих этапов: удаление ролей (на `roles` ссылаются `game_players`,
`DELETE` не выдан ни одному пользователю), массовое редактирование, экспорт списка,
управление языками (`languages`) и справочниками типов/сторон.

Этап 1 сознательно не требует изменений API, схемы БД и прав: `GET /api/roles`
уже реализован и отдаёт всё нужное.

## 2. Что уже есть в репозитории

### 2.1 Серверная сторона (этап 1 — готово)

- `GET /api/roles` — `core/api/app/main.py:126-136`: `lang` опционален (2–5 символов),
  ответ `RolesResponse{roles: [RoleItem]}`.
- `RoleItem` — `core/api/app/models.py:159-178`: `id`, `name`, `alignment`,
  `role_type`, `description` (nullable), `translation: RoleTranslation | None`.
- Запрос — `core/api/app/import_logic.py:195-246`: `LEFT JOIN role_translations` по
  `lang_code`, сортировка `ORDER BY r.role_type, r.name`; роль без перевода языка
  возвращает `translation = null` (`:222-225`). Таблицы `role_type_translations` и
  `alignment_translations` в ответе **не** используются.
- Права на чтение у `api_service` уже есть (`core/init-scripts/03-grant-privileges.sql:41-48`).
- Покрытие тестами: `TestRolesListEndpoint` — `core/api/app/tests/test_main.py:227-266`.
- Контракт: `openapi/api/openapi.yaml:466-502` (`GET /api/roles`), `RoleItem` — `:248-280`.
- Клиент сгенерирован: `OAIRolesApi::listRoles(OptionalParam<QString> lang)` —
  `DBConnect/generated_api/OAIRolesApi.h:68-71`; `OAIRoleItem` содержит
  `id/name/alignment/role_type/description/translation` — `OAIRoleItem.h:43-71`.
  Свежесть проверена: `python scripts/project.py generate --check` → «Generated code is up to date».

### 2.2 Клиент: образец «Players»

- `PlayersWidget` (`DBConnect/src/ui/PlayersWidget.cpp:33-71`) — конструктор:
  `PlayersTableModel` + `QSortFilterProxyModel` (фиксированная строка фильтра,
  без учёта регистра), сортировка по имени, скрытый вертикальный заголовок,
  `QHeaderView::Stretch`; затем `refreshFromCache()` + `DBCache::updatePlayers()`
  и busy-состояние из `isPlayersRequestInFlight()`.
- Данные берутся только из `DBCache` (`onPlayersCacheUpdated → refreshFromCache`,
  `PlayersWidget.cpp:56-57`, `:189-200`), выбор восстанавливается по id после
  `model reset`.
- Ошибки: `describeNetworkError()` + tooltip с исходным текстом (`:243-272`),
  пустой список отличается от недоступного API.
- `PlayersTableModel` (`PlayersTableModel.h:31-40`, `:52-75`) — колонки-перечисление,
  id в `PlayerIdRole` (скрытый), `retranslate()`-слот для смены языка.
- `DBCache` (`DBConnect/src/db_cache/DBCache.h:42-58`, `DBCache.cpp:83-129`):
  роли кешируются **только как `QStringList`** отображаемых имён (перевод, иначе
  оригинал), ошибка запроса ролей не сохраняется и наружу не отдаётся (в отличие от
  игроков: `getPlayersError()`, `getPlayersErrorType()`, `isPlayersRequestInFlight()`).
- Потребители кеша ролей: `ImportGamesWidget.cpp:40` (`updateRoles("ru")`),
  `:77` (`getRolesCache()` для валидации CSV) и `MainWindow.cpp:47` (после импорта ролей).
- `BotCApiClient` без изменений: `listRoles()` и сигнал `rolesListFinished` уже есть,
  таймаут 30 с (`BotCApiClient.cpp:84-87`, `:152-159`).
- Вкладки — `MainWindow.cpp:26-42` (создаются кодом, `MainWindow.ui` содержит только
  placeholder) и `MainWindow.cpp:75-83` (переименование при смене языка).
- Сборка и переводы — `DBConnect/CMakeLists.txt:29-67` (список `SOURCES`),
  `:91-97` (`qt_add_translations`), каталог `DBConnect/translations/dbconnect_ru.ts`.

## 3. Принятые решения

| № | Развилка | Решение |
|---|----------|---------|
| D1 | Названия вкладок | Новая вкладка — `Roles`, ставится сразу после `Players`. Отдельным коммитом импортные вкладки переименовываются `Roles import → Import Roles`, `Games import → Import Games` (сейчас код расходится с README, а `Roles` и `Roles import` рядом читались бы как дубли). |
| D2 | Источник данных | Расширить `DBCache` до полных записей ролей (по образцу `PlayerEntry`), а не заводить в виджете собственный запрос: кеш — единственный владелец списка ролей, запросы уже склеиваются, обновление после импорта ролей и после смены настроек сервера работает само. |
| D3 | Локализация типа и стороны | Первый этап показывает `role_type`/`alignment` значениями справочника (`Townsfolk`, `good` и т. п.) с локальным `tr()`-отображением на русский. Серверные `role_type_translations`/`alignment_translations` не подключаем; если понадобится — отдельная фича с расширением `RoleItem`. |
| D4 | Способ правки (этап 2) | Модальный диалог `RoleEditDialog`, а не inline-правка ячеек: у роли пять полей, два из которых — выпадающие списки (`role_type`, `alignment`), плюс перевод. |
| D5 | Ветки | Этап 1 — `feature/roles-list-view` (уже создана). Этап 2 — новая ветка `feature/roles-edit` от `develop` после мержа этапа 1, а не продолжение текущей. |

## 4. Этап 1 — просмотр списка ролей

### 4.1 Что видит пользователь

Вкладка **Roles**: таблица всех ролей сервера

| Колонка | Содержимое |
|---------|------------|
| Role | перевод названия (`translation.name`), при его отсутствии — `name` |
| Name | `name` из справочника (английское, системное значение) |
| Type | `role_type`, отображается через `tr()` (Townsfolk → Горожанин и т. д.) |
| Side | `alignment`: `good` / `evil` / `neutral` (см. `core/docs/SIDES.md`) |
| Description | `translation.description`, при отсутствии — `description` |

Плюс: поле поиска (фильтр по всем колонкам, фиксированная строка, без учёта
регистра), кнопка **Refresh** с блокировкой на время запроса, счётчик
`Roles: N, shown: M`, статусная строка.

Состояния статуса: во время загрузки — `Loading roles…` (кнопка Refresh
заблокирована); успех — `✔ Roles list loaded.`; пусто —
`No roles in the database.`; ошибка — `✘ Failed to load roles. <описание>`
(401 — неверный ключ, таймаут 30 с, нет соединения) с tooltip исходного текста.

Обновление данных происходит без действий пользователя: после `Import Roles`,
после сохранения настроек сервера (`serverSettingsChanged → DBCache::refreshAll`)
и по кнопке Refresh.

### 4.2 Задачи

| № | Задача | Файлы |
|---|--------|-------|
| R1.1 | Добавить `RoleEntry` (`id`, `name`, `roleType`, `alignment`, `description`, `translationName`, `translationDescription` + `displayName()`/`displayDescription()`), хранилище `QVector<RoleEntry>`, геттеры `getRoleEntries()`, `getRolesError()`, `getRolesErrorType()`, `isRolesRequestInFlight()`; заполнять записи в `onRoleListFinished` (`translation` nullable — читать через `is_translation_Set()`), `m_cachedRoles` собирать из записей (сохранить текущую семантику «перевод, иначе оригинал»); очищать ошибку в `requestRoles()` по образцу `requestPlayers()`; добавить сигнал о начале запроса (сейчас кеш уведомляет только о завершении, поэтому вкладка не узнаёт, что обновление уже идёт извне — после импорта ролей или сохранения настроек) | `DBConnect/src/db_cache/DBCache.h`, `DBCache.cpp` |
| R1.2 | Модель таблицы: колонки `Role/Name/Type/Side/Description`, `RoleIdRole = Qt::UserRole + 1` (скрытый), `setRoles()`, `roleIdAt()`, `headerData()` через `tr()`, `retranslate()` | `DBConnect/src/ui/RolesTableModel.{h,cpp}` (новые) |
| R1.3 | Виджет: `.ui` копируется с `PlayersWidget.ui` (поиск, Refresh, таблица ≥250 px, alternating rows, single row selection, статус + счётчик); прокси-модель с `setFilterKeyColumn(-1)` (поиск по всем колонкам; у Players явно задан только `Name`, `PlayersWidget.cpp:41`), сортировка по `Role`, сохранение выбора по id, `changeEvent/retranslate`, `refreshFromCache`, `applyStatus`, busy по `isRolesRequestInFlight()` + сигналу начала запроса. Тип и сторона отображаются явным `switch` с литералами `tr("Townsfolk")`, `tr("good")`…, чтобы строки попали в каталог при `lupdate`; `retranslate()` после смены языка обновляет и данные модели (`dataChanged`), и заголовки | `DBConnect/src/ui/RolesWidget.{h,cpp,ui}` (новые) |
| R1.4 | Вкладка в главном окне: `m_rolesWidget`, `addTab(..., tr("Roles"))` после Players, строка переименования в `retranslate()`, при старте `DBCache::updateRoles("ru")` (тот же язык, что в `MainWindow.cpp:47`) | `DBConnect/src/ui/MainWindow.{h,cpp}` |
| R1.5 | Переименование импортных вкладок в `Import Roles` / `Import Games` (D1), отдельным коммитом | `MainWindow.cpp:30,34,79-80`, каталог переводов |
| R1.6 | Регистрация новых файлов и `translations --update` + перевод новых строк (контексты `botc::ui::RolesWidget`, `botc::ui::RolesTableModel`), плейсхолдеры `%1`/`%2` сохранить | `DBConnect/CMakeLists.txt:56-67`, `DBConnect/translations/dbconnect_ru.ts` |
| R1.7 | Документация: строка про DBConnect и раздел использования в `README.md:31,53`, актуализировать пример `GET /api/roles` (сейчас в ответе нет `id`, `description`, `translation`) — `core/docs/API-ACCESS.md:314-338`, новая запись `C10` (следующий свободный номер после `C9`, `core/docs/KNOWN-ISSUES.md:278-297`): ошибка списка ролей раньше не сохранялась в кеше и не доходила до UI, теперь показывается на вкладке | `README.md`, `core/docs/API-ACCESS.md`, `core/docs/KNOWN-ISSUES.md` |

Порядок: R1.1 → R1.2 → R1.3 → R1.4/R1.5/R1.6 → R1.7. Коммиты по одной задаче,
сообщения на английском (`feat: add roles list view to DBConnect`).
Следующий шаг после утверждения плана — реализация R1.1 на ветке
`feature/roles-list-view`.

### 4.3 Критерии приёмки этапа 1

- На вкладке Roles видны все роли сервера; для роли без RU-перевода показывается
  английское название и описание, пустых ячеек нет.
- Поиск фильтрует по отображаемому названию, английскому названию, типу, стороне и
  описанию; кириллица и латиница работают без учёта регистра.
- Кнопка Refresh блокируется на время запроса; повторное нажатие не порождает второй
  параллельный запрос (склейка в `DBCache`).
- Ошибки различаются: неверный ключ (401), таймаут, недоступный сервер — текст через
  `describeNetworkError()`, список не выдаётся за пустую базу.
- После импорта ролей и после сохранения настроек сервера список обновляется сам.
- Переключение языка интерфейса EN/RU перерисовывает заголовки, статусы и названия
  вкладок без перезапуска.
- `python DBConnect/scripts/build.py` собирает Debug без новых предупреждений;
  `python scripts/project.py verify` зелёный (контракт не менялся, generated-код свежий).
- Проверено вручную против локального стека (`core/docker-compose up -d`,
  `SSL_VERIFY = false` в локальном `DBConnect/config.ini`).

### 4.4 Ручной чек-лист

1. Пустая БД ролей → `No roles in the database.`
2. Неверный API key в Settings → ошибка 401 с понятным текстом, повтор после
   исправления ключа восстанавливает список.
3. Остановленный API → сообщение о соединении, tooltip с деталями.
4. Поиск `дамочка`, `Chambermaid`, `Townsfolk`, `neutral` — ожидаемые строки.
5. Сортировка по колонкам Type/Side/Name, затем поиск — фильтр и сортировка совместимы.
6. `Import Roles` файла с новой ролью → она появляется на вкладке Roles.
7. Смена языка интерфейса на вкладке Settings → все строки новые, кириллица не «плывёт».
8. Растягивание окна: описания не обрезаются в нечитаемый вид.

## 5. Этап 2 — редактирование роли

Отдельная ветка `feature/roles-edit` от `develop` после мержа этапа 1.

### 5.1 Контракт API (сначала спека, потом код)

`openapi/api/openapi.yaml`:

- новый путь `PATCH /api/roles/{roleId}` (`operationId: updateRole`, tag `Roles`,
  security `ApiKeyAuth`, опциональный `lang` как у `GET /api/roles`);
- схема `RoleUpdateRequest`: все поля опциональны, `name` 1–200, `alignment`
  `^(good|evil|neutral)$`, `role_type` из перечисления, `description` nullable
  ≤2000, `translations: {lang: RoleTranslation}`; минимум одно поле в теле
  (`minProperties: 1`);
- ответы: 200 `RoleItem` (с `translation` для запрошенного `lang`), 401, 404
  (нет роли), 409 (имя занято), 422 (некорректное тело/неизвестный язык).

После изменения спеки — только `python scripts/project.py generate`
(`core/api/app/models.py` и `DBConnect/generated_api` автогенерируются, руками не
правятся); в клиенте появится `OAIRoleUpdateRequest` и `OAIRolesApi::updateRole`.

### 5.2 Сервер

| № | Задача | Файлы |
|---|--------|-------|
| R2.1 | `update_role(role_id, payload)` в **одной транзакции** (учесть I4: `import_roles` не транзакционен), `UPDATE roles ... RETURNING`, upsert переводов по `(role_id, lang_code)`; перед upsert явно проверить язык по таблице `languages`, чтобы неизвестный `lang` давал 422, а не ошибку FK. Пустое тело запроса проверять в эндпоинте: `minProperties: 1` в спеке не гарантирует проверку в сгенерированной Pydantic-модели | `core/api/app/import_logic.py` |
| R2.2 | Исключение конфликта имени по образцу `PlayerNameConflictError` (`import_logic.py:23-28`, `:379-381`); пустое имя → `ValueError` | `import_logic.py` |
| R2.3 | Маршрут и маппинг ошибок: 200/404/409/422, как у `PATCH /api/players/{playerId}` (`core/api/app/main.py:162-185`) | `core/api/app/main.py` |
| R2.4 | Права: у `api_service` уже есть `UPDATE` на `roles` и `INSERT, UPDATE, SELECT` на `role_translations` (`03-grant-privileges.sql:41-48`) — новых грантов не нужно; если решим ограничить набор колонок (как `100-grant-players-update.sql`), добавить миграцию `101-…` | `core/init-scripts/03-grant-privileges.sql` (проверить), `core/migrations/` |
| R2.5 | Тесты: `TestRoleUpdateEndpoint` по образцу `TestPlayerRenameEndpoint` (`test_main.py:354-510`, мок `fetchrow` + разбор параметров), включая неизвестный язык (422) и пустое тело (422); плюс `test_schemas.py` на `RoleUpdateRequest` (неверный `alignment`, длина имени) | `core/api/app/tests/` |

### 5.3 Клиент

| № | Задача | Файлы |
|---|--------|-------|
| R2.6 | `BotCApiClient::updateRole(roleId, request)` + сигнал `roleUpdateFinished(OAIRoleItem, errorType, httpStatus, errorStr)` по образцу `updatePlayer` (`BotCApiClient.h:55`, сигнал `:75-77`); HTTP-код читается только из full-worker варианта сигнала (`BotCApiClient.cpp:191-200`) — без него 404/409/422 не различить; таймаут ролей 30 с уже задан (`BotCApiClient.cpp:150`) | `DBConnect/src/api/BotCApiClient.{h,cpp}` |
| R2.7 | `DBCache::applyRoleUpdate(...)` — точечное обновление записи в кеше по id, по образцу `applyPlayerRename` (`DBCache.cpp:34-54`) | `DBConnect/src/db_cache/DBCache.{h,cpp}` |
| R2.8 | `RoleEditDialog`: name (EN), Type (combo), Side (combo: good/evil/neutral), Description, RU name/description; клиентская валидация (1–200, ≤2000), кнопки Save/Cancel, блокировка на время запроса | `DBConnect/src/ui/RoleEditDialog.{h,cpp,ui}` (новые) |
| R2.9 | Открытие диалога — двойной клик по строке и/или кнопка Edit; таблица остаётся только для чтения (никаких редактируемых флагов в модели — D4); подтверждение с предупреждением: импорт партий ищет роли по **русским переводам** `role_translations` (`import_logic.py:60-63`), поэтому правка RU-названия меняет то, какие роли найдёт следующий CSV-импорт, а правка английского имени расходится с ключом `roles.name`, по которому импорт ролей делает upsert; маппинг 404/409/422 и сетевых ошибок; после успеха — патч кеша и `updateRoles("ru")` | `DBConnect/src/ui/RolesWidget.{h,cpp}` |
| R2.10 | Регистрация файлов в CMake, новые строки в `dbconnect_ru.ts`, перевод | `DBConnect/CMakeLists.txt`, `DBConnect/translations/dbconnect_ru.ts` |

### 5.4 Документация и проверка этапа 2

- `core/docs/API-ACCESS.md`: новый раздел `PATCH /api/roles/{roleId}` (тело, коды,
  пример cURL), предупреждение о влиянии правки `role_type`/`alignment` на
  аналитические представления (`v_role_stats`, `v_role_type_stats` считаются из
  `roles`, поэтому правка меняет статистику ретроспективно) и о переименовании
  ролей относительно будущих CSV-импортов.
- `core/docs/KNOWN-ISSUES.md`: связать с I4 (транзакционность ролей), Q1 (изменяемость `roles.alignment` упомянута там как побочный аргумент против выводимой исторической стороны) и Q4 (статистика ролей по текущей стороне роли).
- `README.md`: описание возможностей DBConnect.
- Проверка: `python scripts/project.py verify`, `python scripts/project.py generate --check`
  (в коммите generated-код из спеки), ручные сценарии — правка перевода, правка типа,
  конфликт имени (409), правка удалённой роли (404), пустое имя/неверный тип (422),
  недоступный сервер, отмена диалога; визуальная проверка Metabase после правки типа.

## 6. Риски и связанные известные проблемы

| Риск | Что делать |
|------|------------|
| Общий кеш ролей используется валидацией импорта партий (`ImportGamesWidget.cpp:77`) — ошибка в `getRolesCache()` ломает импорт | `m_cachedRoles` продолжает собираться из записей в том же порядке и с той же семантикой (перевод, иначе оригинал); добавить проверку вручную: импорт партий после появления вкладки Roles |
| Гонка кеша ролей при старте (KNOWN-ISSUES C3) | Вне объёма фичи и **не** считается исправленной: показ ошибки на вкладке Roles только упрощает диагностику; рекомендация из C3 (дождаться начальной загрузки перед валидацией CSV) остаётся отдельной задачей |
| `lang` в `GET /api/roles` не сверяется с таблицей `languages` — опечатка даёт молча `translation = null` (KNOWN-ISSUES) | Зафиксировать в документации этапа 1; валидацию языка делать отдельной задачей, не в этой фиче |
| Правка `role_type`/`alignment` меняет историческую аналитику | Предупреждение в диалоге и в `API-ACCESS.md`; при необходимости — отказ от правки этих полей на этапе 2 (см. Q4) |
| `DELETE` ролям не выдан, удаление не предусмотрено | Вне объёма; UI не предлагает удаление |
| Описания длинные (≤2000), колонки растянуты | `QHeaderView::Stretch` для названий, для Description — `Interactive` + tooltip полным текстом |
| Клиент и сервер разъезжаются по контракту | Обязательный `python scripts/project.py generate --check` в проверке обеих веток |

## 7. Git-процесс

- Ветка этапа 1 — `feature/roles-list-view` от `develop` (создана).
- Сообщения коммитов английские, по одной задаче (`feat:`, `docs:`, `chore:`).
- Push — только после того, как работа проверена, отчёт о изменениях и проверках
  предоставлен и пользователь явно подтвердил отправку.
- Этап 2 — `feature/roles-edit` от `develop` после мержа этапа 1 (D5).
- Версия монорепозитория не меняется: фича не ломает контракт (этап 1 — только
  клиент, этап 2 — аддитивный путь API).

## 8. Открытые вопросы к обсуждению

1. Нужен ли на этапе 1 показ перевода типа/стороны из БД (`role_type_translations`,
   `alignment_translations`)? Сейчас предложено отображать их через `tr()` (D3).
2. Переименовывать ли импортные вкладки в `Import Roles`/`Import Games` (D1) — меняет
   привычные подписи.
3. Нужна ли на этапе 2 правка `role_type` и `alignment`, учитывая влияние на
   аналитику, или достаточно названия, описания и переводов?
4. Нужен ли фильтр по типу/стороне (выпадающие списки над таблицей) уже на этапе 1
   или хватает текстового поиска?
