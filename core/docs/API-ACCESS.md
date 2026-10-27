# BotC API — Документация для внешних пользователей

> REST API для импорта партий Blood on the Clocktower через HTTPS.

---

## 📋 Обзор

API позволяет удалённо импортировать данные партий в базу данных без прямого доступа к PostgreSQL.

| Параметр | Значение |
|----------|----------|
| **URL** | `https://<SERVER_IP>` |
| **Протокол** | HTTPS (TLS 1.2 или TLS 1.3) |
| **Аутентификация** | API-ключ в заголовке `X-API-Key` для `/api/*`; `GET /health`, `/docs` и `/openapi.json` открыты без ключа |
| **Формат** | JSON |
| **Сертификат** | Для локальной разработки используется self-signed сертификат; production должен использовать доверенный сертификат |

В примерах с `verify=False` и `curl -k` отключена проверка подлинности сертификата сервера. Используйте их только для локальной разработки с self-signed сертификатом; в production настройте доверие к сертификату и оставьте проверку включённой.

---

## 🔑 Получение API-ключа

Обратитесь к администратору базы данных. Вам будет выдан ключ вида:

```
sk-EXAMPLE_DO_NOT_USE
```

**Важно:**
- Ключ — это секрет. Не передавайте его третьим лицам.
- Если ключ скомпрометирован — сообщите администратору для отзыва.
- Ключ отображается только один раз при создании.

---

## 📡 Эндпоинты

### `POST /api/games/import` — Импорт одной партии

> Поле `alignment_end` — это сторона игрока к концу партии и допускает только `добро`/`зло`. У роли (`roles.alignment`) сторон три, а у победившей стороны (`alignment_win`) есть `ничья`. Полная модель — в [`SIDES.md`](SIDES.md).

Отправляет данные одной партии в базу данных. Запрос требует API-ключа.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
Content-Type: application/json
```

**Тело запроса:**
```json
{
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_names": ["МелонКО"],
    "alignment_win": "добро",
    "location": "Москва, Антикафе на Арбате",
    "game_number": 1,
    "duration": "01:30:00",
    "notes": "Отличная партия, все получили удовольствие",
    "players": [
        {
            "name": "Мая Вишневская",
            "seat_number": 1,
            "role_start": "Дамочка",
            "role_end": "Дамочка",
            "alignment_end": "добро",
            "is_alive": true
        },
        {
            "name": "Борис Петров",
            "seat_number": 2,
            "role_start": "Азартный игрок",
            "role_end": "Азартный игрок",
            "alignment_end": "добро",
            "is_alive": true
        },
        {
            "name": "Виктор Сидоров",
            "seat_number": 3,
            "role_start": "Убийца",
            "role_end": "Убийца",
            "alignment_end": "зло",
            "is_alive": false
        }
    ]
}
```

**Поля:**

| Поле (верхний уровень) | Тип | Обязательное | Описание |
|------------------------|-----|--------------|----------|
| `game_date` | string (date) | ✅ | Дата партии (YYYY-MM-DD) |
| `scenario_name` | string | ✅ | Название сценария |
| `storyteller_names` | array | ✅ | Имена рассказчиков (1–5, несколько при совместном ведении) |
| `alignment_win` | string | ✅ | Победивший alignment: `добро`, `зло` или `ничья` |
| `location` | string | ✅ | Место проведения |
| `game_number` | int | ✅ | Номер партии в рамках встречи |
| `duration` | string | | Длительность (`HH:MM:SS` или `D HH:MM:SS`, например `1 02:30:00`) |
| `notes` | string | | Заметки к партии |
| `players` | array | ✅ | Список игроков (мин. 1, макс. 30) |

| Поле (в `players[]`) | Тип | Обязательное | Описание |
|----------------------|-----|--------------|----------|
| `name` | string | ✅ | Имя игрока |
| `seat_number` | int | | Номер места |
| `role_start` | string | ✅ | Начальная роль (русское название) |
| `role_end` | string | ✅ | Конечная роль (русское название) |
| `alignment_end` | string | ✅ | Сторона игрока к концу партии: только `добро` или `зло`. Роль может быть нейтральной (например, Странник — сторону выбирает рассказчик), но игрок всё равно заканчивает партию за добро или зло. `нейтральный` в этом поле отклоняется (422) |
| `is_alive` | boolean | ✅ | Выжил ли игрок |

**Ответ (успех, 200):**
```json
{
    "status": "ok",
    "game_id": "a1b2c3d4-e5f6-...",
    "players_created": 10,
    "errors": []
}
```

**Ответ (ошибка импорта, 400):**
```json
{
    "detail": {
        "status": "error",
        "errors": ["Отсутствуют роли: ФейковаяРоль. Обратитесь к администратору."]
    }
}
```

**Ответ (невалидный ключ, 401):**
```json
{
    "detail": "Invalid API key"
}
```

**Ответ (ключ отозван, 403):**
```json
{
    "detail": "API key has been revoked"
}
```

---

### `POST /api/games/import_batch` — Пакетный импорт партий

Принимает объект с массивом `games`; каждый элемент имеет тот же формат, что и запрос к `/api/games/import`.

```json
{
  "games": [
    {
      "game_date": "2026-01-15",
      "scenario_name": "Вселенная зла",
      "storyteller_names": ["МелонКО"],
      "alignment_win": "добро",
      "location": "Москва",
      "game_number": 1,
      "players": [
        {
          "name": "Мая Вишневская",
          "role_start": "Дамочка",
          "role_end": "Дамочка",
          "alignment_end": "добро",
          "is_alive": true
        }
      ]
    }
  ]
}
```

Успешный HTTP-ответ содержит массив результатов в `games`. Ошибка импорта отдельной партии отражается в её `status` и `errors`; неожиданная ошибка базы данных может прервать запрос. Ошибки структуры запроса возвращаются как HTTP 422.

---

### `POST /api/roles/import` — Импорт ролей

Импортирует (upsert) список ролей с опциональными переводами на разные языки.
Существующие роли обновляются, новые — создаются.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
Content-Type: application/json
```

