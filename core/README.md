# BotC Core — Docker Stack

PostgreSQL 16 + FastAPI + Nginx reverse proxy. Ядро системы Blood on the Clocktower.

> Полная документация проекта: [README.md](../README.md)

## Quick Start

```bash
cp .env.example .env
bash scripts/generate-cert.sh
docker-compose up -d
```

## Architecture

```
┌──────────┐   ┌──────────┐   ┌──────────────┐
│ Nginx    │──▶│ FastAPI  │──▶│ PostgreSQL    │
│ :443/:80 │   │ :8000    │   │ :5432 (inner) │
└──────────┘   └──────────┘   └──────────────┘
     ▲                              ▲
     │ HTTPS + API-ключ             │ SSH-туннель
     ▼                              ▼
  Внешние клиенты            PgAdmin (admin)
```

Порт 5432 **не проброшен наружу**. Доступ только через SSH-туннель или внутри Docker network.

## Configuration

| Переменная | Описание |
|------------|----------|
| `POSTGRES_PASSWORD` | Пароль администратора БД |
| `BOTC_USER_PASSWORD` | Пароль приложения (ограниченные права) |
| `API_SERVICE_PASSWORD` | Пароль API-сервиса |

## API Endpoints

| Endpoint | Описание |
|----------|----------|
| `GET /health` | Проверка работоспособности |
| `POST /api/import` | Импорт партии (роли на русском) |
| `POST /api/roles/import` | Импорт ролей с переводами |
| `GET /api/roles` | Список доступных ролей |

Подробная документация: [docs/API-ACCESS.md](docs/API-ACCESS.md)

## Key Scripts

| Скрипт | Назначение |
|--------|------------|
| `scripts/create-api-key.sh` | Создать / отозвать API-ключ |
| `scripts/generate-cert.sh` | Сгенерировать self-signed SSL-сертификат |
| `scripts/setup-ssh-user.sh` | Создать SSH-пользователя для туннеля |

## Documentation

- **SSH access:** [docs/SSH-ACCESS.md](docs/SSH-ACCESS.md)
- **REST API:** [docs/API-ACCESS.md](docs/API-ACCESS.md)

## Management

```bash
docker-compose ps          # status
docker-compose logs -f     # follow logs
docker-compose down        # stop
docker-compose down -v     # stop + remove data
```

## Running Tests

```bash
# Перейти в директорию API
cd api

# Установить зависимости (включая pytest, pytest-asyncio, httpx)
pip install -r requirements.txt

# Запустить все тесты
python -m pytest tests/ -v

# Краткий вывод
python -m pytest tests/ -v --tb=short

# С отчётом о покрытии (нужен pytest-cov)
pip install pytest-cov
python -m pytest tests/ -v --cov=app --cov-report=term-missing
```

Тесты покрывают: эндпоинты, аутентификацию, логику импорта, Pydantic-схемы и работу с БД (asyncpg.Pool).

## Backup

```bash
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql
```
