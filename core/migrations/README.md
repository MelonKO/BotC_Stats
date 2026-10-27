# Миграции базы данных

Каталог содержит SQL-миграции для **уже работающей** базы. Они не выполняются
автоматически и не попадают в образ `botc-stats-db`: PostgreSQL запускает скрипты из
`/docker-entrypoint-initdb.d` только при создании пустого каталога данных, а изменение
схемы работающей установки — отдельное, осознанное действие.

## Запуск

```bash
cd core

# бэкап перед изменением схемы
docker compose -f docker-compose.release.yml exec -T db \
  pg_dump -U postgres botc_stats > backup.sql

# применить все неприменённые миграции
docker compose -f docker-compose.release.yml run --rm db-migrate
```

Локальная разработка (`docker-compose.yml`) использует тот же runner:

```bash
docker compose --profile migrate run --rm db-migrate
```

## Как работает runner

`db/migrate.sh` (`/usr/local/bin/botc-migrate` в образе):

1. создаёт таблицу журнала `public.schema_migrations (version, applied_at)`;
2. перебирает `/migrations/*.sql` в лексикографическом порядке — имя файла задаёт порядок;
3. пропускает файлы, уже записанные в журнал;
4. применяет остальные через `psql -v ON_ERROR_STOP=1` и останавливается на первой ошибке;
5. записывает имя успешно применённого файла в журнал.

Повторный запуск безопасен: он донакатывает только новые файлы. На пустой базе, которую
инициализировал `init-scripts/01-schema.sql`, миграции ничего не меняют по существу и просто
отмечаются в журнале.

Ручной запуск одного файла (без записи в журнал):

```bash
docker compose -f docker-compose.release.yml exec -T db \
  psql -U postgres -d botc_stats -v ON_ERROR_STOP=1 \
  < migrations/98-migrate-alignment-end.sql
```

## Соглашения

| Требование | Почему |
|------------|--------|
| Имя `NNN-краткое-описание.sql` | порядок применения — лексикографический |
| Идемпотентность (`IF EXISTS`, `IF NOT EXISTS`, `DO`-блоки) | миграция может встретить как старую схему, так и свежую из `init-scripts/` |
| Собственная транзакция (`BEGIN` / `COMMIT`) или атомарность одного оператора | `ON_ERROR_STOP` останавливает файл, но не откатывает уже применённые операторы |
| Дублирование изменения в `init-scripts/01-schema.sql` | init-скрипты создают актуальную схему с нуля, миграции доводят до неё старые установки |
| Файл **не** кладётся в `init-scripts/` | иначе он выполнится только на пустой базе и никогда — на работающей |

## Существующие миграции

| Файл | Что делает |
|------|------------|
| `98-migrate-alignment-end.sql` | сужает `games_import_staging.alignment_end_ru` до `добро`/`зло` |
| `99-migrate-storyteller-draw.sql` | несколько рассказчиков (`game_storytellers`), сторона `ничья`, обновлённые `process_games_import()` и `v_game_summary` |
| `100-grant-players-update.sql` | право `UPDATE (name)` на `players` для `api_service` (переименование игрока через `PATCH /api/players/{playerId}`) |
| `101-grant-role-translations-delete.sql` | право `DELETE` на `role_translations` для `api_service` (удаление перевода через `PATCH /api/roles/{roleId}`) |

## Проверка состояния

```bash
docker compose -f docker-compose.release.yml exec -T db \
  psql -U postgres -d botc_stats \
  -c "SELECT version, applied_at FROM schema_migrations ORDER BY version"
```
