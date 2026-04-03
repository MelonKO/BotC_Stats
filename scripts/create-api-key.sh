#!/bin/bash
# ============================================================
#  BotC — Управление API-ключами
# ============================================================
#
#  Использование:
#    bash create-api-key.sh create  "Имя Фамилия"  [contact]
#    bash create-api-key.sh revoke  <api_key>
#    bash create-api-key.sh list
#    bash create-api-key.sh info    <api_key>
#
#  Требования:
#    - Доступ к серверу через SSH
#    - Docker Compose запущен
# ============================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Настройки подключения к БД
DB_USER="postgres"
DB_NAME="botc_stats"
CONTAINER="botc-postgres"

# ============================================================
#  Утилиты
# ============================================================

run_sql() {
    docker exec "$CONTAINER" psql -U "$DB_USER" -d "$DB_NAME" -t -A -c "$1"
}

generate_key() {
    # Генерируем ключ формата sk-<32 random hex chars>
    echo "sk-$(openssl rand -hex 16)"
}

hash_key() {
    # SHA-256 хеш
    echo -n "$1" | sha256sum | awk '{print $1}'
}

# ============================================================
#  Команды
# ============================================================

cmd_create() {
    local owner_name="${1:-}"
    local owner_contact="${2:-}"

    if [[ -z "$owner_name" ]]; then
        echo -e "${RED}Ошибка: укажите имя владельца${NC}"
        echo "Использование: $0 create \"Имя Фамилия\" [contact]"
        exit 1
    fi

    local api_key
    api_key=$(generate_key)
    local key_hash
    key_hash=$(hash_key "$api_key")

    run_sql "INSERT INTO api_keys (key_hash, owner_name, owner_contact)
             VALUES ('$key_hash', '$owner_name', '${owner_contact:-}');" >/dev/null

    echo ""
    echo -e "${GREEN}============================================${NC}"
    echo -e "${GREEN}  API-ключ создан!${NC}"
    echo -e "${GREEN}============================================${NC}"
    echo ""
    echo -e "Владелец: ${YELLOW}${owner_name}${NC}"
    echo -e "Контакт:  ${YELLOW}${owner_contact:-не указан}${NC}"
    echo ""
    echo -e "${GREEN}API-ключ (сохраните!):${NC}"
    echo -e "${YELLOW}${api_key}${NC}"
    echo ""
    echo -e "${YELLOW}ВНИМАНИЕ: Ключ показывается только один раз!${NC}"
    echo -e "Передайте его пользователю по защищённому каналу."
    echo ""
}

cmd_revoke() {
    local api_key="${1:-}"

    if [[ -z "$api_key" ]]; then
        echo -e "${RED}Ошибка: укажите ключ для отзыва${NC}"
        echo "Использование: $0 revoke <api_key>"
        exit 1
    fi

    local key_hash
    key_hash=$(hash_key "$api_key")

    local found
    found=$(run_sql "SELECT owner_name, active FROM api_keys WHERE key_hash = '$key_hash';")

    if [[ -z "$found" ]]; then
        echo -e "${RED}Ключ не найден в базе данных.${NC}"
        exit 1
    fi

    echo -e "Найден: ${YELLOW}${found}${NC}"
    echo -e "${YELLOW}Отозвать этот ключ? (y/N)${NC}"
    read -r confirm
    if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
        echo "Отменено."
        exit 0
    fi

    run_sql "UPDATE api_keys SET active = false, revoked_at = now() WHERE key_hash = '$key_hash';" >/dev/null
    echo -e "${GREEN}Ключ отозван.${NC}"
}

cmd_list() {
    echo ""
    echo -e "${GREEN}============================================${NC}"
    echo -e "${GREEN}  Активные API-ключи${NC}"
    echo -e "${GREEN}============================================${NC}"
    echo ""

    run_sql "SELECT
        owner_name,
        owner_contact,
        active,
        created_at::date,
        last_used_at::date as last_used,
        CASE WHEN revoked_at IS NOT NULL THEN revoked_at::date ELSE '-' END as revoked
    FROM api_keys
    ORDER BY created_at DESC;"

    echo ""
}

cmd_info() {
    local api_key="${1:-}"

    if [[ -z "$api_key" ]]; then
        echo -e "${RED}Ошибка: укажите ключ${NC}"
        echo "Использование: $0 info <api_key>"
        exit 1
    fi

    local key_hash
    key_hash=$(hash_key "$api_key")

    local info
    info=$(run_sql "SELECT
        owner_name,
        owner_contact,
        active,
        created_at,
        last_used_at,
        revoked_at
    FROM api_keys WHERE key_hash = '$key_hash';")

    if [[ -z "$info" ]]; then
        echo -e "${RED}Ключ не найден.${NC}"
        exit 1
    fi

    echo ""
    echo -e "${GREEN}Информация о ключе:${NC}"
    echo "$info"
    echo ""
}

# ============================================================
#  Main
# ============================================================

if [[ $# -lt 1 ]]; then
    echo -e "${YELLOW}Использование:${NC}"
    echo "  $0 create  \"Имя Фамилия\"  [contact]   — создать ключ"
    echo "  $0 revoke  <api_key>                    — отозвать ключ"
    echo "  $0 list                                  — список всех ключей"
    echo "  $0 info    <api_key>                    — информация о ключе"
    exit 1
fi

command="$1"
shift

case "$command" in
    create)
        cmd_create "${1:-}" "${2:-}"
        ;;
    revoke)
        cmd_revoke "${1:-}"
        ;;
    list)
        cmd_list
        ;;
    info)
        cmd_info "${1:-}"
        ;;
    *)
        echo -e "${RED}Неизвестная команда: $command${NC}"
        exit 1
        ;;
esac
