#!/usr/bin/env python3
"""
BotC CSV Uploader — загрузка данных игр и ролей в базу данных Blood on the Clocktower через REST API.

Использование:
    python uploader.py <путь_к_csv_файлу>           # Импорт партии (по умолчанию)
    python uploader.py --roles <путь_к_roles_csv>   # Импорт таблицы ролей

Требования:
    - Файл .env с параметрами API
    - CSV файл в соответствующем формате
    - Запущенный Docker-контейнер с API endpoint
"""

import sys
import os
import csv
import argparse
import warnings
from pathlib import Path
from collections import defaultdict

import requests
import urllib3
from dotenv import load_dotenv

# Подавить предупреждения о self-signed SSL (когда SSL_VERIFY=false)
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


def load_config() -> dict:
    """Загрузка конфигурации из .env файла."""
    env_path = Path(__file__).parent / ".env"
    if not env_path.exists():
        print(f"Ошибка: Файл .env не найден по пути {env_path}")
        print("Скопируйте .env.example в .env и настройте параметры API.")
        sys.exit(1)

    load_dotenv(env_path)

    config = {
        "api_url": os.getenv("API_URL", "https://localhost:443"),
        "api_key": os.getenv("API_KEY"),
        "ssl_verify": os.getenv("SSL_VERIFY", "false").lower() == "true",
    }

    if not config["api_key"]:
        print("Ошибка: API_KEY не указан в .env файле")
        sys.exit(1)

    return config


def validate_csv_file(csv_path: str) -> Path:
    """Проверка существования CSV файла."""
    path = Path(csv_path)
    if not path.exists():
        print(f"Ошибка: Файл '{csv_path}' не найден")
        sys.exit(1)
    if not path.suffix.lower() == ".csv":
        print(f"Ошибка: Файл должен иметь расширение .csv")
        sys.exit(1)
    return path


def parse_csv(csv_path: Path) -> list[dict]:
    """
    Парсинг CSV файла в список партий.

    CSV содержит несколько строк на партию (по одной на игрока).
    Группируем по уникальной комбинации (game_date, scenario_name, storyteller_name, color_win).

    Returns:
        Список словарей, каждый представляет одну партию в формате API.
    """
    games_dict = defaultdict(lambda: {
        "game_date": None,
        "scenario_name": None,
        "storyteller_name": None,
        "alignment_win": None,
        "location": None,
        "game_number": None,
        "duration": None,
        "notes": None,
        "players": [],
    })

    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        for row in reader:
            # Опциональные поля
            duration_val = row.get("duration", "").strip() or None
            notes_val = row.get("notes", "").strip() or None

            # Ключ группировки: уникальная партия
            game_key = (
                row["game_date"].strip(),
                row["scenario_name"].strip(),
                row["storyteller_name"].strip(),
                row["alignment_win"].strip(),
                row["location"].strip(),
                row["game_number"].strip(),
            )

            game = games_dict[game_key]
            game["game_date"] = row["game_date"].strip()
            game["scenario_name"] = row["scenario_name"].strip()
            game["storyteller_name"] = row["storyteller_name"].strip()
            game["alignment_win"] = row["alignment_win"].strip()
            game["location"] = row["location"].strip()
            game["game_number"] = int(row["game_number"].strip())
            game["duration"] = duration_val
            game["notes"] = notes_val

            # Добавляем игрока
            seat_val = row.get("seat_number", "").strip()
            game["players"].append({
                "name": row["player_name"].strip(),
                "seat_number": int(seat_val) if seat_val else None,
                "role_start": row["role_start_name"].strip(),
                "role_end": row["role_end_name"].strip(),
                "alignment_end": row["alignment_end"].strip(),
                "is_alive": row["is_alive"].strip().lower() in ("true", "1", "yes", "да"),
            })

    return list(games_dict.values())