**Тело запроса:**
```json
{
    "roles": [
        {
            "name": "Chambermaid",
            "alignment": "good",
            "role_type": "Outsider",
            "description": "Simple, but not harmless",
            "translations": {
                "ru": {
                    "name": "Горничная",
                    "description": "Просто, но не безобидно"
                }
            }
        },
        {
            "name": "Imp",
            "alignment": "evil",
            "role_type": "Demon",
            "translations": {
                "ru": {
                    "name": "Имп",
                    "description": "Каждую ночь* выбирайте игрока: он умирает"
                }
            }
        }
    ]
}
```

**Поля (каждая роль):**

| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `name` | string | ✅ | Английское имя роли |
| `alignment` | string | ✅ | `good`, `evil` или `neutral` |
| `role_type` | string | ✅ | `Townsfolk`, `Outsider`, `Minion`, `Demon` или `Traveller` |
| `description` | string | | Описание на английском |
| `translations` | object | | Словарь переводов по языкам; в новой базе доступны `en` и `ru` |
| `translations.<lang>.name` | string | ✅ (если перевод указан) | Имя роли на языке |
| `translations.<lang>.description` | string | | Описание на языке |

**Ответ (успех, 200):**
```json
{
    "status": "ok",
    "roles_created": 15,
    "roles_updated": 2,
    "errors": []
}
```

**Ответ (ошибка, 400):**
```json
{
    "detail": {
        "status": "error",
        "roles_created": 10,
        "roles_updated": 0,
        "errors": ["Role 'FakeRole': invalid input value for enum alignment: \"wrong\""]
    }
}
```

Новая база содержит языки `en` и `ru`. Для добавления другого языка сначала зарегистрируйте его в таблице `languages`.

**Пример на Python:**
```python
import requests
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

API_URL = "https://<SERVER_IP>/api/roles/import"
API_KEY = "sk-EXAMPLE_DO_NOT_USE"

data = {
    "roles": [
        {
            "name": "Chambermaid",
            "alignment": "good",
            "role_type": "Outsider",
            "translations": {
                "ru": {"name": "Горничная", "description": "Просто, но не безобидно"}
            }
        }
    ]
}

response = requests.post(
    API_URL,
    headers={"X-API-Key": API_KEY, "Content-Type": "application/json"},
    json=data,
    verify=False,  # Development only; use a trusted certificate in production.
)

result = response.json()
print(f"Создано: {result['roles_created']}, Обновлено: {result['roles_updated']}")
```

