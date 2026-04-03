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
sk-a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4
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
    "color_win": "синий",
    "players": [
        {
            "name": "Анна Никитина",
            "role_start": "Дамочка",
            "role_end": "Дамочка",
            "color_end": "синий",
            "is_alive": true
        },
        {
            "name": "Борис Петров",
            "role_start": "Азартный игрок",
            "role_end": "Азартный игрок",
            "color_end": "синий",
            "is_alive": true
        },
        {
            "name": "Виктор Сидоров",
            "role_start": "Убийца",
            "role_end": "Убийца",
            "color_end": "красный",
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
| `color_win` | string | ✅ | Победившая команда: `синий` или `красный` |
| `players` | array | ✅ | Список игроков (мин. 1, макс. 30) |

| Поле (в `players[]`) | Тип | Обязательное | Описание |
|----------------------|-----|--------------|----------|
| `name` | string | ✅ | Имя игрока |
| `role_start` | string | ✅ | Начальная роль |
| `role_end` | string | ✅ | Конечная роль (может совпадать с начальной) |
| `color_end` | string | ✅ | Команда в конце: `синий` или `красный` |
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
        {"name": "Азартный игрок", "color": "синий", "role_type": "Горожанин"},
        {"name": "Амнезиак", "color": "синий", "role_type": "Горожанин"},
        {"name": "Дамочка", "color": "синий", "role_type": "Изгой"},
        {"name": "Политик", "color": "синий", "role_type": "Изгой"},
        {"name": "Пукка", "color": "красный", "role_type": "Демон"},
        {"name": "Убийца", "color": "красный", "role_type": "Приспешник"}
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
API_KEY = "sk-a1b2c3d4e5f6..."  # Ваш API-ключ

data = {
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_name": "МелонКО",
    "color_win": "синий",
    "players": [
        {
            "name": "Анна",
            "role_start": "Дамочка",
            "role_end": "Дамочка",
            "color_end": "синий",
            "is_alive": True
        },
        {
            "name": "Борис",
            "role_start": "Убийца",
            "role_end": "Убийца",
            "color_end": "красный",
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
API_KEY = "sk-a1b2c3d4e5f6..."

response = requests.get(
    API_URL,
    headers={"X-API-Key": API_KEY},
    verify=False
)

roles = response.json()["roles"]
for role in roles:
    print(f"{role['name']} — {role['color']} ({role['role_type']})")
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
  -H "X-API-Key: sk-a1b2c3d4e5f6..." \
  -H "Content-Type: application/json" \
  -d '{
    "game_date": "2026-01-15",
    "scenario_name": "Вселенная зла",
    "storyteller_name": "МелонКО",
    "color_win": "синий",
    "players": [
      {
        "name": "Анна",
        "role_start": "Дамочка",
        "role_end": "Дамочка",
        "color_end": "синий",
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

MIT — используйте, модифицируйте, делитесь.
