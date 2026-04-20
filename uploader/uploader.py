#!/usr/bin/env python3
"""
BotC CSV Uploader — загрузка данных игр и ролей в базу данных Blood on the clocktower через REST API.

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
import logging
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Any

import requests
import urllib3
from dotenv import load_dotenv
from tqdm import tqdm

# Настройка logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

# Подавить предупреждения о self-signed SSL (когда SSL_VERIFY=false)
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Константы
API_URL_DEFAULT = "https://localhost:443"
API_IMPORT_ENDPOINT = "/api/import"
API_ROLES_IMPORT_ENDPOINT = "/api/roles/import"

# Валидация ролей (английские)
VALID_ALIGNMENT_ROLES = {"good", "evil", "neutral"}
VALID_ROLE_TYPES = {"Townsfolk", "Outsider", "Minion", "Demon", "Traveller"}

# Валидация игр (русские)
VALID_ALIGNMENTS_RU = {"добро", "зло", "нейтральный"}
VALID_ALIGNMENTS_WIN_RU = {"добро", "зло"}


def load_config() -> Dict[str, Any]:
    """Загрузка конфигурации из .env файла."""
    env_path = Path(__file__).parent / ".env"
    if not env_path.exists():
        logger.error("Файл .env не найден по пути %s", env_path)
        logger.error("Скопируйте .env.example в .env и настройте параметры API.")
        sys.exit(1)

    load_dotenv(env_path)

    config = {
        "api_url": os.getenv("API_URL", API_URL_DEFAULT),
        "api_key": os.getenv("API_KEY"),
        "ssl_verify": os.getenv("SSL_VERIFY", "false").lower() == "true",
    }

    if not config["api_key"]:
        logger.error("API_KEY не указан в .env файле")
        sys.exit(1)

    return config


def validate_csv_file(csv_path: str) -> Path:
    """Проверка существования CSV файла."""
    path = Path(csv_path)
    if not path.exists():
        logger.error("Файл '%s' не найден", csv_path)
        sys.exit(1)
    if not path.suffix.lower() == ".csv":
        logger.error("Файл должен иметь расширение .csv")
        sys.exit(1)
    return path


def parse_csv(csv_path: Path) -> List[Dict[str, Any]]:
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

        required_cols = {
            "game_date", "scenario_name", "location", "game_number",
            "storyteller_name", "alignment_win", "player_name",
            "role_start_name", "role_end_name", "alignment_end",
            "is_alive", "seat_number"
        }
        missing = required_cols - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Отсутствуют обязательные колонки: {', '.join(sorted(missing))}")

        row: dict[str | Any, str | Any]
        for row in reader:
            # Опциональные поля
            duration_val = row.get("duration", "").strip() or None
            notes_val: str | None = row.get("notes", "").strip() or None

            # Валидация alignment_win (только "добро" или "зло")
            alignment_win_val = row["alignment_win"].strip()
            if alignment_win_val not in VALID_ALIGNMENTS_WIN_RU:
                raise ValueError(
                    f"Неверный alignment_win: '{alignment_win_val}'. "
                    f"Допустимы: {', '.join(sorted(VALID_ALIGNMENTS_WIN_RU))}"
                )

            # Валидация alignment_end
            alignment_end_val = row["alignment_end"].strip()
            if alignment_end_val not in VALID_ALIGNMENTS_RU:
                raise ValueError(
                    f"Неверный alignment_end: '{alignment_end_val}'. "
                    f"Допустимы: {', '.join(sorted(VALID_ALIGNMENTS_RU))}"
                )

            # Ключ группировки: уникальная партия
            game_key = (
                row["game_date"].strip(),
                row["scenario_name"].strip(),
                row["storyteller_name"].strip(),
                alignment_win_val,
                row["location"].strip(),
                row["game_number"].strip(),
            )

            game = games_dict[game_key]
            game["game_date"] = row["game_date"].strip()
            game["scenario_name"] = row["scenario_name"].strip()
            game["storyteller_name"] = row["storyteller_name"].strip()
            game["alignment_win"] = alignment_win_val
            game["location"] = row["location"].strip()
            game_number_val = row["game_number"].strip()
            game["game_number"] = int(game_number_val) if game_number_val else None
            game["duration"] = duration_val
            game["notes"] = notes_val

            # Добавляем игрока
            seat_val = row.get("seat_number", "").strip()
            game["players"].append({
                "name": row["player_name"].strip(),
                "seat_number": int(seat_val) if seat_val else None,
                "role_start": row["role_start_name"].strip(),
                "role_end": row["role_end_name"].strip(),
                "alignment_end": alignment_end_val,
                "is_alive": row["is_alive"].strip().lower() in ("true", "1", "yes", "да"),
            })

    return list(games_dict.values())


def send_import(api_url: str, api_key: str, ssl_verify: bool, game_data: dict) -> Dict[str, Any]:
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
            return {"status": "error", "errors": [f"HTTP error: {e}"]}

    except requests.exceptions.ConnectionError:
        return {
            "status": "error",
            "errors": ["Connection failed. Check if container is running."],
        }

    except requests.exceptions.Timeout:
        return {"status": "error", "errors": ["Request timeout - API not responding"]}

    except requests.exceptions.RequestException as e:
        return {"status": "error", "errors": [f"Request error: {e}"]}


# ============================================================
#  Roles import
# ============================================================

def parse_roles_csv(csv_path: Path) -> List[Dict[str, Any]]:
    """
    Парсинг CSV файла ролей.

    Ожидаемые колонки: name, alignment, role_type (description — опционально)
    Колонки переводов: {lang}_name, {lang}_description (например, ru_name, ru_description)
    Все значения должны быть на английском (кроме переводов).

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
            logger.error("CSV должен содержать колонки: %s", ", ".join(sorted(expected)))
            logger.info("Найдены: %s", ", ".join(reader.fieldnames or []))
            sys.exit(1)

        has_description = "description" in (reader.fieldnames or [])

        # Определяем языки переводов из заголовков (паттерн: {lang}_name, {lang}_description)
        fieldnames = set(reader.fieldnames or [])
        translation_langs = set()
        for field in fieldnames:
            if field.endswith("_name") and field != "name":
                lang = field.rsplit("_", 1)[0]
                if f"{lang}_description" in fieldnames:
                    translation_langs.add(lang)

        for line_num, row in enumerate(reader, start=2):
            name = row["name"].strip()
            alignment = row["alignment"].strip()
            role_type = row["role_type"].strip()
            description = row.get("description", "").strip() if has_description else None

            if not name:
                errors.append(f"Строка {line_num}: пустое имя роли")
                continue

            if alignment not in VALID_ALIGNMENT_ROLES:
                errors.append(
                    f"Строка {line_num} ({name}): неверный alignment '{alignment}'. Допустимы: {', '.join(sorted(VALID_ALIGNMENT_ROLES))}")
                continue

            if role_type not in VALID_ROLE_TYPES:
                errors.append(
                    f"Строка {line_num} ({name}): неверный тип '{role_type}'. Допустимы: {', '.join(sorted(VALID_ROLE_TYPES))}")
                continue

            role_data = {"name": name, "alignment": alignment, "role_type": role_type}
            if description is not None:
                role_data["description"] = description if description else None

            # Собираем переводы
            translations = {}
            for lang in sorted(translation_langs):
                tr_name = row.get(f"{lang}_name", "").strip()
                tr_description = row.get(f"{lang}_description", "").strip() or None

                if tr_name:
                    tr_data = {"name": tr_name}
                    if tr_description:
                        tr_data["description"] = tr_description
                    translations[lang] = tr_data

            if translations:
                role_data["translations"] = translations

            roles.append(role_data)

    if errors:
        logger.warning("Ошибки валидации ролей:")
        for err in errors:
            logger.warning("     - %s", err)
        if not roles:
            logger.error("Нет валидных ролей для импорта.")
            sys.exit(1)

    return roles


