# Blood on the Clocktower — Project

**Blood on the Clocktower (BotC)** — система учёта партий настольной игры Blood on the Clocktower с базой данных PostgreSQL, REST API и набором инструментов для импорта и аналитики.

## Architecture

```
┌─────────────────────────────────────────────┐
│                  BotC Monorepo               │
├──────────────┬──────────────┬───────────────┤
│   core/      │  uploader/   │  (future)     │
│  Docker +    │  CSV → API   │  telegram-bot │
│  PostgreSQL  │  Importer    │  web-ui       │
│  + FastAPI   │              │               │
│  + Nginx     │              │               │
└──────────────┴──────────────┴───────────────┘
         │              │
         ▼              ▼
    PostgreSQL DB ←── POST /api/import
```

## Components

| Компонент | Описание | Документация |
|-----------|----------|-------------|
| **core/** | Docker-стек: PostgreSQL 16, FastAPI, Nginx. База данных + REST API. | [core/README.md](core/README.md) |
| **uploader/** | CLI-утилита для импорта CSV-файлов с партиями через REST API. | [uploader/README.md](uploader/README.md) |

## Quick Start

### 1. Запуск сервера (core)

```bash
cd core
cp .env.example .env
docker-compose up -d
```

### 2. Создание API-ключа

```bash
bash scripts/create-api-key.sh create "Your Name" "contact"
```

### 3. Импорт данных (uploader)

```bash
cd uploader
python -m venv venv && source venv/bin/activate  # Linux/macOS
python -m venv venv && venv\Scripts\activate      # Windows
pip install -r requirements.txt

# Настроить .env — вставить API_KEY
python uploader.py path/to/games.csv
```

## Project Structure

```
BotC/
├── .gitattributes          # Git line endings config
├── .gitignore
├── BotC_Schema.sql         # Standalone DB schema (for reference)
├── README.md               # This file
├── core/                   # Docker stack (DB + API + Nginx)
│   ├── docker-compose.yml
│   ├── init-scripts/
│   ├── api/
│   ├── nginx/
│   └── scripts/
├── uploader/               # CSV importer CLI
│   ├── uploader.py
│   └── requirements.txt
└── test_sample.csv         # Sample data for testing
```

## Versioning

Каждый компонент версионируется отдельно. Теги в monorepo:

- `core-v1.x.x` — версии ядра (БД + API)
- `uploader-v2.x.x` — версии загрузчика

## License

MIT
