# Blood on the Clocktower — Docker Setup (docker-botc)

## 📋 Обзор проекта

**docker-botc** — это Docker-конфигурация для развёртывания PostgreSQL 16 с полной схемой базы данных для учёта партий настольной игры **Blood on the Clocktower** (BotC).

Включает **REST API** для удалённого импорта данных и **SSH-туннель** для администрирования.

### Назначение

- Хранение реестра игроков и ролей
- Учёт партий: дата, сценарий, рассказчик, победившая команда
- Аналитика: винрейты игроков, эффективность ролей, статистика выживаемости
- Гибкий импорт данных через REST API (внешние) или staging-таблицу (админ)

### Технологический стек

| Компонент | Версия |
|-----------|--------|
| PostgreSQL | 16 |
| FastAPI | 0.115+ |
| Nginx | Alpine |
| Docker | 3.8 (Compose file version) |
| OS | Windows (win32) |

### Архитектура сервисов

```
┌─────────────────────────────────────────────────────────┐
│  Сервер                                                  │
│                                                          │
│  ┌──────────┐   ┌──────────┐   ┌──────────────────┐    │
│  │ Nginx    │──▶│ FastAPI  │──▶│   PostgreSQL      │    │
│  │ :443/:80 │   │ :8000    │   │   (только внутри) │    │
│  │ SSL      │   │ asyncpg  │   │   :5432           │    │
│  └──────────┘   └──────────┘   └──────────────────┘    │
│       ▲                              ▲                  │
│       │ HTTPS + API-ключ             │ SSH-туннель      │
│       │                              │                  │
└───────┼──────────────────────────────┼──────────────────┘
        │                              │
  Внешние скрипты              PgAdmin (админ)
  (Python, requests)           (localhost:5432)
```

---

## 📁 Структура файлов

```
docker-botc/
├── docker-compose.yml              # Конфигурация: db + api + nginx
├── .env.example                    # Шаблон переменных окружения
├── .gitignore                      # Git-игноры
├── .dockerignore                   # Docker-игноры
├── README.md                       # Документация
├── init-scripts/
│   ├── 01-schema.sql              # Схема БД: типы, таблицы, представления, функции, api_keys
│   ├── 02-create-user.sh          # Создание пользователя botc_user
│   └── 03-grant-privileges.sql    # Назначение прав (включая api_service)
├── api/
│   ├── app/
│   │   ├── main.py                # FastAPI-приложение, роуты
│   │   ├── auth.py                # Валидация API-ключей
│   │   ├── db.py                  # Подключение к БД (asyncpg)
│   │   ├── schemas.py             # Pydantic-модели
│   │   └── import_logic.py        # Логика импорта
│   ├── Dockerfile
│   └── requirements.txt
├── nginx/
│   ├── nginx.conf                 # Reverse proxy, SSL, rate limiting
│   └── ssl/
│       ├── cert.pem               # Самоподписанный сертификат
│       └── key.pem                # Закрытый ключ
├── scripts/
│   ├── setup-ssh-user.sh          # Скрипт создания SSH-пользователя для туннеля
│   ├── generate-cert.sh           # Генерация self-signed SSL-сертификата
│   └── create-api-key.sh          # Создание/отзыв API-ключей
├── docs/
│   ├── SSH-ACCESS.md              # Подробная документация SSH-доступа
│   └── API-ACCESS.md              # Документация REST API для внешних пользователей
└── QWEN.md                        # Контекст для AI-ассистента (этот файл)
```

---

## 🚀 Быстрый старт

### Требования

- Docker Desktop (или Docker Engine + Compose)
- OpenSSL (для генерации SSL-сертификатов)
- 500 MB свободного места на диске

### Запуск

```bash
# 1. Скопировать .env.example в .env
cp .env.example .env

# 2. Сгенерировать SSL-сертификат
bash scripts/generate-cert.sh

# 3. Запустить контейнеры
docker-compose up -d

# 4. Проверить статус
docker-compose ps
```

