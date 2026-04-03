# Blood on the Clocktower — Docker Setup

> Docker-контейнер с PostgreSQL для базы данных игры **Blood on the Clocktower**.
> Включает REST API для удалённого импорта данных и SSH-туннель для администрирования.

---

## 🚀 Быстрый старт

### Требования
- Docker Desktop (или Docker Engine + Compose)
- OpenSSL (для генерации SSL-сертификатов)
- 500 MB свободного места на диске

### Запуск

```bash
# 1. Скопировать .env.example в .env и настроить переменные
cp .env.example .env

# 2. Сгенерировать SSL-сертификат для API
bash scripts/generate-cert.sh

# 3. Запустить контейнеры в фоновом режиме
docker-compose up -d

# 4. Проверить статус
docker-compose ps

# 5. Проверить логи
docker-compose logs db
```

### Подключение к базе данных

> **Важно:** Порт 5432 **не проброшен наружу**. Подключение только через SSH-туннель.

```bash
# 1. Создать SSH-туннель (на клиентской машине)
ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>

# 2. В ДРУГОМ ТЕРМИНАЛЕ — подключиться к БД
psql -h localhost -p 5432 -U botc_user -d botc_stats

# Администрирование (на сервере)
docker-compose exec db psql -U postgres -d botc_stats
```

📖 **Полная документация SSH-доступа:** [docs/SSH-ACCESS.md](docs/SSH-ACCESS.md)

### REST API для внешних импортеров

API доступен по HTTPS на порту 443:

```bash
# Проверка работоспособности
curl -k https://<SERVER_IP>/health

# Импорт партии (требуется API-ключ)
curl -k -X POST "https://<SERVER_IP>/api/import" \
  -H "X-API-Key: sk-your-key" \
  -H "Content-Type: application/json" \
  -d '{"game_date": "..."}'
```

📖 **Полная документация API:** [docs/API-ACCESS.md](docs/API-ACCESS.md)

---

## 📁 Структура проекта

```
docker-botc/
├── docker-compose.yml          # Конфигурация: db + api + nginx
├── .env.example                # Пример переменных окружения
├── .gitignore                  # Git-игноры
├── .dockerignore               # Docker-игноры
├── init-scripts/
│   ├── 01-schema.sql          # Схема БД (автоматически загружается)
│   ├── 02-create-user.sh      # Создание пользователя botc_user
│   └── 03-grant-privileges.sql # Назначение прав приложению
├── api/                        # FastAPI-сервис для импорта
│   ├── app/
│   │   ├── main.py             # Роуты и приложение
│   │   ├── auth.py             # Валидация API-ключей
│   │   ├── db.py               # Подключение к БД (asyncpg)
│   │   ├── schemas.py          # Pydantic-модели
│   │   └── import_logic.py     # Логика импорта
│   ├── Dockerfile
│   └── requirements.txt
├── nginx/
│   ├── nginx.conf              # Reverse proxy + SSL
│   └── ssl/                    # SSL-сертификаты (генерируются)
│       ├── cert.pem
│       └── key.pem
├── scripts/
│   ├── setup-ssh-user.sh       # Создание SSH-пользователя для туннеля
│   ├── generate-cert.sh        # Генерация self-signed SSL-сертификата
│   └── create-api-key.sh       # Создание/отзыв API-ключей
├── docs/
│   ├── SSH-ACCESS.md           # Документация SSH-доступа
│   └── API-ACCESS.md           # Документация REST API
└── README.md                   # Документация
```

---

## ⚙️ Конфигурация

### Переменные окружения (.env)

| Переменная | По умолчанию | Описание |
|------------|--------------|----------|
| `POSTGRES_DB` | `botc_stats` | Имя базы данных |
| `POSTGRES_USER` | `postgres` | Администратор (суперпользователь) |
| `POSTGRES_PASSWORD` | `change_me` | Пароль администратора |
| `BOTC_USER_PASSWORD` | `change_me` | Пароль приложения (ограниченные права) |
| `API_SERVICE_PASSWORD` | `change_me` | Пароль API-сервиса для подключения к БД |

### Пример `.env`

```env
POSTGRES_DB=botc_stats
POSTGRES_USER=postgres
POSTGRES_PASSWORD=your_secure_admin_password
BOTC_USER_PASSWORD=your_secure_app_password
API_SERVICE_PASSWORD=your_secure_api_password
```

---

## 🔐 Безопасность

### Гибридная модель доступа

Проект использует **два метода доступа** для разных сценариев:

| Метод | Для кого | Порт | Аутентификация |
|-------|----------|------|----------------|
| **SSH-туннель** | Администратор (ты) | 22 | SSH-ключ |
| **REST API (HTTPS)** | Внешние импортеры | 443 | API-ключ |
| **Docker network** | Внутренние сервисы (бот, веб) | — | Прямое подключение |

Порт PostgreSQL **не проброшен наружу** — доступен только внутри Docker network.

### SSH-туннель для администрирования

```bash
ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>
psql -h localhost -p 5432 -U botc_user -d botc_stats
```

📖 **Полная инструкция:** [docs/SSH-ACCESS.md](docs/SSH-ACCESS.md)

### REST API для внешних импортеров

```bash
# Создание API-ключа
bash scripts/create-api-key.sh create "Имя Фамилия"

# Использование
curl -k -X POST "https://<SERVER_IP>/api/import" \
  -H "X-API-Key: sk-your-key" \
  -H "Content-Type: application/json" \
  -d '{...}'
```