**Пример cURL:**
```bash
curl -k -X POST "https://<SERVER_IP>/api/roles/import" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{
    "roles": [{
      "name": "Chambermaid",
      "alignment": "good",
      "role_type": "Outsider",
      "translations": {
        "ru": {"name": "Горничная", "description": "Просто, но не безобидно"}
      }
    }]
  }'
```

---

### `GET /api/roles` — Список доступных ролей

Возвращает все роли, которые можно использовать при импорте. Необязательный параметр
`lang` (например, `ru`) добавляет локализованные названия типа и стороны роли, а
также перевод названия и описания роли. Без `lang` все четыре локализованных
поля (`role_type_translation`, `alignment_translation`, `translation.name` и
`translation.description`) равны `null`. Если для указанного языка перевода нет
или язык неизвестен, эти поля также равны `null`; запрос пока не проверяет язык
по таблице `languages`. Названия типов и сторон локализуются по данным таблиц
`role_type_translations` и `alignment_translations`, а не на клиенте. Требуется
API-ключ.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
```

**Ответ (200):**
```json
{
    "roles": [
        {
            "id": "a09f3ff7-0f02-485a-b0e1-87388b27face",
            "name": "Chambermaid",
            "alignment": "good",
            "role_type": "Outsider",
            "description": "Simple, but not harmless",
            "role_type_translation": "Изгой",
            "alignment_translation": "добро",
            "translation": {
                "name": "Дамочка",
                "description": "Служанка, которая следит за гостями"
            }
        }
    ]
}
```

---

### `PATCH /api/roles/{roleId}` — Редактирование роли

Частично обновляет роль и её переводы. Требуется API-ключ. UUID роли (`id`)
остаётся неизменным, поэтому существующие ссылки на роль в партиях сохраняются.
Необязательный параметр `lang` (код длиной 2–5 символов, например `ru`)
задаёт язык локализованных полей в возвращаемой записи.

> ⚠️ Сторона (`alignment`) и тип (`role_type`) роли используются в текущих
> аналитических представлениях, поэтому их изменение повлияет и на аналитику
> исторических партий. Импорт ролей обновляет записи по английскому `name`:
> изменение этого имени влияет на идентичность роли при последующем импорте.
> Импорт партий ищет роли по русскому названию, поэтому изменение русского
> имени (`translations.ru.name`) влияет на поиск роли в CSV партий.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
Content-Type: application/json
```

**Путь и параметры запроса:**

| Параметр | Где | Тип | Описание |
|----------|-----|-----|----------|
| `roleId` | путь | UUID | Неизменяемый идентификатор роли из `GET /api/roles` |
| `lang` | query | string | Необязательно; код языка длиной 2–5 символов для локализованных полей ответа |

**Тело запроса:**

Тело частичное: укажите только поля, которые нужно изменить, но передайте
минимум одно свойство. Неуказанные поля сохраняются. `name`, `alignment` и
`role_type` не могут быть `null`; `description` допускает `null`, чтобы очистить
описание. Если переданы `translations` или `translations_to_delete`, их значение
не может быть `null`.

| Поле | Тип | Ограничения и назначение |
|------|-----|--------------------------|
| `name` | string | Английское имя роли, 1–200 символов; пробелы по краям удаляются, имя только из пробелов отклоняется |
| `alignment` | string | Сторона роли: `good`, `evil` или `neutral` |
| `role_type` | string | `Townsfolk`, `Outsider`, `Minion`, `Demon` или `Traveller` |
| `description` | string или `null` | Описание, максимум 2000 символов; `null` очищает описание |
| `translations` | object | Словарь `{код_языка: RoleTranslation}` (коды длиной 2–5 символов) для установки или обновления переводов |
| `translations_to_delete` | array of strings | Коды языков, переводы для которых нужно удалить; код длиной 2–5 символов |

`RoleTranslation` содержит обязательное `name` (1–200 символов, непустое и не
состоящее только из пробелов) и необязательное `description` (до 2000 символов
или `null`). Каждый код языка в `translations` и `translations_to_delete` должен
быть зарегистрирован в базе данных; неизвестный код приводит к `422`. Один и тот
же язык нельзя одновременно указать в `translations` и
`translations_to_delete`.