### Подключение к базе данных

> **Важно:** Порт 5432 **не проброшен наружу**. Подключение только через SSH-туннель.

```bash
# SSH-туннель (клиентская машина)
ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>

# В другом терминале — подключение к БД
psql -h localhost -p 5432 -U botc_user -d botc_stats

# Администрирование (на сервере)
docker-compose exec db psql -U postgres -d botc_stats
```

📖 **Полная документация:** [docs/SSH-ACCESS.md](docs/SSH-ACCESS.md)

### REST API

```bash
# Проверка работоспособности
curl -k https://<SERVER_IP>/health

# Импорт партии (требуется API-ключ)
curl -k -X POST "https://<SERVER_IP>/api/import" \
  -H "X-API-Key: sk-your-key" \
  -H "Content-Type: application/json" \
  -d '{"game_date": "..."}'
```

📖 **Полная документация:** [docs/API-ACCESS.md](docs/API-ACCESS.md)

### Остановка

```bash
docker-compose down              # Остановить
docker-compose down -v           # Остановить + удалить данные
docker-compose restart           # Перезапустить
```

---

## ⚙️ Конфигурация

### Переменные окружения (.env)

| Переменная | По умолчанию | Описание |
|------------|--------------|----------|
| `POSTGRES_DB` | `botc_stats` | Имя базы данных |
| `POSTGRES_USER` | `postgres` | Администратор БД |
| `POSTGRES_PASSWORD` | `change_me` | Пароль администратора |
| `BOTC_USER_PASSWORD` | `change_me` | Пароль приложения |
| `API_SERVICE_PASSWORD` | `change_me` | Пароль API-сервиса для БД |

---

## 🗄️ Архитектура базы данных

### Типы данных (ENUM)

- **role_type**: `'Горожанин'`, `'Изгой'`, `'Приспешник'`, `'Демон'`, `'Странник'`
- **color**: `'синий'`, `'красный'`, `'нейтральный'`

### Основные таблицы

| Таблица | Назначение |
|---------|------------|
| `players` | Реестр игроков (уникальное имя, контакты) |
| `roles` | Справочник ролей (название, цвет, тип) |
| `games` | Метаданные партий (дата, сценарий, рассказчик, победитель) |
| `game_players` | Раскладка партии (связь игрок-роль, выживание) |
| `games_import_staging` | Временная таблица для CSV-импорта |
| `api_keys` | Хранение хешей API-ключей для аутентификации |

### Аналитические представления

| Представление | Данные |
|---------------|--------|
| `v_player_stats` | Статистика игрока: игры, победы, винрейт, выживаемость |
| `v_role_stats` | Эффективность ролей: винрейт, выживаемость, смены ролей |
| `v_game_summary` | Сводка по партии: состав, победитель, выжившие |
| `v_role_type_stats` | Агрегированная статистика по типам ролей |

### Функции

| Функция | Назначение |
|---------|------------|
| `process_games_import()` | Обработка staging-таблицы → нормализованные таблицы (SECURITY DEFINER) |

### Многопользовательская модель

| Пользователь | Права | Назначение |
|--------------|-------|------------|
| `postgres` | Суперпользователь | Администрирование |
| `botc_user` | CRUD (SELECT, INSERT, UPDATE, DELETE) | Приложение |
| `api_service` | INSERT на игры/players/roles/staging, SELECT на views | API-сервис импорта |

---

## 📥 Импорт данных

### CSV-формат

```csv
game_date,scenario_name,storyteller_name,color_win,player_name,role_start_name,role_end_name,color_end,is_alive
2026-01-15,Вселенная зла,МелонКО,синий,Анна,Дамочка,Дамочка,синий,true
```

### Процесс импорта

```bash
# 1. Загрузить CSV в staging
docker-compose exec db psql -U postgres -d botc_stats \
  -c "\COPY games_import_staging FROM '/path/to/file.csv' DELIMITER ',' CSV HEADER"

# 2. Обработать
docker-compose exec db psql -U postgres -d botc_stats \
  -c "SELECT * FROM process_games_import();"
```

