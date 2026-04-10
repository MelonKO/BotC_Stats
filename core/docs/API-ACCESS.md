# BotC API — Документация для внешних пользователей

> REST API для импорта партий Blood on the Clocktower через HTTPS.

---

## 📋 Обзор

API позволяет удалённо импортировать данные партий в базу данных без прямого доступа к PostgreSQL.

| Параметр | Значение |
|----------|----------|
| **URL** | `https://<SERVER_IP>` |
| **Протокол** | HTTPS (TLS 1.2+) |
| **Аутентификация** | API-ключ в заголовке `X-API-Key` |
| **Формат** | JSON |
| **Сертификат** | Самоподписанный (используйте `verify=False`) |

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

### `POST /api/import` — Импорт партии

Отправляет данные одной партии в базу данных.

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
    "storyteller_name": "МелонКО",
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
| `storyteller_name` | string | ✅ | Имя рассказчика |
| `alignment_win` | string | ✅ | Победивший alignment: `добро` или `зло` |
| `location` | string | ✅ | Место проведения |
| `game_number` | int | ✅ | Номер партии в рамках встречи |
| `duration` | string | | Длительность (HH:MM:SS) |
| `notes` | string | | Заметки к партии |
| `players` | array | ✅ | Список игроков (мин. 1, макс. 30) |

| Поле (в `players[]`) | Тип | Обязательное | Описание |
|----------------------|-----|--------------|----------|
| `name` | string | ✅ | Имя игрока |
| `seat_number` | int | | Номер места |
| `role_start` | string | ✅ | Начальная роль (русское название) |
| `role_end` | string | ✅ | Конечная роль (русское название) |
| `alignment_end` | string | ✅ | Конечный alignment: `добро`, `зло` или `нейтральный` |
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

**Ответ (ошибка валидации, 400):**
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
| `translations` | object | | Словарь переводов по языкам (`"ru"`, `"de"`, ...) |
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
    verify=False,
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

Возвращает все роли, которые можно использовать при импорте.

**Заголовки:**
```
X-API-Key: <ваш_ключ>
```

**Ответ (200):**
```json
{
    "roles": [
        {"name": "Gambler", "alignment": "good", "role_type": "Townsfolk"},
        {"name": "Amnesiac", "alignment": "good", "role_type": "Townsfolk"},
        {"name": "Chambermaid", "alignment": "good", "role_type": "Townsfolk"},
        {"name": "Librarian", "alignment": "good", "role_type": "Townsfolk"},
        {"name": "Investigator", "alignment": "good", "role_type": "Townsfolk"},
        {"name": "Pukka", "alignment": "evil", "role_type": "Demon"},
        {"name": "Slayer", "alignment": "evil", "role_type": "Minion"}
    ]
}
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

API_URL = "https://<SERVER_IP>/api/import"
API_KEY = "sk-EXAMPLE_DO_NOT_USE"  # Ваш API-ключ

data = {
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_name": "МелонКО",
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
    "https://<SERVER_IP>/api/import",
    headers={"X-API-Key": API_KEY},
    json=data,
    verify=False
)
```

### cURL

```bash
curl -k -X POST "https://<SERVER_IP>/api/import" \
  -H "X-API-Key: sk-EXAMPLE_DO_NOT_USE" \
  -H "Content-Type: application/json" \
  -d '{
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_name": "МелонКО",
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
| `400` | Ошибка валидации данных | Проверьте формат запроса, имена ролей |
| `401` | Невалидный API-ключ | Проверьте ключ, обратитесь к администратору |
| `403` | Ключ отозван | Обратитесь к администратору за новым ключом |
| `429` | Rate limit (10 запросов/мин) | Подождите и повторите |
| `500` | Ошибка сервера | Сообщите администратору |

---

## 🔒 Безопасность

- Все запросы передаются по HTTPS (TLS 1.2+)
- API-ключи хранятся в БД только как SHA-256 хеши
- Rate limiting: 10 запросов в минуту на один IP
- Не передавайте API-ключ в URL, логах или публичных репозиториях

---

## 📄 Автоматическая документация (Swagger/OpenAPI)

FastAPI автоматически генерирует интерактивную документацию:

```
https://<SERVER_IP>/docs      — Swagger UI
https://<SERVER_IP>/redoc     — ReDoc
```

> **Примечание:** Эти эндпоинты доступны только внутри сервера или через SSH-туннель.

---

## 📄 Лицензия

GNU GPLv3