**Ответ (200):** обновлённая запись `RoleItem`. При `lang` в ответе также
возвращаются локализованные поля для этого языка.
```json
{
  "id": "a09f3ff7-0f02-485a-b0e1-87388b27face",
  "name": "Revised Chambermaid",
  "alignment": "evil",
  "role_type": "Minion",
  "description": "Updated description",
  "role_type_translation": "Приспешник",
  "alignment_translation": "зло",
  "translation": {
    "name": "Новая Горничная",
    "description": "Новое описание"
  }
}
```

**Ошибки:**

| Код | Когда |
|-----|-------|
| `401` | Отсутствует или неверный API-ключ |
| `403` | API-ключ отозван |
| `404` | Роли с таким `roleId` нет |
| `409` | Английское имя уже занято другой ролью |
| `422` | Некорректный UUID или тело, нет изменяемых свойств, неизвестный язык либо язык одновременно задан и удаляется |

**Пример cURL — изменение полей роли и русского перевода:**
```bash
curl -k -X PATCH "https://<SERVER_IP>/api/roles/a09f3ff7-0f02-485a-b0e1-87388b27face?lang=ru" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Revised Chambermaid",
    "alignment": "evil",
    "role_type": "Minion",
    "description": "Updated description",
    "translations": {
      "ru": {
        "name": "Новая Горничная",
        "description": "Новое описание"
      }
    }
  }'
```

**Пример cURL — удаление русского перевода:**
```bash
curl -k -X PATCH "https://<SERVER_IP>/api/roles/a09f3ff7-0f02-485a-b0e1-87388b27face" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{"translations_to_delete": ["ru"]}'
```

---

### `GET /api/players` — Список игроков

Требует API-ключ. Возвращает игроков, зарегистрированных в базе, отсортированных по имени:

```json
{
  "players": [
    {"id": "b1c2d3e4-f5a6-7890-abcd-1234567890ef", "name": "Мая Вишневская"}
  ]
}
```

---

### `PATCH /api/players/{playerId}` — Переименование игрока

Меняет имя существующего игрока. Требуется API-ключ. Идентификатор игрока (`id`) не меняется, поэтому история партий остаётся привязанной к той же записи: во всех отчётах и представлениях (`v_player_stats`, `v_game_summary`) игрок будет показан под новым именем.

> ⚠️ Импорт партий ищет игрока по имени. CSV-файл, в котором осталось старое имя, при следующем импорте создаст **нового** игрока, а не привяжется к переименованному. Переименование необратимо: история предыдущих имён не хранится.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
Content-Type: application/json
```

**Путь:**

| Параметр | Тип | Описание |
|----------|-----|----------|
| `playerId` | UUID | Идентификатор игрока из `GET /api/players` |

**Тело запроса:**

| Поле | Тип | Обязательное | Описание |
|------|-----|--------------|----------|
| `name` | string | ✅ | Новое имя: от 1 до 200 символов. Пробелы по краям отбрасываются; регистр сохраняется как введён |

**Ответ (200):** обновлённая запись игрока.
```json
{
  "id": "b1c2d3e4-f5a6-7890-abcd-1234567890ef",
  "name": "Мая Вишневская-Новая"
}
```

**Ошибки:**

| Код | Когда |
|-----|-------|
| `401` | Отсутствует или неверный API-ключ |
| `404` | Игрока с таким `playerId` нет |
| `409` | Имя уже занято другим игроком (имена уникальны) |
| `422` | `playerId` не UUID, имя пустое/состоит из пробелов/длиннее 200 символов |

**Пример cURL:**
```bash
curl -k -X PATCH "https://<SERVER_IP>/api/players/b1c2d3e4-f5a6-7890-abcd-1234567890ef" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{"name": "Мая Вишневская-Новая"}'
```

---

### `GET /health` — Проверка работоспособности

Не требует API-ключа. Показывает статус сервиса и БД.

**Ответ (200):**
```json
{
    "status": "ok",
    "db_connected": true
}
```

---

## 💻 Примеры кода

### Python (requests)

```python
import requests