> **Важно:** Все роли должны существовать в таблице `roles` перед импортом. Функция `process_games_import()` выполняет строгую валидацию и откатывает импорт при отсутствии ролей.

---

## 🔐 Безопасность

### Гибридная модель доступа

| Метод | Для кого | Порт | Аутентификация |
|-------|----------|------|----------------|
| SSH-туннель | Администратор (ты) | 22 | SSH-ключ |
| REST API (HTTPS) | Внешние импортеры | 443 | API-ключ (SHA-256 хеш) |
| Docker network | Внутренние сервисы (бот, веб) | — | Прямое подключение |

### SSH-туннель

Порт PostgreSQL **не доступен извне**. Подключение только через SSH-туннель:

```bash
ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>
psql -h localhost -p 5432 -U botc_user -d botc_stats
```

SSH-пользователь `botc-ssh` ограничен:
- Только туннелирование к `localhost:5432`
- Без TTY, без интерактивного shell
- Аутентификация по ключу

### REST API

- Nginx reverse proxy с self-signed SSL-сертификатом
- Rate limiting: 10 запросов/мин на IP
- API-ключи хранятся как SHA-256 хеши в БД
- Валидация всех данных через Pydantic

### Многопользовательская модель БД

- Отдельный пользователь `botc_user` с ограниченными правами (нет CREATE)
- `PUBLIC` не имеет прав CREATE на схеме и базе данных
- Пароли задаются через `.env`, не хардкодятся
- Volume для данных сохраняет данные при перезапуске контейнера
- Init-скрипты выполняются только при первом запуске (пустой volume)

---

## 📊 Полезные запросы

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

-- Роли с наибольшим % смены
SELECT role_name, role_changed_count, times_played,
       ROUND(100.0 * role_changed_count / times_played, 1) AS change_rate_pct
FROM v_role_stats
WHERE times_played >= 5
ORDER BY change_rate_pct DESC;
```

---

## 🛠️ Устранение неполадок

| Проблема | Решение |
|----------|---------|
| Контейнер не запускается | `docker-compose logs` |
| База не инициализируется | `docker-compose down -v && docker-compose up -d` |
| Ошибка подключения | Проверить SSH-туннель: `ssh -L 5432:localhost:5432 botc-ssh@<IP>` |
| API не отвечает | `docker-compose logs api`, `docker-compose restart api` |
| SSL-ошибка API | `bash scripts/generate-cert.sh && docker-compose restart nginx` |
| Импорт не работает | Проверить `SELECT COUNT(*) FROM games_import_staging;` |
| Роль не найдена | Добавить в таблицу `roles` перед импортом |

---

## 🗄️ Резервное копирование

```bash
# Бэкап
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql

# Восстановление
docker-compose exec db psql -U postgres botc_stats < backup.sql
```

---

## 📝 Начальные данные

Схема автоматически заполняет таблицу `roles` шестью ролями при первом запуске:

| Роль | Цвет | Тип |
|------|------|-----|
| Дамочка | синий | Изгой |
| Азартный игрок | синий | Горожанин |
| Убийца | красный | Приспешник |
| Амнезиак | синий | Горожанин |
| Политик | синий | Изгой |
| Пукка | красный | Демон |

---

## ⚠️ Важно для AI-ассистента

- **Язык**: Всегда отвечай на русском языке
- **SQL**: Технические термины (имена таблиц, полей, функции) — на английском
- **Схема**: Не меняй структуру без явного запроса пользователя
- **Нормализация**: При добавлении таблиц следуй принципам 3NF
- **Безопасность**: Не используй `postgres` для операций приложения
- **API**: FastAPI использует asyncpg для подключения к БД внутри Docker network
- **Nginx**: Reverse proxy на порту 443, self-signed SSL-сертификат
- **Rate limiting**: 10 запросов/мин на IP (настроено в Nginx)
