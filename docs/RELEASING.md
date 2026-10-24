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

Теги образов для релиза `v1.2.3`:

- `1.2.3` — точная версия;
- `sha-<12 символов SHA>` — конкретный коммит;
- `1.2` — последняя стабильная версия этой minor-ветки;
- `latest` — последняя стабильная версия.

У pre-release (`v1.2.3-rc.1`) есть только точная версия и `sha-…`: теги `1.2` и `latest`
всегда указывают на стабильный релиз.

`postgres` и `metabase` — сторонние образы, они не пересобираются.

## Одноразовая настройка репозитория

1. **Settings → Actions → General → Workflow permissions** → *Read and write permissions*.
   Без этого `GITHUB_TOKEN` не сможет создать релиз и запушить пакеты, даже если в workflow
   указаны `permissions: packages: write` / `contents: write`.
2. **Видимость пакетов GHCR.** Первый пакет создаётся приватным. Если образы должны быть
   доступны без авторизации: откройте пакет в профиле/организации → *Package settings* →
   *Change visibility* → *Public*. Для приватных образов на сервере нужен
   `docker login ghcr.io` с токеном, у которого есть `read:packages`.
3. **Защита тегов (рекомендуется).** Settings → Rules → Rulesets → новый tag ruleset на
   `v*` с запретом удаления/перезаписи тегов.
4. **Подпись exe (опционально).** Сейчас сборка не подписывается, поэтому Windows SmartScreen
   покажет предупреждение при первом запуске. Для подписи понадобится сертификат и отдельный
   шаг в job `dbconnect` (секреты репозитория).

## Подготовка релиза

Версия хранится в `DBConnect/CMakeLists.txt`:

```cmake
project(DBConnect VERSION 1.2.3 LANGUAGES CXX)
```

Workflow сравнивает тег с этим значением и падает, если они расходятся. Дополнительно можно
обновить `version-string` в `DBConnect/vcpkg.json` и `info.version` в `openapi/api/openapi.yaml` —
на сборку это не влияет, но сохраняет версии компонентов консистентными (workflow их не проверяет).

Порядок по git-flow:

```bash
git flow release start 1.2.3

# 1. Поднять версию в DBConnect/CMakeLists.txt (и, при желании, в DBConnect/vcpkg.json)
git commit -am "chore(release): 1.2.3"

# 2. Завершить release-ветку без автоматического тега git-flow
git flow release finish -n 1.2.3

# 3. Поставить тег на merge-коммит в main
git tag -a v1.2.3 -m "BotC Stats 1.2.3"

# 4. Отправить main вместе с тегом — это запускает релиз
git push origin main --follow-tags
git push origin develop
```

Тег должен указывать на коммит в `main`. Локальную проверку можно сделать до пуша:

```powershell
python scripts/project.py verify    # generated-код актуален + тесты core API
python DBConnect/scripts/build.py deploy
```

## Как устроен workflow

| Job | Runner | Что делает |
|-----|--------|------------|
| `version` | ubuntu | проверяет формат тега и совпадение с версией CMake, вычисляет `version` / `major_minor` / `prerelease` |
| `verify` | ubuntu | тесты `core/api`, `project.py generate`, проверка что `core/api/app/models.py` не устарел, `docker compose config` для обоих compose-файлов; отдаёт сгенерированный C++-клиент как артефакт |
| `dbconnect` | windows-latest | скачивает сгенерированный клиент, собирает Release через `DBConnect/scripts/build.py deploy`, проверяет пакет и загружает zip |
| `images` | ubuntu | собирает и пушит `botc-stats-api` и `botc-stats-nginx` в GHCR (matrix) |
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
- Артефакт workflow хранится 7 дней, но файлы релиза остаются навсегда.

## Развёртывание core из опубликованных образов

На сервере по-прежнему нужен checkout репозитория: из него берутся `init-scripts/`
(инициализация новой БД) и `nginx/ssl/` (сертификаты). Сам код API и конфигурация nginx
приходят из образов.

```bash
cd core
cp .env.example .env
# Заполните пароли, METABASE_EMBEDDING_SECRET и SERVER_IP.
# Добавьте версию, которую хотите развернуть:
echo "BOTC_VERSION=1.2.3" >> .env

bash scripts/generate-cert.sh          # или положите доверенный сертификат в nginx/ssl/
docker compose -f docker-compose.release.yml pull
docker compose -f docker-compose.release.yml up -d
```

Проверка:

```bash
curl -k https://localhost/health
docker compose -f docker-compose.release.yml ps
```

Откат — изменить `BOTC_VERSION` на предыдущую версию и повторить `pull` + `up -d`.
Данные БД лежат в named volume `botc_data` и при смене версии не затрагиваются.

Если образы размещены не в GHCR или репозиторий форкнут, переопределите имена:

```dotenv
BOTC_API_IMAGE=registry.example.com/botc-stats-api
BOTC_NGINX_IMAGE=registry.example.com/botc-stats-nginx
```

## Локальная проверка образов

Docker-сборку можно воспроизвести без registry:

```bash
docker build -t botc-stats-api:local -f core/api/Dockerfile core/api
docker build -t botc-stats-nginx:local core/nginx

cd core
BOTC_API_IMAGE=botc-stats-api BOTC_NGINX_IMAGE=botc-stats-nginx BOTC_VERSION=local \
  docker compose -f docker-compose.release.yml up -d
```

## Траблшутинг

| Симптом | Причина и решение |
|---------|-------------------|
| `Tag vX.Y.Z does not match DBConnect/CMakeLists.txt` | версия в CMake не поднята или тег поставлен не на тот коммит — исправьте версию и перевыпустите тег |
| `Resource not accessible by integration` при создании релиза | у репозитория отключены write-права `GITHUB_TOKEN` (см. «Одноразовая настройка») |
| `denied: permission_denied` при пуше в GHCR | нет `packages: write` в job или политика организации запрещает публикацию пакетов |
| Сборка DBConnect идёт больше часа | пустой кеш vcpkg (первый запуск, смена образа runner или `vcpkg.json`). Это ожидаемо |
| `Could not find a baseline commit` в vcpkg | в checkout vcpkg нет нужного baseline; workflow пытается до-фетчить его, при повторной ошибке обновите `builtin-baseline` и `qtbase` в `DBConnect/vcpkg.json` |
| `config.ini contains an API key` | в рабочей копии появился локальный `config.ini` с ключом; CI его не содержит, проверьте, что файл не попал в архив |