📖 **Полная документация:** [docs/API-ACCESS.md](docs/API-ACCESS.md)

### Многопользовательская архитектура

По умолчанию контейнер настроен с **ограниченными правами**:

| Пользователь | Права | Назначение |
|--------------|-------|------------|
| `postgres` | Суперпользователь | Администрирование БД |
| `botc_user` | CRUD только | Приложение (SELECT, INSERT, UPDATE, DELETE) |
| `api_service` | INSERT на игры/players/roles/staging, SELECT на views | API-сервис импорта |

### Как это работает

1. При первом запуске PostgreSQL выполняет скрипты из `init-scripts/` в алфавитном порядке:
   - `01-schema.sql` — создает таблицы, представления, функции
   - `02-create-user.sh` — создает пользователя `botc_user` с паролем из переменной окружения

2. Приложение должно подключаться через `botc_user`, а не `postgres`

### Подключение приложения

```bash
# Через psql с ограниченным пользователем
docker-compose exec db psql -U botc_user -d botc_stats

# Подключение из Docker-контейнера приложения
PGHOST=db PGPORT=5432 PGUSER=botc_user PGPASSWORD=${BOTC_USER_PASSWORD} PGDATABASE=botc_stats
```

### Рекомендации по безопасности

✅ **Измените пароли в `.env` перед использованием:**
- `POSTGRES_PASSWORD` — сильный пароль администратора (16+ символов)
- `BOTC_USER_PASSWORD` — отдельный пароль для приложения

✅ **Не используйте `postgres` в приложении** — только для администрирования

✅ **Регулярно обновляйте образ PostgreSQL** для получения патчей безопасности

✅ **Используйте volume для данных** — они сохраняются при перезапуске контейнера

---

## 📊 Управление

### Сервисы

| Сервис | Контейнер | Назначение |
|--------|-----------|------------|
| PostgreSQL | `botc-postgres` | База данных |
| FastAPI | `botc-api` | REST API для импорта |
| Nginx | `botc-nginx` | Reverse proxy + SSL |

### Остановка контейнеров

```bash
docker-compose down
```

### Остановка с удалением volumes (данных)

```bash
docker-compose down -v
```

### Перезапуск

```bash
docker-compose restart
```

### Просмотр логов

```bash
# Все логи
docker-compose logs

# Логи в реальном времени
docker-compose logs -f

# Логи за последнюю минуту
docker-compose logs --tail=1m
```

---

## 🗄️ Импорт данных

### Способ 1: REST API (рекомендуется для внешних пользователей)

```bash
# Создать API-ключ
bash scripts/create-api-key.sh create "Имя Фамилия"

# Импортировать партию
curl -k -X POST "https://<SERVER_IP>/api/import" \
  -H "X-API-Key: sk-your-key" \
  -H "Content-Type: application/json" \
  -d '{"game_date": "2026-01-15", "scenario_name": "...", ...}'
```

📖 **Полная документация:** [docs/API-ACCESS.md](docs/API-ACCESS.md)

### Способ 2: Прямая вставка через psql (администратор)

```bash
docker-compose exec db psql -U postgres -d botc_stats -f /docker-entrypoint-initdb.d/01-schema.sql
```

### Способ 3: Импорт через staging-таблицу (администратор)

```bash
# 1. Загрузить CSV в staging-таблицу
docker-compose exec db psql -U postgres -d botc_stats -c "\COPY games_import_staging FROM '/path/to/file.csv' DELIMITER ',' CSV HEADER"

# 2. Запустить обработку
docker-compose exec db psql -U postgres -d botc_stats -c "SELECT * FROM process_games_import();"
```

---

## 🔍 Аналитика

### Топ-5 игроков по винрейту

```bash
docker-compose exec db psql -U postgres -d botc_stats -c "SELECT name, games_played, win_rate_pct FROM v_player_stats WHERE games_played >= 5 ORDER BY win_rate_pct DESC LIMIT 5;"
```

### Статистика по ролям

```bash
docker-compose exec db psql -U postgres -d botc_stats -c "SELECT role_name, times_played, win_rate_pct FROM v_role_stats ORDER BY times_played DESC;"
```

---

## 🛠️ Устранение неполадок

### Контейнер не запускается

```bash
# Проверить логи всех сервисов
docker-compose logs

# Логи конкретного сервиса
docker-compose logs api
docker-compose logs nginx
```

### API не отвечает

```bash
# Проверить статус API
docker-compose exec api python -c "import urllib.request; print(urllib.request.urlopen('http://localhost:8000/health').read().decode())"

# Перезапустить API
docker-compose restart api
```

### SSL-ошибка при подключении к API

```bash
# Проверить, что сертификаты сгенерированы
ls -la nginx/ssl/

# Перегенерировать
bash scripts/generate-cert.sh
docker-compose restart nginx
```

### База данных не инициализируется

```bash
# Удалить volume и пересоздать
docker-compose down -v
docker-compose up -d
```

### Ошибка подключения

```bash
# Убедиться, что контейнер запущен
docker-compose ps

# Проверить SSH-туннель (должен быть запущен на клиенте)
ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>
```

---

## 📦 Резервное копирование

```bash
# Создать бэкап
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql

# Восстановить
docker-compose exec db psql -U postgres botc_stats < backup.sql
```

---

## 📄 Лицензия

MIT — используйте, модифицируйте, делитесь.

---

> 💡 **Совет**: Перед запуском измените пароли в `.env` и сгенерируйте SSL-сертификат: `bash scripts/generate-cert.sh`
