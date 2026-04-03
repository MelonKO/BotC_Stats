#!/usr/bin/env python3
"""
BotC CSV Uploader — загрузка данных игр в базу данных Blood on the Clocktower через REST API.

Использование:
    python uploader.py <путь_к_csv_файлу>

Требования:
    - Файл .env с параметрами API
    - CSV файл в формате games_import_staging
    - Запущенный Docker-контейнер с API endpoint
"""

import sys
import os
import csv
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
        "color_win": None,
        "players": [],
    })

    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        for row in reader:
            # Ключ группировки: уникальная партия
            game_key = (
                row["game_date"].strip(),
                row["scenario_name"].strip(),
                row["storyteller_name"].strip(),
                row["color_win"].strip(),
            )

            game = games_dict[game_key]
            game["game_date"] = row["game_date"].strip()
            game["scenario_name"] = row["scenario_name"].strip()
            game["storyteller_name"] = row["storyteller_name"].strip()
            game["color_win"] = row["color_win"].strip()

            # Добавляем игрока
            game["players"].append({
                "name": row["player_name"].strip(),
                "role_start": row["role_start_name"].strip(),
                "role_end": row["role_end_name"].strip(),
                "color_end": row["color_end"].strip(),
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


def main():
    if len(sys.argv) != 2:
        print("Использование: python uploader.py <путь_к_csv_файлу>")
        print("Пример: python uploader.py ../test_sample.csv")
        sys.exit(1)

    csv_path = sys.argv[1]

    # Загрузка конфигурации
    print("Загрузка конфигурации...")
    config = load_config()

    # Проверка CSV файла
    csv_file = validate_csv_file(csv_path)
    print(f"Файл для загрузки: {csv_file}")

    # Парсинг CSV
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
