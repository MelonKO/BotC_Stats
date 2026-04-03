#!/bin/bash
# ============================================================
#  BotC — Генерация самоподписанного SSL-сертификата для Nginx
# ============================================================
#
#  Запуск на сервере (или локально, затем скопировать файлы):
#    bash scripts/generate-cert.sh
#
#  Результат:
#    nginx/ssl/cert.pem — сертификат (365 дней)
#    nginx/ssl/key.pem  — закрытый ключ
# ============================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SSL_DIR="$SCRIPT_DIR/../nginx/ssl"

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Генерация самоподписанного SSL-сертификата${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""

# Создание директории
mkdir -p "$SSL_DIR"

# Генерация
echo -e "[1/1] Генерация ключей (RSA 2048, 365 дней)..."

openssl req -x509 \
    -newkey rsa:2048 \
    -keyout "$SSL_DIR/key.pem" \
    -out "$SSL_DIR/cert.pem" \
    -days 365 \
    -nodes \
    -subj "/C=RU/ST=Moscow/L=Moscow/O=BotC/CN=botc-server" \
    -addext "subjectAltName=IP:127.0.0.1" \
    2>/dev/null

chmod 644 "$SSL_DIR/cert.pem"
chmod 600 "$SSL_DIR/key.pem"

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Готово!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo -e "Файлы созданы в:"
echo -e "  ${YELLOW}${SSL_DIR}/cert.pem${NC} — сертификат"
echo -e "  ${YELLOW}${SSL_DIR}/key.pem${NC}  — закрытый ключ"
echo ""
echo -e "${YELLOW}ВНИМАНИЕ: Это самоподписанный сертификат.${NC}"
echo -e "Клиенты должны использовать verify=False или добавить cert.pem в доверенные."
echo ""

# Показать fingerprint для верификации на клиентах
echo -e "SHA256 fingerprint сертификата:"
openssl x509 -in "$SSL_DIR/cert.pem" -fingerprint -noout -sha256 2>/dev/null | cut -d= -f2
echo ""