API_URL = "https://<SERVER_IP>/api/games/import"
API_KEY = "sk-EXAMPLE_DO_NOT_USE"  # Ваш API-ключ

data = {
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_names": ["МелонКО"],
    "alignment_win": "добро",
    "location": "Москва, Антикафе на Арбате",
    "game_number": 1,
    "players": [
        {
            "name": "Анна",
            "seat_number": 1,
            "role_start": "Дамочка",
            "role_end": "Дамочка",
            "alignment_end": "добро",
            "is_alive": True
        },
        {
            "name": "Борис",
            "seat_number": 2,
            "role_start": "Убийца",
            "role_end": "Убийца",
            "alignment_end": "зло",
            "is_alive": False
        }
    ]
}

response = requests.post(
    API_URL,
    headers={
        "X-API-Key": API_KEY,
        "Content-Type": "application/json",
    },
    json=data,
    verify=False  # Самоподписанный сертификат
)

result = response.json()
print(f"Статус: {result['status']}")
print(f"Создано игроков: {result['players_created']}")
if result.get('errors'):
    print(f"Ошибки: {result['errors']}")
```

### Python — получение списка ролей

```python
import requests

API_URL = "https://<SERVER_IP>/api/roles"
API_KEY = "sk-EXAMPLE_DO_NOT_USE"

response = requests.get(
    API_URL,
    headers={"X-API-Key": API_KEY},
    verify=False
)

roles = response.json()["roles"]
for role in roles:
    print(f"{role['name']} — {role['alignment']} ({role['role_type']})")
```

### Python — отключение предупреждения InsecureRequestWarning

```python
import requests
import urllib3

# Отключить предупреждение о self-signed cert
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

response = requests.post(
    "https://<SERVER_IP>/api/games/import",
    headers={"X-API-Key": API_KEY},
    json=data,
    verify=False
)
```

### cURL

```bash
curl -k -X POST "https://<SERVER_IP>/api/games/import" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_names": ["МелонКО"],
    "alignment_win": "добро",
    "location": "Москва, Антикафе на Арбате",
    "game_number": 1,
    "players": [
      {
        "name": "Анна",
        "seat_number": 1,
        "role_start": "Дамочка",
        "role_end": "Дамочка",
        "alignment_end": "добро",
        "is_alive": true
      }
    ]
  }'
```

---

## ⚠️ Обработка ошибок

| HTTP-код | Причина | Что делать |
|----------|---------|------------|
| `400` | Ошибка импорта, возвращённая логикой API (например, неизвестная роль) | Проверьте имена ролей и данные партии |
| `401` | Неизвестный API-ключ | Проверьте ключ, обратитесь к администратору |
| `403` | Ключ отозван | Обратитесь к администратору за новым ключом |
| `422` | Тело запроса не проходит проверку схемы (включая отсутствующий обязательный заголовок) | Проверьте обязательные поля, значения и наличие `X-API-Key` |
| `500` | Ошибка сервера или базы данных | Сообщите администратору |
| `502` | Nginx не получил корректный ответ от API-сервиса | Сообщите администратору |
| `503` | Превышен настроенный лимит запросов Nginx | Подождите и повторите запрос позже |
| `504` | API-сервис не ответил за отведённое Nginx время | Повторите запрос позже или сообщите администратору |

---

## 🔒 Безопасность

- Все запросы передаются по HTTPS (TLS 1.2 или TLS 1.3)
- API-ключи хранятся в БД только как SHA-256 хеши
- Nginx применяет лимиты на IP: до 30 запросов/мин для импортов (burst 20) и 120 запросов/мин для остальных `/api/` маршрутов (burst 30); превышение лимита возвращает 503 при стандартной конфигурации Nginx.
- Не передавайте API-ключ в URL, логах или публичных репозиториях

---

## 📄 Автоматическая документация (Swagger/OpenAPI)

FastAPI предоставляет Swagger UI и OpenAPI-схему:

```
https://<SERVER_IP>/docs         — Swagger UI
https://<SERVER_IP>/openapi.json — OpenAPI JSON
```

Оба адреса доступны публично через Nginx без API-ключа; не передавайте им секреты. ReDoc не публикуется через текущую конфигурацию Nginx.

---

## 📄 Лицензия

GNU GPLv3
