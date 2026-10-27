# Blood on the Clocktower Statistics — Project

[![CI](https://github.com/MelonKO/BotC_Stats/actions/workflows/ci.yml/badge.svg)](https://github.com/MelonKO/BotC_Stats/actions/workflows/ci.yml)
[![Release](https://github.com/MelonKO/BotC_Stats/actions/workflows/release.yml/badge.svg)](https://github.com/MelonKO/BotC_Stats/actions/workflows/release.yml)

**Blood on the Clocktower Statistics (BotC Stats)** — система учёта партий настольной игры Blood on the Clocktower с базой данных PostgreSQL, REST API и набором инструментов для импорта и аналитики.

## Architecture

```
┌───────────────────────────────────────────────────────────┐
│                    BotC Monorepo                           │
├──────────────┬──────────────┬─────────────────────────────┤
│   core/      │  DBConnect/  │  Future integrations        │
│  Docker:     │  Qt6 GUI     │  e.g. bot or web UI          │
│  PostgreSQL  │  CSV → API   │                              │
│  + FastAPI   │              │                              │
│  + Nginx     │              │                              │
│  + Metabase  │              │                              │
└──────┬───────┴──────┬───────┴──────────────────────────────┘
       │              │
       ▼              ▼
 PostgreSQL DB ←── POST /api/games/import
```

## Components

| Компонент | Описание | Подробности |
|-----------|----------|-------------|
| **[core/](core/)** | Docker-стек: PostgreSQL 16, FastAPI, Nginx и Metabase. REST API, база данных и аналитические панели. | [core/README.md](core/README.md) |
| **[DBConnect/](DBConnect/)** | Qt6 desktop GUI: импорт партий и ролей из CSV, просмотр списка ролей и игроков, переименование игрока. Новые игроки создаются при импорте партий. Использует автогенерированный OpenAPI-клиент. | [DBConnect/](DBConnect/) |

## Quick Start

### 1. Запуск сервера (core)

```bash
cd core
cp .env.example .env
# Edit .env: set unique passwords, the Metabase secret, and SERVER_IP.
bash scripts/generate-cert.sh
docker-compose up -d
```

### 2. Создание API-ключа

```bash
bash scripts/create-api-key.sh create "Your Name" "contact"
```

### 3. Импорт данных (DBConnect)

Открыть приложение DBConnect. На вкладке **Settings** указать API URL и API key,
сохранить. Список ролей доступен на вкладке **Roles**. Для импорта выбрать
CSV-файл на вкладке **Games import** или **Roles import** и нажать **Import**.

Приложение читает и сохраняет `config.ini` относительно текущего рабочего каталога; не добавляйте в Git файл с реальным API-ключом. При каждой конфигурации CMake в каталог сборки копируется `DBConnect/config.ini`, если он существует, иначе — `DBConnect/config.ini.example`. Поэтому изменения только в копии каталога сборки могут быть перезаписаны при следующей конфигурации; для локальных настроек задайте исходный `DBConnect/config.ini` и запускайте приложение из каталога с нужным `config.ini`.

Язык интерфейса переключается на вкладке **Settings → Interface → Language** (`System`, `English`, `Русский`) и применяется сразу, без перезапуска. Выбор хранится в `config.ini` в секции `[UI]` (`LANGUAGE = auto|en|ru`, по умолчанию `auto` — язык системы). Язык интерфейса не меняет язык игровых данных: роли по-прежнему запрашиваются с `lang=ru`.

### Повседневные задачи (`scripts/project.py`)

Для повседневных операций есть единая точка входа — `scripts/project.py`. Он собирает DBConnect, перегенерирует клиентов из OpenAPI-спецификации, копирует их в рабочий код и запускает тесты:

```powershell
python scripts/project.py doctor            # какие инструменты найдены и актуален ли сгенерированный код
python scripts/project.py setup             # npm install + зависимости генератора
python scripts/project.py generate          # спецификация → Pydantic-модели и C++-клиент → рабочий код
python scripts/project.py generate --check  # проверить, что рабочий код соответствует спецификации
python scripts/project.py sync              # только скопировать сгенерированные файлы
python scripts/project.py build             # сборка DBConnect (аргументы уходят в build.py)
python scripts/project.py deploy            # Release + build_release/dist/
python scripts/project.py test              # тесты core API
python scripts/project.py verify            # generate --check + test
```

Аргументы после `build` и `deploy` передаются в `DBConnect/scripts/build.py` без изменений: `python scripts/project.py build --preset release --jobs 4`. Пути к инструментам можно закрепить в `scripts/project.ini` (в git не попадает, шаблон — `scripts/project.ini.example`); приоритет: аргумент → конфиг → переменная окружения (`BOTC_PYTHON`, `BOTC_NPM`) → автоопределение.

### Сборка DBConnect вне CLion (Windows)

Нужны CMake 3.25+, Ninja, C++ build tools Visual Studio и vcpkg. Установите системную переменную `VCPKG_ROOT`, указывающую на каталог vcpkg. Для Ninja откройте **Developer PowerShell for Visual Studio**, чтобы компилятор `cl.exe` был доступен в `PATH`.

Скрипт `DBConnect/scripts/build.py` сам подготавливает окружение (активирует инструменты Visual Studio, выставляет `VCPKG_ROOT` на отдельную копию vcpkg, выбирает один и тот же CMake) — это рекомендуемый способ сборки вне IDE:

```powershell
python DBConnect/scripts/build.py doctor                # диагностика: тулчейн, vcpkg, ABI
python DBConnect/scripts/build.py                       # configure + build Debug
python DBConnect/scripts/build.py build --preset release
python DBConnect/scripts/build.py deploy                # Release + deploy (build_release/dist/)
python DBConnect/scripts/build.py clean --preset debug  # удалить build_debug, vcpkg_installed не трогает
```

Параметры: `--run` запускает собранный exe, `--jobs N` ограничивает параллелизм, `--dry-run` печатает команду CMake, `--cmake PATH` и `--vcpkg-root PATH` переопределяют автоопределение. Полный список — `python DBConnect/scripts/build.py --help`.

Пути к инструментам для конкретной машины задаются в `DBConnect/scripts/build.ini` (в git не попадает, шаблон — `build.ini.example`): `vs_install`, `cmake`, `vcpkg_root`, `ninja` в секции `[toolchain]`, а также `preset` и `jobs` в `[build]`. Любое значение можно не указывать; приоритет: аргумент командной строки → файл конфига → переменная окружения (`VCPKG_ROOT`) → автоопределение. `build.py doctor` показывает, откуда взялся каждый путь.

Те же шаги вручную:

```powershell
cd DBConnect
cmake --list-presets
cmake --preset debug                    # configure
cmake --build --preset build-debug      # build

# Configure + build:
cmake --workflow --preset debug

# Release deployment (build_release/dist/):
cmake --workflow --preset deploy
```

Shared CMake presets are in `DBConnect/CMakePresets.json`; machine-specific paths are supplied through environment variables, not hard-coded into the repository. Start a new terminal after changing environment variables.

**Не меняйте тулчейн без нужды.** vcpkg включает версию компилятора, триплет и версию CMake, которой собираются порты, в ABI каждого пакета. Обновление CMake (или переключение между CMake из Visual Studio, MSYS2 и CLion) помечает все пакеты устаревшими и заново собирает Qt из исходников. Не удаляйте `vcpkg_installed` ради «чистой» сборки — используйте `build.py clean`, он удаляет только `build_debug`/`build_release`, и давайте прерванной установке vcpkg завершиться.

### Локализация интерфейса DBConnect

Исходный язык строк — английский; русский перевод лежит в `DBConnect/translations/dbconnect_ru.ts`. Каталог собирается `lrelease` и встраивается в исполняемый файл как ресурс `/i18n`, поэтому рядом с `build_release/dist/DBConnect.exe` никакие `.qm` не нужны. Инструменты `lupdate`/`lrelease` приходят из vcpkg-портов `qttools` и `qttranslations` (закреплены на версию Qt в `DBConnect/vcpkg.json`).

```powershell
# обновить .ts после изменения tr()/translate() и собрать .qm
python DBConnect/scripts/build.py translations --update
# только собрать .qm из готового .ts (без lupdate)
python DBConnect/scripts/build.py translations
# другой каталог сборки
python DBConnect/scripts/build.py translations --preset release --update
```

Команда при необходимости сначала конфигурирует выбранный preset. После правки переводов выполните обычную сборку (`python scripts/project.py build` или `deploy`) — именно она встраивает обновлённые `.qm` в exe.

Чтобы добавить язык: добавьте его код в `I18N_TRANSLATED_LANGUAGES` и `TS_FILES` в `DBConnect/CMakeLists.txt`, создайте и переведите новый `.ts`. Селектор языков в приложении строится по встроенным каталогам автоматически. Сообщения валидации CSV и стандартные диалоги Qt переводятся вместе с остальным интерфейсом.

## Project Structure

```
BotC/
├── .gitattributes              # Git line endings config
├── .gitignore
├── README.md                   # This file
├── botc_character_list.csv     # Полный список ролей (EN + RU переводы)
├── test_sample.csv             # Пример CSV для импорта партий
├── test_roles.csv              # Пример CSV для импорта ролей
├── core/                       # Docker stack (DB + API + Nginx + Metabase)
│   ├── docker-compose.yml
│   ├── docker-compose.release.yml
│   ├── init-scripts/           # Инициализация пустой базы
│   ├── migrations/             # Миграции работающей базы (db-migrate)
│   ├── db/                     # Образ БД и runner миграций
│   ├── api/
│   ├── nginx/
│   ├── scripts/
│   └── docs/                   # SSH and API access documentation
├── openapi/                    # Источник контракта API и генерация клиентов
├── scripts/                    # Единая точка входа (project.py)
├── DBConnect/                  # Qt6 GUI importer (games, role list/import, players)
│   ├── src/
│   ├── generated_api/          # Auto-generated OpenAPI C++ client
│   ├── translations/           # Каталоги переводов интерфейса (*.ts)
│   ├── CMakeLists.txt
│   ├── config.ini.example      # Шаблон локальных настроек (без ключа)
│   └── config.ini              # Локальные настройки, не отслеживается Git
```

## Database Schema

### Основные таблицы

| Таблица | Назначение |
|---------|------------|
| `players` | Реестр игроков (уникальное имя, контакты) |
| `roles` | Справочник ролей (английское название, alignment, тип) |
| `role_translations` | Переводы имён ролей на разные языки (ru, en, ...) |
| `role_type_translations` | Переводы типов ролей (Townsfolk → Горожанин и т.д.) |
| `alignment_translations` | Переводы сторон (good → добро, draw → ничья и т.д.) |
| `languages` | Поддерживаемые языки |
| `games` | Метаданные партий (дата, сценарий, место, победившая сторона) |
| `game_storytellers` | Связь партий с одним или несколькими рассказчиками |
| `game_players` | Раскладка партии (связь игрок-роль, сторона в конце игры, выживание) |
| `games_import_staging` | Временные строки импорта CSV (русские названия и значения) |
| `api_keys` | Хранение хешей API-ключей для аутентификации |

### Аналитические представления

| Представление | Данные |
|---------------|--------|
| `v_player_stats` | Статистика игрока: игры, победы, винрейт, выживаемость |
| `v_role_stats` | Эффективность ролей: винрейт, выживаемость, частота смены ролей |
| `v_game_summary` | Сводка по партии: состав, победитель, количество выживших |
| `v_role_type_stats` | Статистика по типу роли и стороне |

### Импорт ролей

Роли и переводы добавляются через `POST /api/roles/import`:

```json
{
  "roles": [{
    "name": "Chambermaid",
    "alignment": "good",
    "role_type": "Outsider",
    "translations": {
      "ru": {"name": "Горничная", "description": "Просто, но не безобидно"}
    }
  }]
}
```

Поле `translations` опционально. Без него импортируется только английская роль. Значения `alignment` для роли: `good`, `evil` или `neutral`; для стороны победителя допустимы `good`, `evil` и `draw`, а сторона игрока в конце партии — только `good` или `evil`. Подробнее: [core/docs/SIDES.md](core/docs/SIDES.md).

## Security

### Гибридная модель доступа

| Метод | Для кого | Порт | Аутентификация |
|-------|----------|------|----------------|
| SSH-туннель | Администратор | 22 | SSH-ключ |
| REST API (HTTPS) | Внешние импортеры | 443 | API-ключ (SHA-256 хеш) |
| Docker network | Внутренние сервисы | — | Прямое подключение |

Порт PostgreSQL опубликован на `127.0.0.1:5432` хоста и недоступен напрямую из внешней сети; администраторы подключаются через SSH-туннель. Внутри Docker база доступна сервисам compose.

### Многопользовательская модель БД

| Пользователь | Права | Назначение |
|--------------|-------|------------|
| `postgres` | Суперпользователь | Администрирование |
| `botc_user` | CRUD (SELECT, INSERT, UPDATE, DELETE) | Приложение |
| `api_service` | Только необходимые API-операции: чтение/создание игроков и партий, запись staging, обновление ролей/переводов, чтение справочников и аналитических представлений | API-сервис импорта |

Точные права по таблицам перечислены в [`core/init-scripts/03-grant-privileges.sql`](core/init-scripts/03-grant-privileges.sql).

## Useful Queries

```sql
-- Топ-5 игроков по винрейту (мин. 5 игр)
SELECT name, games_played, win_rate_pct
FROM v_player_stats
WHERE games_played >= 5
ORDER BY win_rate_pct DESC
LIMIT 5;

-- Топ ролей по винрейту
SELECT role_name, times_played, win_rate_pct
FROM v_role_stats
WHERE times_played >= 3
ORDER BY win_rate_pct DESC;

-- Последняя импортированная партия
SELECT * FROM v_game_summary ORDER BY game_date DESC LIMIT 1;
```

## Backup & Restore

```bash
# Backup
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql

# Restore
docker-compose exec db psql -U postgres botc_stats < backup.sql
```

## Versioning

Проект использует **единую версию** для всех компонентов (monorepo semver). Один тег = гарантированная совместимость.

### Правила

| Тип изменения | Бамп | Пример |
|---|---|---|
| Багфикс в любом компоненте | PATCH | `v3.0.0` → `v3.0.1` |
| Новая фича в любом компоненте | MINOR | `v3.0.1` → `v3.1.0` |
| Breaking change (даже в одном компоненте) | MAJOR | `v3.1.0` → `v4.0.0` |

### Формат релиза

Каждый тег сопровождается описанием, что изменилось в каждом компоненте:

```
v3.1.0
  core:      no changes
  DBConnect: added CSV input validation

v4.0.0
  core:      BREAKING — changed API endpoint to /api/v2/import
  DBConnect: updated to use new /api/v2/import
```

## Releases

Релиз запускается пушем тега `vX.Y.Z` после мержа релизного PR в `main`: workflow собирает
Windows-архив DBConnect, публикует образы `core` в GitHub Container Registry и создаёт
GitHub Release. Прямой пуш в `main` запрещён — изменения принимаются через PR.

```bash
# после мержа PR release/1.2.3 -> main
git switch main
git pull --ff-only
git tag -a v1.2.3 -m "BotC Stats 1.2.3"
git push origin v1.2.3
```

Что попадает в релиз, как поднять версию и как развернуть core из опубликованных образов —
в [docs/RELEASING.md](docs/RELEASING.md).

## License

GNU GPLv3
