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

Ветку можно отправить в origin и открыть PR в `main`, чтобы прогнать CI до мержа:

```bash
git push -u origin release/1.2.3
```

### 3. Проверить локально

```powershell
python scripts/project.py setup            # один раз: npm-зависимости и генератор моделей
python scripts/project.py verify           # generated-код актуален + тесты core API
python DBConnect/scripts/build.py deploy   # опционально: проверить Windows-сборку
```

Тег должен указывать на коммит, в котором есть `scripts/project.py`: workflow вызывает его
в job `verify`.

### 4. Влить в main и поставить тег

```bash
git switch main
git pull --ff-only
git merge --no-ff release/1.2.3 -m "Merge branch 'release/1.2.3'"

git tag -a v1.2.3 -m "BotC Stats 1.2.3"
git push origin main --follow-tags
```

`--follow-tags` отправляет `main` и достижимые аннотированные теги. Если тег нужно отправить
отдельно: `git push origin v1.2.3`. Именно пуш тега запускает релиз.

### 5. Вернуть изменения в develop

```bash
git switch develop
git merge --no-ff release/1.2.3 -m "Merge branch 'release/1.2.3' into develop"
git push origin develop
git branch -d release/1.2.3
git push origin --delete release/1.2.3   # если ветка отправлялась
```

### 6. Проверить релиз

1. Actions → workflow **Release** завершился успешно.
2. В GitHub Release лежат `DBConnect-1.2.3-win64.zip` и `SHA256SUMS.txt`.
3. Образы доступны: `docker pull ghcr.io/<owner>/botc-stats-api:1.2.3`.
4. Сервер обновлён на новую версию (см. «Развёртывание core из опубликованных образов»).

## Если main защищён

Прямой merge в `main` может быть запрещён. Тогда:

```bash
git switch release/1.2.3
git push -u origin release/1.2.3
# открыть PR release/1.2.3 -> main и смержить его

git switch main
git pull --ff-only
git tag -a v1.2.3 -m "BotC Stats 1.2.3"
git push origin v1.2.3
```

Тег ставится на merge-коммит в `main` уже после мержа PR.

## Хотфикс

Хотфикс делается от `main` и поднимает patch-версию:

```bash
git switch main
git pull --ff-only
git switch -c hotfix/1.2.4

# исправление + версия 1.2.4 в DBConnect/CMakeLists.txt
git add -A
git commit -m "fix: ..."

git switch main
git merge --no-ff hotfix/1.2.4 -m "Merge branch 'hotfix/1.2.4'"
git tag -a v1.2.4 -m "BotC Stats 1.2.4"
git push origin main --follow-tags

git switch develop
git merge --no-ff hotfix/1.2.4 -m "Merge branch 'hotfix/1.2.4' into develop"
git push origin develop
git branch -d hotfix/1.2.4
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
- [ ] релизная ветка влита в `main` через `--no-ff`;
- [ ] аннотированный тег `vX.Y.Z` стоит на коммите в `main`;
- [ ] тег отправлен (`git push origin vX.Y.Z` или `--follow-tags`);
- [ ] workflow **Release** зелёный, ассеты и образы на месте;
- [ ] изменения влиты обратно в `develop`;
- [ ] сервер обновлён и `/health` отвечает.

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
