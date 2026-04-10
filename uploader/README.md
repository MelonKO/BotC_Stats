# BotC Uploader — CSV Importer

CLI-утилита для загрузки CSV-файлов с партиями и ролями через REST API.

> Полная документация проекта: [README.md](../README.md)

## Quick Start

```bash
python -m venv venv
venv\Scripts\activate        # Windows
pip install -r requirements.txt

cp .env.example .env
# Отредактировать .env — вставить API_KEY

# Импорт партии
python uploader.py path/to/games.csv

# Импорт ролей с переводами
python uploader.py --roles path/to/roles.csv
```

## Configuration

| Переменная | Описание |
|------------|----------|
| `API_URL` | URL API сервера (например, `https://localhost:443`) |
| `API_KEY` | API-ключ, созданный через `core/scripts/create-api-key.sh` |
| `SSL_VERIFY` | `false` для self-signed сертификатов |

## Режимы работы

### 1. Импорт партий (по умолчанию)

CSV-файл с данными партий. Один файл может содержать несколько партий — скрипт автоматически группирует по `(game_date, scenario_name, location, game_number, storyteller_name, alignment_win)`.

**Формат CSV:**
```csv
game_date,scenario_name,location,game_number,storyteller_name,alignment_win,player_name,role_start_name,role_end_name,alignment_end,is_alive,seat_number,duration,notes
2026-01-15,Вселенная зла,"Москва, Антикафе на Арбате",1,МелонКО,добро,Мая Вишневская,Дамочка,Дамочка,добро,true,1,01:30:00,"Отличная партия"
```

Роли указываются **русскими** названиями (`Дамочка`, `Убийца`) — сервер мапит их в английские через `role_translations`.

### 2. Импорт ролей (`--roles`)

CSV-файл с ролями и опциональными переводами.

**Формат CSV:**
```csv
name,alignment,role_type,description,ru_name,ru_description
Chambermaid,good,Outsider,"Simple, but not harmless",Горничная,"Просто, но не безобидно"
Imp,evil,Demon,"Each night*, choose a player: they die",Имп,"Каждую ночь* выбирайте игрока: он умирает"
```

| Колонка | Описание |
|---------|----------|
| `name` | Английское имя роли (обязательно) |
| `alignment` | `good`, `evil` или `neutral` |
| `role_type` | `Townsfolk`, `Outsider`, `Minion`, `Demon` или `Traveller` |
| `description` | Описание на английском (опционально) |
| `{lang}_name` | Имя роли на языке (например, `ru_name`) |
| `{lang}_description` | Описание на языке (опционально) |

Языки определяются автоматически из заголовков CSV (`ru_name`, `de_name` и т.д.).

## Architecture

```
CSV → Parse → Group by game → POST /api/import (X-API-Key) → DB
CSV (--roles) → Parse → POST /api/roles/import (X-API-Key) → DB
```

## Testing

```bash
# Установить зависимости (включая pytest, pytest-mock)
pip install -r requirements.txt

# Запустить все тесты
python -m pytest tests/ -v

# Краткий вывод
python -m pytest tests/ -v --tb=short

# С отчётом о покрытии (нужен pytest-cov)
pip install pytest-cov
python -m pytest tests/ -v --cov=uploader --cov-report=term-missing
```

## Troubleshooting

| Проблема | Решение |
|----------|---------|
| `.env` не найден | `cp .env.example .env` |
| `Invalid API key` | Проверьте ключ в `.env`, создайте новый при необходимости |
| SSL ошибка | Установите `SSL_VERIFY=false` |
| Модуль не найден | `venv\Scripts\activate && pip install -r requirements.txt` |
| Отсутствуют роли | Добавьте роли через `python uploader.py --roles roles.csv` |
