# BotC Uploader

Python-скрипт для загрузки данных игр в базу данных **Blood on the Clocktower** через REST API.

## Описание

Скрипт читает CSV-файл с данными игр, парсит его и отправляет каждую партию на импорт через REST API (`POST /api/import`). Это позволяет импортировать данные без прямого подключения к PostgreSQL.

## Требования

- Python 3.10+
- Запущенный Docker-контейнер с BotC API
- Валидный API-ключ (создаётся через `scripts/create-api-key.sh`)

## Установка

```bash
# 1. Создать виртуальное окружение
python -m venv venv

# 2. Активировать (Windows)
venv\Scripts\activate

# 3. Установить зависимости
pip install -r requirements.txt
```

## Настройка

```bash
# Скопировать шаблон конфигурации
copy .env.example .env
```

Отредактировать `.env` и указать параметры API:

```env
API_URL=https://localhost:443
API_KEY=sk-your-api-key-here
SSL_VERIFY=false
```

### Получение API-ключа

API-ключ создаётся на сервере с помощью скрипта:

```bash
cd docker-botc
bash scripts/create-api-key.sh create "Ваше Имя"
```

Скрипт выведет ключ в формате `sk-xxxxx`. Скопируйте его в `.env` файл.

## Использование

```bash
python uploader.py <путь_к_csv_файлу>
```

### Пример

```bash
python uploader.py ../test_sample.csv
```

### Формат CSV

CSV-файл должен содержать заголовок и следующие колонки:

| Колонка | Описание |
|---------|----------|
| `game_date` | Дата игры (YYYY-MM-DD) |
| `scenario_name` | Название сценария |
| `storyteller_name` | Имя рассказчика |
| `color_win` | Победившая команда (синий/красный) |
| `player_name` | Имя игрока |
| `role_start_name` | Начальная роль |
| `role_end_name` | Конечная роль |
| `color_end` | Команда игрока в конце (синий/красный) |
| `is_alive` | Выжил ли игрок (true/false) |

Пример:
```csv
game_date,scenario_name,storyteller_name,color_win,player_name,role_start_name,role_end_name,color_end,is_alive
2026-01-15,Вселенная зла,МелонКО,синий,Анна Никитина,Дамочка,Дамочка,синий,true
```

> **Важно:** Один CSV-файл может содержать несколько партий. Скрипт автоматически группирует строки по уникальной комбинации `(game_date, scenario_name, storyteller_name, color_win)` и отправляет каждую партию отдельным запросом.

## Вывод

После импорта скрипт выводит:
- Количество успешно импортированных партий
- Количество созданных игроков
- ID созданных партий (если доступны)

При ошибке (например, несуществующая роль или неверный API-ключ) выводится сообщение об ошибке.

## Структура проекта

```
botc-uploader/
├── .env.example          # Шаблон конфигурации
├── .gitignore
├── requirements.txt      # Зависимости Python (requests, python-dotenv)
├── uploader.py           # Основной скрипт загрузки
└── README.md             # Документация
```

## Архитектура работы

```
CSV файл → Парсинг → Группировка по партиям → HTTP POST /api/import → БД
                                              ↑
                                        X-API-Key header
```

1. **Парсинг CSV** — чтение файла, извлечение данных
2. **Группировка** — объединение строк по партиям
3. **HTTP-запрос** — отправка JSON на API endpoint
4. **Обработка на сервере** — API валидирует роли, вставляет в staging, вызывает `process_games_import()`
5. **Результат** — возврат статуса, ID партии, количество созданных игроков

## Устранение неполадок

| Проблема | Решение |
|----------|---------|
| `.env` не найден | Скопируйте `.env.example` в `.env` |
| Ошибка подключения | Проверьте, что контейнер запущен (`docker-compose ps`) |
| `Invalid API key` | Убедитесь, что API-ключ в `.env` совпадает с созданным через скрипт |
| `API key has been revoked` | Ключ отозван — создайте новый: `bash scripts/create-api-key.sh create "Имя"` |
| SSL ошибка | Установите `SSL_VERIFY=false` в `.env` (для self-signed сертификатов) |
| Отсутствуют роли | Добавьте роли в таблицу `roles` перед импортом |
| Модуль не найден | Активируйте виртуальное окружение: `venv\Scripts\activate` |
| Зависимости не установлены | Запустите `pip install -r requirements.txt` |

## См. также

- [Основная документация BotC](../README.md)
- [Схема базы данных](../BotC_Schema.sql)
- [Документация API](../docker-botc/docs/API-ACCESS.md)
