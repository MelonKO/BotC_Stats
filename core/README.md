# BotC Core — Docker Stack

PostgreSQL 16 + FastAPI + Nginx reverse proxy + Metabase analytics. Ядро системы Blood on the Clocktower.

> Полная документация проекта: [README.md](../README.md)

## Quick Start

```bash
cp .env.example .env
# Edit .env: replace default passwords/secrets and set SERVER_IP.
bash scripts/generate-cert.sh
docker-compose up -d
```

## Architecture

```
Внешние клиенты ──HTTPS──▶ Nginx (:80/:443)
                              ├── /api/*   ──▶ FastAPI (:8000) ──▶ PostgreSQL (:5432)
                              ├── /stats/* ──▶ Metabase (:3000) ──▶ PostgreSQL
                              └── /view/*  ──▶ Публичная embed-страница

Администратор ──SSH-туннель──▶ localhost:5432 на хосте ──▶ PostgreSQL
```

Порт 5432 опубликован только на `127.0.0.1` хоста, а не на внешних интерфейсах; администраторы подключаются через SSH-туннель. API и Metabase доступны через Nginx.

## Configuration

| Переменная | Описание |
|------------|----------|
| `POSTGRES_PASSWORD` | Пароль администратора БД |
| `BOTC_USER_PASSWORD` | Пароль приложения (ограниченные права) |
| `API_SERVICE_PASSWORD` | Пароль API-сервиса |
| `METABASE_IMAGE` | Версия образа Metabase |
| `METABASE_META_PASSWORD` | Пароль пользователя служебной БД Metabase |
| `METABASE_READONLY_PASSWORD` | Пароль пользователя чтения аналитики |
| `METABASE_EMBEDDING_SECRET` | Секрет для подписанных embed-токенов |
| `SERVER_IP` | Адрес, используемый в URL Metabase |

## API Endpoints

| Endpoint | Описание |
|----------|----------|
| `GET /health` | Проверка работоспособности |
| `POST /api/games/import` | Импорт одной партии (роли на русском) |
| `POST /api/games/import_batch` | Импорт нескольких партий |
| `POST /api/roles/import` | Импорт ролей с переводами |
| `GET /api/roles` | Список доступных ролей (опциональный `lang`) |
| `GET /api/players` | Список игроков |

Подробная документация: [docs/API-ACCESS.md](docs/API-ACCESS.md)

## Key Scripts

| Скрипт | Назначение |
|--------|------------|
| `scripts/create-api-key.sh` | Создать / отозвать API-ключ |
| `scripts/generate-cert.sh` | Сгенерировать self-signed SSL-сертификат |
| `scripts/setup-ssh-user.sh` | Создать ограниченного SSH-пользователя для туннеля |
| `nginx/nginx.conf` | HTTPS reverse proxy, rate limits и Metabase routes |

## Documentation

- **SSH access:** [docs/SSH-ACCESS.md](docs/SSH-ACCESS.md)
- **REST API:** [docs/API-ACCESS.md](docs/API-ACCESS.md)
- **Metabase:** `https://<SERVER_IP>/stats/`; публичные embed-страницы размещаются под `/view/`.

## Management

```bash
docker-compose ps          # status
docker-compose logs -f     # follow logs
docker-compose down        # stop
docker-compose down -v     # stop + remove data
```

## Running Tests

```bash
# Из каталога core/ перейти в директорию API
cd api

# Установить зависимости (включая pytest, pytest-asyncio, httpx)
pip install -r requirements.txt

# Запустить все тесты
python -m pytest app/tests/ -v

# Краткий вывод
python -m pytest app/tests/ -v --tb=short

# С отчётом о покрытии (нужен pytest-cov)
pip install pytest-cov
python -m pytest app/tests/ -v --cov=app --cov-report=term-missing
```

Тесты покрывают: эндпоинты, аутентификацию, логику импорта, Pydantic-схемы и работу с БД (asyncpg.Pool).

## Backup

```bash
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql
```