def send_import(api_url: str, api_key: str, ssl_verify: bool, game_data: dict) -> dict:
    """
    Отправка одной партии на импорт через API.

    Returns:
        Словарь с результатом импорта.
    """
    url = f"{api_url.rstrip('/')}/api/import"
    headers = {
        "X-API-Key": api_key,
        "Content-Type": "application/json",
    }

    try:
        response = requests.post(
            url,
            json=game_data,
            headers=headers,
            verify=ssl_verify,
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    except requests.exceptions.HTTPError as e:
        # Пытаемся извлечь сообщение об ошибке из ответа
        try:
            error_data = response.json()
            error_msg = error_data.get("detail", str(e))
            if isinstance(error_msg, dict):
                errors = error_msg.get("errors", [str(error_msg)])
                return {"status": "error", "errors": errors}
            return {"status": "error", "errors": [str(error_msg)]}
        except Exception:
            return {"status": "error", "errors": [f"HTTP ошибка: {e}"]}

    except requests.exceptions.ConnectionError:
        return {
            "status": "error",
            "errors": [f"Не удалось подключиться к API по адресу {url}. Проверьте, что контейнер запущен."],
        }

    except requests.exceptions.Timeout:
        return {"status": "error", "errors": ["Превышено время ожидания ответа от API"]}

    except requests.exceptions.RequestException as e:
        return {"status": "error", "errors": [f"Ошибка запроса: {e}"]}


# ============================================================
#  Roles import
# ============================================================

VALID_ALIGNMENT = {"good", "evil", "neutral"}
VALID_ROLE_TYPES = {"Townsfolk", "Outsider", "Minion", "Demon", "Traveller"}


def parse_roles_csv(csv_path: Path) -> list[dict]:
    """
    Парсинг CSV файла ролей.

    Ожидаемые колонки: name, alignment, role_type (description — опционально)
    Все значения должны быть на английском.

    Returns:
        Список словарей с валидированными ролями.
    """
    roles = []
    errors = []

    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        # Проверка заголовков
        expected = {"name", "alignment", "role_type"}
        if not expected.issubset(set(reader.fieldnames or [])):
            print(f"Ошибка: CSV должен содержать колонки: {', '.join(sorted(expected))}")
            print(f"Найдены: {', '.join(reader.fieldnames or [])}")
            sys.exit(1)

        has_description = "description" in (reader.fieldnames or [])

        for line_num, row in enumerate(reader, start=2):
            name = row["name"].strip()
            alignment = row["alignment"].strip()
            role_type = row["role_type"].strip()
            description = row.get("description", "").strip() if has_description else None

            if not name:
                errors.append(f"Строка {line_num}: пустое имя роли")
                continue

            if alignment not in VALID_ALIGNMENT:
                errors.append(f"Строка {line_num} ({name}): неверный alignment '{alignment}'. Допустимы: {', '.join(sorted(VALID_ALIGNMENT))}")
                continue

            if role_type not in VALID_ROLE_TYPES:
                errors.append(f"Строка {line_num} ({name}): неверный тип '{role_type}'. Допустимы: {', '.join(sorted(VALID_ROLE_TYPES))}")
                continue

            role_data = {"name": name, "alignment": alignment, "role_type": role_type}
            if description is not None:
                role_data["description"] = description if description else None

            roles.append(role_data)

    if errors:
        print("⚠️  Ошибки валидации ролей:")
        for err in errors:
            print(f"     - {err}")
        if not roles:
            print("Нет валидных ролей для импорта.")
            sys.exit(1)

    return roles


def send_roles_import(api_url: str, api_key: str, ssl_verify: bool, roles: list[dict]) -> dict:
    """
    Отправка списка ролей на импорт через API.

    Returns:
        Словарь с результатом импорта ролей.
    """
    url = f"{api_url.rstrip('/')}/api/roles/import"
    headers = {
        "X-API-Key": api_key,
        "Content-Type": "application/json",
    }

    try:
        response = requests.post(
            url,
            json={"roles": roles},
            headers=headers,
            verify=ssl_verify,
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    except requests.exceptions.HTTPError as e:
        try:
            error_data = response.json()
            error_msg = error_data.get("detail", str(e))
            if isinstance(error_msg, dict):
                errors = error_msg.get("errors", [str(error_msg)])
                return {"status": "error", "errors": errors}
            return {"status": "error", "errors": [str(error_msg)]}
        except Exception:
            return {"status": "error", "errors": [f"HTTP ошибка: {e}"]}

    except requests.exceptions.ConnectionError:
        return {
            "status": "error",
            "errors": [f"Не удалось подключиться к API по адресу {url}. Проверьте, что контейнер запущен."],
        }

    except requests.exceptions.Timeout:
        return {"status": "error", "errors": ["Превышено время ожидания ответа от API"]}

    except requests.exceptions.RequestException as e:
        return {"status": "error", "errors": [f"Ошибка запроса: {e}"]}


def main():
    parser = argparse.ArgumentParser(
        description="BotC CSV Uploader — импорт партий и ролей",
    )
    parser.add_argument("csv_file", help="Путь к CSV файлу")
    parser.add_argument(
        "--roles",
        action="store_true",
        help="Режим импорта таблицы ролей (по умолчанию — импорт партий)",
    )
    args = parser.parse_args()

    # Загрузка конфигурации
    print("Загрузка конфигурации...")
    config = load_config()

    # Проверка CSV файла
    csv_file = validate_csv_file(args.csv_file)
    print(f"Файл для загрузки: {csv_file}")

    if args.roles:
        # === Импорт ролей ===
        print("Парсинг CSV ролей...")
        try:
            roles = parse_roles_csv(csv_file)
        except Exception as e:
            print(f"Ошибка парсинга CSV: {e}")
            sys.exit(1)

        print(f"Найдено ролей: {len(roles)}")

        print(f"Отправка {len(roles)} ролей на импорт...")
        result = send_roles_import(
            config["api_url"],
            config["api_key"],
            config["ssl_verify"],
            roles,
        )

        if result["status"] == "ok":
            print("\n✅ Импорт ролей успешно завершён!")
            print(f"   Создано: {result.get('roles_created', 0)}")
            print(f"   Обновлено: {result.get('roles_updated', 0)}")
        else:
            print("\n❌ Импорт ролей завершён с ошибками!")
            print(f"   Создано: {result.get('roles_created', 0)}")
            print(f"   Обновлено: {result.get('roles_updated', 0)}")
            for err in result.get("errors", []):
                print(f"     - {err}")
            sys.exit(1)

    else:
        # === Импорт партий ===
        print("Парсинг CSV файла...")
        try:
            games = parse_csv(csv_file)
        except Exception as e:
            print(f"Ошибка парсинга CSV: {e}")
            sys.exit(1)

        print(f"Найдено партий: {len(games)}")

        # Отправка каждой партии на импорт
        total_games = 0
        total_players = 0
        has_errors = False

        for i, game in enumerate(games, 1):
            scenario = game["scenario_name"]
            date = game["game_date"]
            player_count = len(game["players"])
            print(f"\n[{i}/{len(games)}] Импорт партии: {scenario} ({date}, {player_count} игроков)...")

            result = send_import(
                config["api_url"],
                config["api_key"],
                config["ssl_verify"],
                game,
            )

            if result["status"] == "ok":
                total_games += 1
                players_created = result.get("players_created", 0)
                total_players += players_created
                print(f"  ✅ Успешно! Создано новых игроков: {players_created}")
                if result.get("game_id"):
                    print(f"  🆔 ID партии: {result['game_id']}")
            else:
                has_errors = True
                errors = result.get("errors", ["Неизвестная ошибка"])
                print(f"  ❌ Ошибка импорта:")
                for error in errors:
                    print(f"     - {error}")

        # Итоговый отчёт
        print("\n" + "=" * 60)
        if has_errors:
            print("⚠️  Импорт завершён с ошибками!")
            print(f"   Успешно импортировано: {total_games} партий")
            print(f"   Создано игроков: {total_players}")
            sys.exit(1)
        else:
            print("✅ Импорт успешно завершён!")
            print(f"   Импортировано партий: {total_games}")
            print(f"   Создано игроков: {total_players}")


if __name__ == "__main__":
    main()
