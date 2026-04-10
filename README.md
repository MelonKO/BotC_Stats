# Blood on the Clocktower Statistics — Project

**Blood on the Clocktower Statistics (BotC Stats)** — система учёта партий настольной игры Blood on the Clocktower с базой данных PostgreSQL, REST API и набором инструментов для импорта и аналитики.

## Architecture

```
┌───────────────────────────────────────────────────────────┐
│                    BotC Monorepo                           │
├──────────────┬──────────────┬─────────────────────────────┤
│   core/      │  uploader/   │  (future)                   │
│  Docker +    │  CSV → API   │  telegram-bot, web-ui, ...  │
│  PostgreSQL  │  Importer    │                             │
│  + FastAPI   │              │                             │
│  + Nginx     │              │                             │
└──────┬───────┴──────┬───────┴─────────────────────────────┘
       │              │
       ▼              ▼
  PostgreSQL DB ←── POST /api/import
```

## Components

| Компонент | Описание | Подробности |
|-----------|----------|-------------|
| **[core/](core/)** | Docker-стек: PostgreSQL 16, FastAPI, Nginx. База данных + REST API. | [core/README.md](core/README.md) |
| **[uploader/](uploader/)** | CLI-утилита для импорта CSV-файлов с партиями через REST API. | [uploader/README.md](uploader/README.md) |

## Quick Start

### 1. Запуск сервера (core)

```bash
cd core
cp .env.example .env
bash scripts/generate-cert.sh
docker-compose up -d
```

### 2. Создание API-ключа

```bash
bash scripts/create-api-key.sh create "Your Name" "contact"
```

### 3. Импорт данных (uploader)

```bash
cd uploader
python -m venv venv
venv\Scripts\activate        # Windows
pip install -r requirements.txt

# Настроить .env — вставить API_KEY
python uploader.py path/to/games.csv
```

## Project Structure

```
BotC/
├── .gitattributes              # Git line endings config
├── .gitignore
├── README.md                   # This file
├── botc_character_list.csv     # Полный список ролей (EN + RU переводы)
├── test_sample.csv             # Пример CSV для импорта партий
├── test_roles.csv              # Пример CSV для импорта ролей
├── core/                       # Docker stack (DB + API + Nginx)
│   ├── docker-compose.yml
│   ├── init-scripts/
│   ├── api/
│   ├── nginx/
│   ├── scripts/
│   └── docs/                   # SSH and API access documentation
├── uploader/                   # CSV importer CLI
│   ├── uploader.py
│   └── requirements.txt
```

## Database Schema

### Основные таблицы

| Таблица | Назначение |
|---------|------------|
| `players` | Реестр игроков (уникальное имя, контакты) |
| `roles` | Справочник ролей (английское название, alignment, тип) |
| `role_translations` | Переводы имён ролей на разные языки (ru, en, ...) |
| `role_type_translations` | Переводы типов ролей (Townsfolk → Горожанин и т.д.) |
| `alignment_translations` | Переводы align-ментов (good → добро и т.д.) |
| `languages` | Поддерживаемые языки |
| `games` | Метаданные партий (дата, сценарий, рассказчик, победитель) |
| `game_players` | Раскладка партии (связь игрок-роль, выживание) |
| `api_keys` | Хранение хешей API-ключей для аутентификации |

### Аналитические представления

| Представление | Данные |
|---------------|--------|
| `v_player_stats` | Статистика игрока: игры, победы, винрейт, выживаемость |
| `v_role_stats` | Эффективность ролей: винрейт, выживаемость, частота смены ролей |
| `v_game_summary` | Сводка по партии: состав, победитель, количество выживших |

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

Поле `translations` опционально. Без него импортируется только английская роль.

## Security

### Гибридная модель доступа

| Метод | Для кого | Порт | Аутентификация |
|-------|----------|------|----------------|
| SSH-туннель | Администратор | 22 | SSH-ключ |
| REST API (HTTPS) | Внешние импортеры | 443 | API-ключ (SHA-256 хеш) |
| Docker network | Внутренние сервисы | — | Прямое подключение |

Порт PostgreSQL **не проброшен наружу** — доступен только внутри Docker network.

### Многопользовательская модель БД

| Пользователь | Права | Назначение |
|--------------|-------|------------|
| `postgres` | Суперпользователь | Администрирование |
| `botc_user` | CRUD (SELECT, INSERT, UPDATE, DELETE) | Приложение |
| `api_service` | INSERT/UPDATE/SELECT на игры/players/roles/staging/role_translations, SELECT на views | API-сервис импорта |

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
  uploader:  added CSV input validation

v4.0.0
  core:      BREAKING — changed API endpoint to /api/v2/import
  uploader:  updated to use new /api/v2/import
```

## License

GNU GPLv3
