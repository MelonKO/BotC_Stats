# Релизы (GitHub Releases)

Релиз — это git-тег `vX.Y.Z`. Пуш тега запускает workflow
[`.github/workflows/release.yml`](../.github/workflows/release.yml), который проверяет код,
собирает Windows-приложение DBConnect, публикует образы `core` в GitHub Container Registry
(GHCR) и создаёт GitHub Release с вложенным архивом.

Один тег = одна версия всего монорепозитория (см. раздел
[Versioning](../README.md#versioning)).

## Что публикуется

| Артефакт | Где | Содержимое |
|----------|-----|------------|
| `DBConnect-<version>-win64.zip` | вложения GitHub Release | `DBConnect.exe`, Qt DLL и плагины, `config.ini` из `config.ini.example` (пустой ключ, `SSL_VERIFY = false`) |
| `SHA256SUMS.txt` | вложения GitHub Release | контрольные суммы архива |
| `ghcr.io/<owner>/botc-stats-api` | GHCR | FastAPI-сервис (`core/api/Dockerfile`) |
| `ghcr.io/<owner>/botc-stats-nginx` | GHCR | nginx с запечёнными `nginx.conf` и `html/`, сертификаты монтируются отдельно |
| `ghcr.io/<owner>/botc-stats-db` | GHCR | `postgres:16` с запечёнными `core/init-scripts/` — схема, пользователи и гранты для пустой базы. Миграции в образ не входят |

Теги образов для релиза `v1.2.3`:

- `1.2.3` — точная версия;
- `sha-<12 символов SHA>` — конкретный коммит;
- `1.2` — последняя стабильная версия этой minor-ветки;
- `latest` — последняя стабильная версия.

У pre-release (`v1.2.3-rc.1`) есть только точная версия и `sha-…`: теги `1.2` и `latest`
всегда указывают на стабильный релиз.

`metabase` — сторонний образ, он не пересобирается. Образ БД собирается поверх официального
`postgres:16` и содержит только init-скрипты: они выполняются исключительно при создании
пустого каталога данных, поэтому `core/migrations/` в образ не попадает.

## Одноразовая настройка репозитория

1. **Settings → Actions → General → Workflow permissions** → *Read and write permissions*.
   Без этого `GITHUB_TOKEN` не сможет создать релиз и запушить пакеты, даже если в workflow
   указаны `permissions: packages: write` / `contents: write`.
2. **Видимость пакетов GHCR.** Первый пакет создаётся приватным. Если образы должны быть
   доступны без авторизации: откройте пакет в профиле/организации → *Package settings* →
   *Change visibility* → *Public*. Для приватных образов на сервере нужен
   `docker login ghcr.io` с токеном, у которого есть `read:packages`.
3. **Защита ветки `main`.** Требуется PR, force-push запрещён, merge-коммиты разрешены;
   требование линейной истории выключено — иначе релизный PR нельзя было бы смержить
   merge-коммитом. Настройка: Settings → Rules / Rulesets → `main`.
4. **Защита тегов (рекомендуется).** Settings → Rules → Rulesets → новый tag ruleset на
   `v*` с запретом удаления/перезаписи тегов.
5. **Подпись exe (опционально).** Сейчас сборка не подписывается, поэтому Windows SmartScreen
   покажет предупреждение при первом запуске. Для подписи понадобится сертификат и отдельный
   шаг в job `dbconnect` (секреты репозитория).
6. **Раннер для сборки DBConnect (опционально).** По умолчанию job `dbconnect` идёт на
   `windows-latest`. Чтобы собирать на своей машине, добавьте переменную репозитория
   `BOTC_DBCONNECT_RUNNER` (Settings → Secrets and variables → Actions → Variables) со
   списком меток в формате JSON:

   ```text
   ["self-hosted","windows","x64"]
   ```

   Одна метка тоже допустима, но тогда её нужно заключить в кавычки: `"windows-latest"`.
   Если переменная не задана или пуста, используется `windows-latest`. Машина раннера должна
   иметь Visual Studio C++ build tools, CMake 3.25+, Ninja и vcpkg в `C:\vcpkg` (путь
   зашит в `VCPKG_ROOT` этой job).

## Процесс релиза

Процесс следует git-flow (релизы уходят в `main`, разработка идёт в `develop`), но утилита
`git flow` не нужна: все шаги выполняются обычными командами Git.

### 1. Подготовить версию

Версия хранится в `DBConnect/CMakeLists.txt`:

```cmake
project(DBConnect VERSION 1.2.3 LANGUAGES CXX)
```

Workflow сравнивает тег с этим значением и падает, если они расходятся. Дополнительно можно
обновить `version-string` в `DBConnect/vcpkg.json` и `info.version` в `openapi/api/openapi.yaml` —
на сборку это не влияет, но сохраняет версии компонентов консистентными (workflow их не проверяет).

### 2. Создать релизную ветку

```bash
git switch develop
git pull --ff-only
git switch -c release/1.2.3

# поднять версию в DBConnect/CMakeLists.txt (и, при желании, в vcpkg.json/openapi.yaml)
git add DBConnect/CMakeLists.txt DBConnect/vcpkg.json openapi/api/openapi.yaml
git commit -m "chore(release): 1.2.3"
```

### 3. Проверить локально

```powershell
python scripts/project.py setup            # один раз: npm-зависимости и генератор моделей
python scripts/project.py verify           # generated-код актуален + тесты core API
python DBConnect/scripts/build.py deploy   # опционально: проверить Windows-сборку
```

Тег должен указывать на коммит, в котором есть `scripts/project.py`: workflow вызывает его
в job `verify`.

### 4. Влить в main через PR

`main` защищена: прямые пуши запрещены, изменения принимаются только через pull request.
Merge-коммиты разрешены, требование линейной истории выключено, поэтому релизную ветку
сливаем способом **Create a merge commit** (не squash/rebase) — так в истории `main`
останется связь с релизной веткой.

```bash
git switch release/1.2.3
git push -u origin release/1.2.3
```

Откройте на GitHub PR `release/1.2.3 → main` и выберите **Create a merge commit**.

### 5. Поставить тег на merge-коммит в main

```bash
git switch main
git pull --ff-only

git tag -a v1.2.3 -m "BotC Stats 1.2.3"
git push origin v1.2.3
```

Тег ставится на merge-коммит, появившийся после мержа PR. Именно пуш тега запускает релиз.
`git push origin main --follow-tags` не подойдёт: прямой пуш в `main` запрещён правилами.

### 6. Вернуть изменения в develop

```bash
git switch develop
git pull --ff-only
git merge --ff-only release/1.2.3
git push origin develop

git branch -d release/1.2.3
git push origin --delete release/1.2.3
```

Релизная ветка ответвлена от `develop`, поэтому слияние обычно проходит как fast-forward.
Если `develop` успел уйти вперёд, используйте обычный `git merge release/1.2.3`
(merge-коммиты в `develop` разрешены) или PR.

### 7. Проверить релиз

1. Actions → workflow **Release** завершился успешно.
2. В GitHub Release лежат `DBConnect-1.2.3-win64.zip` и `SHA256SUMS.txt`.
3. Образы доступны: `docker pull ghcr.io/<owner>/botc-stats-api:1.2.3` (то же для
   `botc-stats-nginx` и `botc-stats-db`).
4. Сервер обновлён на новую версию (см. «Развёртывание core из опубликованных образов»).

## Если правила main нужно обойти

Правила защиты можно временно обойти правами администратора, но для обычного релиза это не
нужно: PR — штатный путь. Обход используйте только для исправления самой ветки или истории
и сразу возвращайте правила после операции.

## Хотфикс

Хотфикс делается от `main` и поднимает patch-версию:

```bash
git switch main
git pull --ff-only
git switch -c hotfix/1.2.4

# исправление + версия 1.2.4 в DBConnect/CMakeLists.txt
git add -A
git commit -m "fix: ..."

git push -u origin hotfix/1.2.4
# PR hotfix/1.2.4 -> main, способ слияния "Create a merge commit"

git switch main
git pull --ff-only
git tag -a v1.2.4 -m "BotC Stats 1.2.4"
git push origin v1.2.4

git switch develop
git merge --ff-only hotfix/1.2.4
git push origin develop
git branch -d hotfix/1.2.4
git push origin --delete hotfix/1.2.4
```

## Pre-release

Pre-release — это тег с суффиксом, например `v1.2.3-rc.1`. Версия в `DBConnect/CMakeLists.txt`
должна быть `1.2.3` (суффикс workflow отбрасывает). Такой тег:

- создаёт GitHub pre-release;
- публикует образы с тегами `1.2.3-rc.1` и `sha-…`, но не двигает `1.2` и `latest`.

Pre-release удобно использовать как первую обкатку пайплайна: если сборка упала, тег можно
удалить и поставить заново, не трогая стабильную версию.

## Чек-лист

- [ ] версия поднята в `DBConnect/CMakeLists.txt` и совпадает с будущим тегом;
- [ ] `python scripts/project.py verify` проходит;
- [ ] PR `release/X.Y.Z → main` смержен способом **Create a merge commit**;
- [ ] аннотированный тег `vX.Y.Z` стоит на merge-коммите в `main`;
- [ ] тег отправлен (`git push origin vX.Y.Z`);
- [ ] workflow **Release** зелёный, ассеты и образы на месте;
- [ ] изменения возвращены в `develop`;
- [ ] если релиз менял схему — миграция добавлена в `core/migrations/` и отражена в `core/init-scripts/01-schema.sql`;
- [ ] на сервере выполнен `db-migrate` и `schema_migrations` содержит все файлы;
- [ ] сервер обновлён и `/health` отвечает.

## Как устроен workflow

| Job | Runner | Что делает |
|-----|--------|------------|
| `version` | ubuntu | проверяет формат тега и совпадение с версией CMake, вычисляет `version` / `major_minor` / `prerelease` |
| `verify` | ubuntu | тесты `core/api`, `project.py generate`, проверка что `core/api/app/models.py` не устарел, `docker compose config` для обоих compose-файлов; отдаёт сгенерированный C++-клиент как артефакт |
| `dbconnect` | `windows-latest` или self-hosted (переменная `BOTC_DBCONNECT_RUNNER`) | скачивает сгенерированный клиент, собирает Release через `DBConnect/scripts/build.py deploy`, проверяет пакет и загружает zip |
| `images` | ubuntu | собирает и пушит `botc-stats-api`, `botc-stats-nginx` и `botc-stats-db` в GHCR (matrix) |
| `release` | ubuntu | скачивает zip, считает `SHA256SUMS.txt`, создаёт GitHub Release с автоматическими заметками |

Особенности:

- `DBConnect/generated_api/` не хранится в git, поэтому Windows-job получает его артефактом
  из `verify`. Локально этот каталог создаёт `python scripts/project.py generate`.
- Первая сборка Qt 6.10.2 через vcpkg занимает десятки минут. Бинарный кеш vcpkg хранится в
  GitHub Actions cache (ключ — хеш `DBConnect/vcpkg.json`). GitHub не разделяет кеш между
  тегами: запуск на теге видит только кеш default-ветки, поэтому кеш ускоряет *повторный*
  запуск того же тега (например, после сбоя упаковки), а новый тег может снова собрать Qt.
  Если это станет проблемой, прогрейте кеш отдельным запуском сборки на `main` или
  перейдите на NuGet-кеш vcpkg в GitHub Packages (нужен PAT с `packages:write`).
- Pre-release тег (`v1.2.3-rc.1`) создаёт GitHub pre-release и не двигает `1.2` и `latest`.
- Раннер для `dbconnect` выбирается переменной `BOTC_DBCONNECT_RUNNER`: по умолчанию
  `fromJSON(vars.BOTC_DBCONNECT_RUNNER || '["windows-latest"]')`. Значение читается в момент
  постановки job в очередь, поэтому смена переменной не переносит уже запущенный job:
  отмените run, измените переменную и перезапустите run (**Re-run all jobs**) — повторный
  запуск возьмёт тот же коммит и тег.
- Артефакт workflow хранится 7 дней, но файлы релиза остаются навсегда.

## Развёртывание core из опубликованных образов

Все образы стека приходят из GHCR: API, nginx и база (`postgres:16` вместе с
`core/init-scripts/`). Из checkout нужны только `docker-compose.release.yml`, `migrations/`
и `nginx/ssl/`: сертификаты и миграции зависят от установки и от версии базы, поэтому в
образы не запекаются.

### 1. Подготовить окружение

```bash
cd core
cp .env.example .env
# Заполните пароли, METABASE_EMBEDDING_SECRET и SERVER_IP.
# Добавьте точную версию, которую хотите развернуть:
echo "BOTC_VERSION=1.2.3" >> .env

bash scripts/generate-cert.sh          # или положите доверенный сертификат в nginx/ssl/
```

### 2. Первый запуск на пустой базе

```bash
docker compose -f docker-compose.release.yml pull
docker compose -f docker-compose.release.yml up -d
```

При первом старте образ БД выполняет `init-scripts/` (схема, пользователи, гранты). Это
происходит только когда том `botc_data` пуст: `init-scripts/01-schema.sql` сразу создаёт
актуальную схему, поэтому отдельные миграции на этом шаге не нужны.

### 3. Проверить

```bash
curl -k https://localhost/health
docker compose -f docker-compose.release.yml ps
```

### Обновление на новую версию

```bash
# 1. Бэкап перед изменением схемы
docker compose -f docker-compose.release.yml exec -T db \
  pg_dump -U postgres botc_stats > backup-before-1.3.0.sql

# 2. Укажите новую версию в .env: BOTC_VERSION=1.3.0
# 3. Скачайте образы
docker compose -f docker-compose.release.yml pull

# 4. Примените миграции (подробности — в разделе «Миграции базы данных»)
docker compose -f docker-compose.release.yml run --rm db-migrate

# 5. Пересоздайте контейнеры на новых образах
docker compose -f docker-compose.release.yml up -d
```

Порядок важен: `db-migrate` работает с уже запущенной базой, поэтому шаг 4 идёт после `pull`
и до `up -d`. Если стек остановлен, поднимите базу: `up -d db`.

Откат — изменить `BOTC_VERSION` на предыдущую версию и повторить `pull` + `up -d`. Данные БД
лежат в named volume `botc_data` и при смене версии не затрагиваются. Откат образа не
отменяет миграции: если новая версия меняла схему, восстановите бэкап из шага 1.

Если образы размещены не в GHCR или репозиторий форкнут, переопределите имена:

```dotenv
BOTC_API_IMAGE=registry.example.com/botc-stats-api
BOTC_NGINX_IMAGE=registry.example.com/botc-stats-nginx
BOTC_DB_IMAGE=registry.example.com/botc-stats-db
```

## Миграции базы данных

Миграции меняют уже работающую базу, поэтому они никогда не выполняются автоматически:
PostgreSQL запускает init-скрипты только на пустом каталоге данных, а приложение не имеет
доступа к DDL. Миграции лежат в `core/migrations/` и применяются отдельным шагом.

### Шаги

1. Проверьте, что база запущена:

   ```bash
   docker compose -f docker-compose.release.yml ps db
   ```

2. Сделайте бэкап — перед первой миграцией новой версии это обязательно:

   ```bash
   docker compose -f docker-compose.release.yml exec -T db \
     pg_dump -U postgres botc_stats > backup.sql
   ```

3. Запустите разовый контейнер миграций. Сервис `db-migrate` включён в профиль `migrate`,
   поэтому обычный `up -d` его не запускает:

   ```bash
   docker compose -f docker-compose.release.yml run --rm db-migrate
   ```

   Runner применяет `migrations/*.sql` в лексикографическом порядке, останавливается на
   первой ошибке (`ON_ERROR_STOP=1`) и записывает имя каждого успешно применённого файла в
   `public.schema_migrations`. Повторный запуск пропускает уже применённые файлы, поэтому
   команду можно выполнять при каждом обновлении.

4. Проверьте результат:

   ```bash
   docker compose -f docker-compose.release.yml exec -T db \
     psql -U postgres -d botc_stats \
     -c "SELECT version, applied_at FROM schema_migrations ORDER BY version"

   curl -k https://localhost/health
   ```

На пустой базе запуск по существу ничего не меняет: `01-schema.sql` уже создал актуальную
схему, миграции ничего не меняют и просто отмечаются в журнале как применённые.

### Ручной запуск одного файла

Если нужно применить конкретную миграцию без runner:

```bash
docker compose -f docker-compose.release.yml exec -T db \
  psql -U postgres -d botc_stats -v ON_ERROR_STOP=1 \
  < migrations/99-migrate-storyteller-draw.sql
```

Такой запуск не попадает в `schema_migrations`. Миграции написаны идемпотентно, поэтому
повторный `db-migrate` после ручного применения безопасен — запись в журнале появится
тогда.

### Правила для новых миграций

- Файл `core/migrations/NNN-краткое-описание.sql`; нумерация задаёт порядок применения.
- Миграция идемпотентна: `IF EXISTS` / `IF NOT EXISTS` / `DO`-блоки с проверкой схемы, чтобы
  её можно было безопасно применить к новой базе и повторно.
- Изменение схемы дублируется в `core/init-scripts/01-schema.sql`: init-скрипты описывают
  базу с нуля, миграции доводят до этой схемы старые установки.
- Файл не кладётся в `core/init-scripts/`: оттуда скрипты выполняются только на пустой базе
  и никогда не доходят до работающей.

## Локальная проверка образов

Docker-сборку можно воспроизвести без registry:

```bash
docker build -t botc-stats-api:local -f core/api/Dockerfile core/api
docker build -t botc-stats-nginx:local core/nginx
docker build -t botc-stats-db:local -f core/db/Dockerfile core

cd core
BOTC_API_IMAGE=botc-stats-api BOTC_NGINX_IMAGE=botc-stats-nginx BOTC_DB_IMAGE=botc-stats-db \
  BOTC_VERSION=local docker compose -f docker-compose.release.yml up -d
```

## Траблшутинг

| Симптом | Причина и решение |
|---------|-------------------|
| `Tag vX.Y.Z does not match DBConnect/CMakeLists.txt` | версия в CMake не поднята или тег поставлен не на тот коммит — исправьте версию и перевыпустите тег |
| `Resource not accessible by integration` при создании релиза | у репозитория отключены write-права `GITHUB_TOKEN` (см. «Одноразовая настройка») |
| `denied: permission_denied` при пуше в GHCR | нет `packages: write` в job или политика организации запрещает публикацию пакетов |
| `db-migrate` печатает `no migration files in /migrations` | команда запущена не из `core/`: каталог `./migrations` монтируется относительно compose-файла |
| `db-migrate` падает на `relation ... does not exist` | миграция не подходит к текущей схеме — восстановите бэкап и проверьте `schema_migrations` |
| схема не обновилась после `up -d` | init-скрипты выполняются только на пустом томе; изменения в работающей базе применяет `db-migrate` |
| `db-migrate` не видит базу | база не запущена: `docker compose -f docker-compose.release.yml up -d db` |
| Сборка DBConnect идёт больше часа | пустой кеш vcpkg (первый запуск, смена образа runner или `vcpkg.json`). Это ожидаемо |
| job `dbconnect` висит в статусе Queued | self-hosted раннер offline; job ждёт до 24 часов. Поставьте `BOTC_DBCONNECT_RUNNER` = `"windows-latest"`, отмените run и запустите его заново |
| `Unexpected end of JSON input` в job `dbconnect` | значение `BOTC_DBCONNECT_RUNNER` не JSON: одна метка — `"windows-latest"`, несколько — `["self-hosted","windows","x64"]` |
| `Could not find a baseline commit` в vcpkg | в checkout vcpkg нет нужного baseline; workflow пытается до-фетчить его, при повторной ошибке обновите `builtin-baseline` и `qtbase` в `DBConnect/vcpkg.json` |
| `config.ini contains an API key` | в рабочей копии появился локальный `config.ini` с ключом; CI его не содержит, проверьте, что файл не попал в архив |