def send_roles_import(api_url: str, api_key: str, ssl_verify: bool, roles: List[Dict[str, Any]]) -> Dict[str, Any]:
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
            return {"status": "error", "errors": [f"HTTP error: {e}"]}

    except requests.exceptions.ConnectionError:
        return {
            "status": "error",
            "errors": ["Connection failed. Check if container is running."],
        }

    except requests.exceptions.Timeout:
        return {"status": "error", "errors": ["Request timeout - API not responding"]}

    except requests.exceptions.RequestException as e:
        return {"status": "error", "errors": [f"Request error: {e}"]}


def main() -> None:
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
    logger.info("Загрузка конфигурации...")
    config = load_config()

    # Проверка CSV файла
    csv_file = validate_csv_file(args.csv_file)
    logger.info("Файл для загрузки: %s", csv_file)

    if args.roles:
        # === Импорт ролей ===
        logger.info("Парсинг CSV ролей...")
        try:
            roles = parse_roles_csv(csv_file)
        except Exception as e:
            logger.error("Ошибка парсинга CSV: %s", e)
            sys.exit(1)

        logger.info("Найдено ролей: %d", len(roles))

        logger.info("Отправка %d ролей на импорт...", len(roles))
        result = send_roles_import(
            config["api_url"],
            config["api_key"],
            config["ssl_verify"],
            roles,
        )

        if result["status"] == "ok":
            logger.info("Импорт ролей успешно завершён!")
            logger.info("   Создано: %d", result.get("roles_created", 0))
            logger.info("   Обновлено: %d", result.get("roles_updated", 0))
        else:
            logger.error("Импорт ролей завершён с ошибками!")
            logger.info("   Создано: %d", result.get("roles_created", 0))
            logger.info("   Обновлено: %d", result.get("roles_updated", 0))
            for err in result.get("errors", []):
                logger.error("     - %s", err)
            sys.exit(1)

    else:
        # === Импорт партий ===
        logger.info("Парсинг CSV файла...")
        try:
            games = parse_csv(csv_file)
        except Exception as e:
            logger.error("Ошибка парсинга CSV: %s", e)
            sys.exit(1)

        logger.info("Найдено партий: %d", len(games))

        # Отправка каждой партии на импорт
        total_games = 0
        total_players = 0
        has_errors = False

        for i, game in enumerate(tqdm(games, desc="Импорт партий", unit="партия"), 1):
            scenario = game["scenario_name"]
            date = game["game_date"]
            player_count = len(game["players"])
            logger.info("[%d/%d] Импорт партии: %s (%s, %d игроков)...", i, len(games), scenario, date, player_count)

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
                logger.info("  Успешно! Создано новых игроков: %d", players_created)
                if result.get("game_id"):
                    logger.info("  ID партии: %s", result["game_id"])
            else:
                has_errors = True
                errors = result.get("errors", ["Неизвестная ошибка"])
                logger.error("  Ошибка импорта:")
                for error in errors:
                    logger.error("     - %s", error)

        # Итоговый отчёт
        logger.info("=" * 60)
        if has_errors:
            logger.warning("Импорт завершён с ошибками!")
            logger.info("   Успешно импортировано: %d партий", total_games)
            logger.info("   Создано игроков: %d", total_players)
            sys.exit(1)
        else:
            logger.info("Импорт успешно завершён!")
            logger.info("   Импортировано партий: %d", total_games)
            logger.info("   Создано игроков: %d", total_players)


if __name__ == "__main__":
    main()
