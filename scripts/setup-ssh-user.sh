#!/bin/bash
# ============================================================
#  BotC — Создание SSH-пользователя для туннеля к PostgreSQL
# ============================================================
#
#  Запуск на УДАЛЁННОМ сервере (где развёрнут Docker):
#    sudo bash setup-ssh-user.sh <username>
#
#  Пример:
#    sudo bash setup-ssh-user.sh botc-ssh
#
#  Скрипт:
#    1. Создаёт системного пользователя без домашнего каталога
#    2. Запрещает интерактивный shell (только SSH-туннель)
#    3. Настраивает ~/.ssh/authorized_keys для подключения по ключу
#    4. Генерирует SSH-ключ на клиенте (опционально)
# ============================================================

set -euo pipefail

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Проверка прав
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}Ошибка: запустите скрипт от root (sudo)${NC}"
    exit 1
fi

# Проверка аргументов
if [[ $# -lt 1 ]]; then
    echo -e "${YELLOW}Использование: sudo bash $0 <username> [path_to_public_key]${NC}"
    echo ""
    echo "Примеры:"
    echo "  sudo bash $0 botc-ssh                           # Интерактивная генерация ключа"
    echo "  sudo bash $0 botc-ssh ~/.ssh/botc-tunnel.pub    # С готовым публичным ключом"
    exit 1
fi

SSH_USER="$1"
SSH_KEY_FILE="${2:-}"

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Настройка SSH-пользователя: ${SSH_USER}${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""

# 1. Создание пользователя
echo -e "[1/5] Создание пользователя ${SSH_USER}..."
if id "$SSH_USER" &>/dev/null; then
    echo -e "${YELLOW}  Пользователь ${SSH_USER} уже существует. Пропускаем.${NC}"
else
    useradd \
        --system \
        --no-create-home \
        --shell /bin/false \
        --comment "SSH tunnel user for BotC PostgreSQL" \
        "$SSH_USER"
    echo -e "${GREEN}  Пользователь создан.${NC}"
fi

# 2. Создание .ssh директории
echo -e "[2/5] Настройка SSH-директории..."
SSH_DIR="/home/$SSH_USER"
mkdir -p "$SSH_DIR/.ssh"
chmod 700 "$SSH_DIR/.ssh"
chown "$SSH_USER:$SSH_USER" "$SSH_DIR" 2>/dev/null || chown "$SSH_USER" "$SSH_DIR" 2>/dev/null || true
chown "$SSH_USER:$SSH_USER" "$SSH_DIR/.ssh" 2>/dev/null || chown "$SSH_USER" "$SSH_DIR/.ssh" 2>/dev/null || true

# 3. Настройка authorized_keys
echo -e "[3/5] Настройка authorized_keys..."

if [[ -n "$SSH_KEY_FILE" ]]; then
    # Использовать готовый публичный ключ
    if [[ ! -f "$SSH_KEY_FILE" ]]; then
        echo -e "${RED}  Ошибка: файл ключа не найден: $SSH_KEY_FILE${NC}"
        exit 1
    fi
    cat "$SSH_KEY_FILE" >> "$SSH_DIR/.ssh/authorized_keys"
    echo -e "${GREEN}  Ключ добавлен из: $SSH_KEY_FILE${NC}"
else
    # Генерация нового ключа
    echo -e "${YELLOW}  Генерация нового SSH-ключа...${NC}"
    KEY_FILE="/tmp/botc-ssh-key-$SSH_USER"

    ssh-keygen -t ed25519 \
        -C "botc-tunnel@$SSH_USER" \
        -f "$KEY_FILE" \
        -N "" \
        -q

    cat "${KEY_FILE}.pub" >> "$SSH_DIR/.ssh/authorized_keys"

    echo ""
    echo -e "${GREEN}  Ключ сгенерирован!${NC}"
    echo -e "${YELLOW}  Скопируйте закрытый ключ на клиентскую машину:${NC}"
    echo ""
    echo -e "    ${GREEN}Клиентская команда (для подключения):${NC}"
    echo -e "    ssh -i ${KEY_FILE} -L 5432:localhost:5432 ${SSH_USER}@<SERVER_IP>"
    echo ""
    echo -e "${YELLOW}  ВНИМАНИЕ: файл закрытого ключа: ${KEY_FILE}${NC}"
    echo -e "${YELLOW}  Переместите его в безопасное место и удалите с сервера!${NC}"
fi

chmod 600 "$SSH_DIR/.ssh/authorized_keys"
chown "$SSH_USER:$SSH_USER" "$SSH_DIR/.ssh/authorized_keys" 2>/dev/null || chown "$SSH_USER" "$SSH_DIR/.ssh/authorized_keys" 2>/dev/null || true

# 4. Настройка SSH-конфигурации (ForceCommand для ограничения доступа)
echo -e "[4/5] Настройка ограничений SSH..."

SSH_CONFIG="/etc/ssh/sshd_config.d/${SSH_USER}.conf"
if [[ ! -d "/etc/ssh/sshd_config.d" ]]; then
    mkdir -p /etc/ssh/sshd_config.d
fi

cat > "$SSH_CONFIG" <<EOF
# BotC SSH tunnel user configuration
Match User ${SSH_USER}
    ForceCommand /bin/false
    PermitOpen localhost:5432
    AllowTcpForwarding yes
    PermitTTY no
    X11Forwarding no
EOF

echo -e "${GREEN}  SSH-ограничения настроены: только туннель к localhost:5432${NC}"

# 5. Перезапуск SSH
echo -e "[5/5] Перезапуск SSH-сервиса..."
if command -v systemctl &>/dev/null; then
    systemctl restart sshd 2>/dev/null || systemctl restart ssh 2>/dev/null || true
    echo -e "${GREEN}  SSH перезапущен через systemctl.${NC}"
elif command -v service &>/dev/null; then
    service sshd restart 2>/dev/null || service ssh restart 2>/dev/null || true
    echo -e "${GREEN}  SSH перезапущен через service.${NC}"
else
    echo -e "${YELLOW}  Перезапустите SSH вручную: systemctl restart sshd${NC}"
fi

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Готово!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo -e "Подключение с клиентской машины:"
echo ""
echo -e "  ${YELLOW}ssh -L 5432:localhost:5432 ${SSH_USER}@<SERVER_IP>${NC}"
echo ""
echo -e "После установления туннеля:"
echo ""
echo -e "  ${YELLOW}psql -h localhost -p 5432 -U botc_user -d botc_stats${NC}"
echo ""
echo -e "${YELLOW}Для постоянного туннеля используйте autossh или настройте SSH config.${NC}"
echo ""
