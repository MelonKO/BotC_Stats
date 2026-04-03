# BotC Uploader — CSV Importer

CLI-утилита для загрузки CSV-файлов с партиями через REST API.

> Полная документация проекта: [README.md](../README.md)

## Quick Start

```bash
python -m venv venv
venv\Scripts\activate        # Windows
pip install -r requirements.txt

cp .env.example .env
# Отредактировать .env — вставить API_KEY

python uploader.py path/to/games.csv
```

## Configuration

| Переменная | Описание |
|------------|----------|
| `API_URL` | URL API сервера (например, `https://localhost:443`) |
| `API_KEY` | API-ключ, созданный через `core/scripts/create-api-key.sh` |
| `SSL_VERIFY` | `false` для self-signed сертификатов |

## CSV Format

```csv
game_date,scenario_name,storyteller_name,color_win,player_name,role_start_name,role_end_name,color_end,is_alive
2026-01-15,Вселенная зла,МелонКО,синий,Анна Никитина,Дамочка,Дамочка,синий,true
```

Один файл может содержать несколько партий — скрипт автоматически группирует по `(game_date, scenario_name, storyteller_name, color_win)`.

## Architecture

```
CSV → Parse → Group by game → POST /api/import (X-API-Key) → DB
```

## Troubleshooting

| Проблема | Решение |
|----------|---------|
| `.env` не найден | `cp .env.example .env` |
| `Invalid API key` | Проверьте ключ в `.env`, создайте новый при необходимости |
| SSL ошибка | Установите `SSL_VERIFY=false` |
| Модуль не найден | `venv\Scripts\activate && pip install -r requirements.txt` |
