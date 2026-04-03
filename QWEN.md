# Blood on the Clocktower — Project Context

## 📋 Project Overview

**Blood on the Clocktower (BotC)** — это система учёта партий настольной игры Blood on the Clocktower с базой данных PostgreSQL. Проект состоит из двух основных компонентов:

1. **BotC_Schema.sql** — PostgreSQL схема для хранения данных об играх, игроках, ролях и статистике
2. **docker-botc/** — Docker-контейнер с PostgreSQL для быстрого развёртывания

### Основные функции

- ✅ Учёт партий: дата, сценарий, рассказчик, победившая команда
- ✅ Хранение раскладки: кто какую роль получил, выжил ли игрок
- ✅ Аналитика: автоматический расчёт винрейта, эффективности ролей
- ✅ Гибкий импорт: поддержка прямой вставки и пакетного импорта из CSV/Excel

### Технологический стек

| Компонент | Версия/Значение |
|-----------|-----------------|
| **База данных** | PostgreSQL 16 |
| **Контейнеризация** | Docker + Docker Compose |
| **Язык SQL** | PL/pgSQL (функции, представления) |
| **Кодировка** | UTF-8 (поддержка кириллицы) |
| **Расширения** | pgcrypto (gen_random_uuid()) |

## 🏗️ Архитектура базы данных

### Типы данных (ENUM)

```sql
role_type: 'Горожанин' | 'Изгой' | 'Приспешник' | 'Демон' | 'Странник'
color: 'синий' | 'красный' | 'нейтральный'
```

### Основные таблицы

| Таблица | Назначение |
|---------|------------|
| `players` | Реестр игроков (уникальное имя, контакты) |
| `roles` | Справочник ролей (название, цвет, тип) |
| `games` | Метаданные партий (дата, сценарий, рассказчик, победитель) |
| `game_players` | Раскладка партии (связь игрок-роль, выживание) |
| `games_import_staging` | Временная таблица для импорта CSV |

### Представления для аналитики

| Представление | Данные |
|---------------|--------|
| `v_player_stats` | Статистика игрока: игры, победы, винрейт, выживаемость |
| `v_role_stats` | Эффективность ролей: винрейт, выживаемость, частота смены ролей |
| `v_game_summary` | Сводка по партии: состав, победитель, количество выживших |
| `v_role_type_stats` | Агрегированная статистика по типам ролей |

### Многопользовательская архитектура

| Пользователь | Права | Назначение |
|--------------|-------|------------|
| `postgres` | Суперпользователь | Администрирование БД |
| `botc_user` | CRUD только | Приложение (SELECT, INSERT, UPDATE, DELETE) |

## 📁 Структура проекта

```
BotC/
├── BotC_Schema.sql              # Основная схема БД
├── README.md                    # Документация схемы
├── docker-botc/                 # Docker-конфигурация
│   ├── docker-compose.yml       # Конфигурация контейнера
│   ├── .env.example             # Пример переменных окружения
│   ├── .gitignore
│   ├── .dockerignore
│   ├── init-scripts/
│   │   ├── 01-schema.sql       # Схема БД (автоматически загружается при первом запуске)
│   │   ├── 02-create-user.sql  # Создание пользователя botc_user
│   │   └── 03-grant-privileges.sql  # Назначение прав приложению
│   └── README.md               # Документация Docker-развёртывания
```

## 🚀 Быстрый старт

### Запуск через Docker

```bash
cd docker-botc

# 1. Скопировать .env.example в .env и настроить переменные
cp .env.example .env

# 2. Запустить контейнер в фоновом режиме
docker-compose up -d

# 3. Проверить статус
docker-compose ps

# 4. Проверить логи
docker-compose logs db
```

### Подключение к базе данных

```bash
# Через psql внутри контейнера (администратор)
docker-compose exec db psql -U postgres -d botc_stats

# Через psql внутри контейнера (приложение)
docker-compose exec db psql -U botc_user -d botc_stats

# Через внешний клиент (localhost:5432)
psql -h localhost -p 5432 -U postgres -d botc_stats
```

### Управление контейнером

```bash
docker-compose down              # Остановить контейнер
docker-compose up -d             # Запустить
docker-compose restart           # Перезапустить
docker-compose down -v           # Удалить данные (volume)
docker-compose logs -f           # Логи в реальном времени
```

## 📥 Импорт данных

### Структура CSV-файла

```csv
game_date,scenario_name,storyteller_name,color_win,player_name,role_start_name,role_end_name,color_end,is_alive
2026-01-15,Вселенная зла,МелонКО,синий,Анна Никитина,Дамочка,Дамочка,синий,true
2026-01-15,Вселенная зла,МелонКО,синий,Борис Петров,Азартный игрок,Азартный игрок,синий,true
```

### Процесс импорта

```bash
# 1. Загрузить CSV в staging-таблицу (через psql)
docker-compose exec db psql -U postgres -d botc_stats -c "\COPY games_import_staging FROM '/path/to/file.csv' DELIMITER ',' CSV HEADER"

# 2. Запустить обработку
docker-compose exec db psql -U postgres -d botc_stats -c "SELECT * FROM process_games_import();"
```

### Импорт через pgAdmin/DBeaver

1. Открыть pgAdmin → база → Tools → Import/Export Data
2. Выбрать таблицу `games_import_staging`
3. Указать путь к CSV, формат CSV, отметить Header: true
4. Нажать Import
5. Выполнить `SELECT * FROM process_games_import();`

## 🔐 Безопасность

### Обязательные действия перед продакшеном

1. **Измените пароли в `.env`:**
   - `POSTGRES_PASSWORD` — сильный пароль администратора (16+ символов)
   - `BOTC_USER_PASSWORD` — отдельный пароль для приложения

2. **Не используйте `postgres` в приложении** — только для администрирования

3. **Используйте volume для данных** — они сохраняются при перезапуске

### Принципы безопасности

- Строгая валидация ролей перед импортом
- Отдельный пользователь `botc_user` с ограниченными правами
- Отсутствие прав CREATE для `botc_user` и PUBLIC
- Автоматическое назначение прав на новые таблицы

## 📊 Полезные запросы

```sql
-- Топ-5 игроков по винрейту (минимум 5 игр)
SELECT name, games_played, win_rate_pct
FROM v_player_stats
WHERE games_played >= 5
ORDER BY win_rate_pct DESC
LIMIT 5;

-- Топ-3 самых частых ролей по винрейту
SELECT role_name, times_played, win_rate_pct
FROM v_role_stats
WHERE times_played >= 3
ORDER BY win_rate_pct DESC
LIMIT 3;

-- Все партии за последний месяц
SELECT game_date, scenario_name, storyteller, color_win, players
FROM v_game_summary
WHERE game_date >= CURRENT_DATE - INTERVAL '1 month'
ORDER BY game_date DESC;

-- Роли, которые чаще всего меняются в ходе игры
SELECT role_name, role_changed_count, times_played,
       ROUND(100.0 * role_changed_count / times_played, 1) AS change_rate_pct
FROM v_role_stats
WHERE times_played >= 5
ORDER BY change_rate_pct DESC;
```

## 🛠️ Устранение неполадок

| Проблема | Решение |
|----------|---------|
| Контейнер не запускается | `docker-compose logs db` → проверить ошибки |
| База не инициализируется | `docker-compose down -v && docker-compose up -d` |
| Ошибка подключения | `docker-compose ps` → проверить статус |
| Импорт не создаёт записи | Проверить `SELECT COUNT(*) FROM games_import_staging;` |
| Роль не существует | Добавить в `roles` таблицу перед импортом |

## 🗄️ Резервное копирование

```bash
# Создать бэкап
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql

# Восстановить
docker-compose exec db psql -U postgres botc_stats < backup.sql
```

## 📝 История версий

### v1.1.0 (текущая)

- ✅ Исправлена логика импорта игроков в `process_games_import()`
- ✅ Рефакторинг схемы для многопользовательской архитектуры
- ✅ Добавлены Docker-конфигурации
- ✅ Обновлённая документация

### v1.0.0

- Первоначальная схема с базовыми таблицами и представлениями

## 🤝 Вклад в проект

1. Создайте ветку для новой фичи: `git checkout -b feature/new-role-type`
2. Внесите изменения в схему или документацию
3. Протестируйте миграции на тестовой БД
4. Отправьте Pull Request с описанием изменений

## 📄 Лицензия

MIT — используйте, модифицируйте, делитесь.

---

**Примечание для AI-ассистента:**
- Всегда используйте русский язык для комментариев и документации
- Технические термины и имена таблиц/полей оставляйте на английском
- Соблюдайте существующую структуру схемы при внесении изменений
- При добавлении новых функций следуйте принципам нормализации (3NF)
