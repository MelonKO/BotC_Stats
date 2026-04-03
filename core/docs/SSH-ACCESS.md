# SSH-доступ к базе данных BotC

> Безопасное подключение к PostgreSQL через SSH-туннель на удалённом сервере.

---

## 📋 Обзор архитектуры

```
┌─────────────────────────────────────────────────────────────┐
│                    Удалённый сервер (IP)                     │
│                                                              │
│  ┌──────────────────┐         ┌─────────────────────┐       │
│  │   SSH-сервер     │────────▶│  Docker-контейнер   │       │
│  │   порт 22        │ туннель │  PostgreSQL :5432   │       │
│  │   botc-ssh       │         │  (только localhost) │       │
│  └──────────────────┘         └─────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
         ▲
         │ ssh -L 5432:localhost:5432
         │
┌─────────────────┐
│  Клиентская     │
│  машина         │
│  psql / DBeaver │
└─────────────────┘
```

**Принцип работы:**
- PostgreSQL слушает **только 127.0.0.1** — порт не доступен извне
- Подключение возможно только через SSH-туннель
- SSH-пользователь `botc-ssh` ограничен только туннелированием (нет shell, нет TTY)

---

## 🚀 Быстрая настройка

### Шаг 1: Создание SSH-пользователя на сервере

На удалённом сервере выполните:

```bash
# Вариант A: С генерацией ключа на сервере
sudo bash scripts/setup-ssh-user.sh botc-ssh

# Вариант B: С готовым публичным ключом клиента
sudo bash scripts/setup-ssh-user.sh botc-ssh ~/.ssh/my-botc-key.pub
```

Скрипт автоматически:
1. Создаёт пользователя `botc-ssh` с ограниченным доступом
2. Настраивает `authorized_keys` для подключения по ключу
3. Ограничивает SSH только туннелированием к `localhost:5432`
4. Запрещает TTY и интерактивный shell

### Шаг 2: Генерация SSH-ключа на клиенте (если ещё нет)

```bash
# На клиентской машине
ssh-keygen -t ed25519 -C "botc-tunnel" -f ~/.ssh/botc-tunnel -N ""
```

Публичный ключ (`~/.ssh/botc-tunnel.pub`) нужно передать на сервер и добавить в `authorized_keys`:

```bash
# На сервере
echo "<содержимое botc-tunnel.pub>" >> /home/botc-ssh/.ssh/authorized_keys
```

### Шаг 3: Запуск Docker-контейнера

```bash
# На сервере
cd /path/to/docker-botc
cp .env.example .env
# Измените пароли в .env
docker-compose up -d
```

### Шаг 4: Подключение через SSH-туннель

```bash
# На клиентской машине — создать туннель
ssh -i ~/.ssh/botc-tunnel -L 5432:localhost:5432 botc-ssh@<SERVER_IP>

# В ДРУГОМ ТЕРМИНАЛЕ — подключиться к БД
psql -h localhost -p 5432 -U botc_user -d botc_stats
```

---

## 🔧 Подробная инструкция

### Создание SSH-пользователя вручную

Если вы предпочитаете настроить пользователя вручную:

```bash
# 1. Создать пользователя без shell
sudo useradd --system --no-create-home --shell /bin/false botc-ssh

# 2. Создать .ssh директорию
sudo mkdir -p /home/botc-ssh/.ssh
sudo chmod 700 /home/botc-ssh/.ssh

# 3. Добавить публичный ключ клиента
sudo echo "ssh-ed25519 AAAA... client@machine" > /home/botc-ssh/.ssh/authorized_keys
sudo chmod 600 /home/botc-ssh/.ssh/authorized_keys
sudo chown -R botc-ssh:botc-ssh /home/botc-ssh/.ssh

# 4. Добавить ограничение в sshd_config
sudo tee /etc/ssh/sshd_config.d/botc-ssh.conf << 'EOF'
Match User botc-ssh
    ForceCommand /bin/false
    PermitOpen localhost:5432
    AllowTcpForwarding yes
    PermitTTY no
    X11Forwarding no
EOF

# 5. Перезапустить SSH
sudo systemctl restart sshd
```

### Настройка SSH-клиента

Добавьте в `~/.ssh/config` на клиентской машине:

```ssh-config
Host botc-db
    HostName <SERVER_IP>
    User botc-ssh
    IdentityFile ~/.ssh/botc-tunnel
    LocalForward 5432 localhost:5432
    ExitOnForwardFailure yes
    ServerAliveInterval 60
    ServerAliveCountMax 3
```

Теперь подключение одной командой:

```bash
ssh botc-db
```

---

## 🔄 Постоянный туннель (autossh)

Для автоматического поддержания соединения:

### Установка autossh

```bash
# Ubuntu/Debian
sudo apt install autossh

# CentOS/RHEL
sudo yum install autossh

# macOS
brew install autossh
```

### Запуск туннеля

```bash
autossh -M 0 \
    -i ~/.ssh/botc-tunnel \
    -o "ExitOnForwardFailure=yes" \
    -o "ServerAliveInterval=30" \
    -o "ServerAliveCountMax=3" \
    -L 5432:localhost:5432 \
    botc-ssh@<SERVER_IP> \
    -N -f
```

### systemd-сервис (Linux)

Создайте `/etc/systemd/system/botc-ssh-tunnel.service`:

```ini
[Unit]
Description=BotC PostgreSQL SSH Tunnel
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=<local_user>
ExecStart=/usr/bin/autossh -M 0 \
    -i /home/<user>/.ssh/botc-tunnel \
    -o ExitOnForwardFailure=yes \
    -o ServerAliveInterval=30 \
    -o ServerAliveCountMax=3 \
    -L 5432:localhost:5432 \
    botc-ssh@<SERVER_IP> -N
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now botc-ssh-tunnel
sudo systemctl status botc-ssh-tunnel
```

---

## 💻 Подключение инструментов

### psql (командная строка)

```bash
# После установления туннеля
psql -h localhost -p 5432 -U botc_user -d botc_stats
```

### DBeaver

1. **Новое подключение → PostgreSQL**
2. **Host:** `localhost`
3. **Port:** `5432`
4. **Database:** `botc_stats`
5. **Username:** `botc_user`
6. **Password:** (из `.env`)
7. Предварительно установите SSH-туннель

### pgAdmin

1. **Add New Server**
2. **Connection → Host:** `localhost`
3. **Connection → Port:** `5432`
4. **Connection → Username:** `botc_user`
5. Предварительно установите SSH-туннель

### Python (psycopg2)

```python
import psycopg2

conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="botc_stats",
    user="botc_user",
    password="your_password"
)
```

---

## 🔐 Безопасность

### Что защищено

| Уровень | Мера |
|---------|------|
| Сеть | Порт 5432 не проброшен наружу |
| SSH | Аутентификация только по ключу |
| SSH | Запрещён TTY и интерактивный shell |
| SSH | Разрешён туннель только к `localhost:5432` |
| БД | Отдельный пользователь `botc_user` с ограниченными правами |
| БД | `postgres` только для администрирования |

### Рекомендации

- ✅ Используйте **ed25519** ключи (не RSA < 2048)
- ✅ Храните закрытый ключ в безопасном месте
- ✅ Не передавайте ключи по незащищённым каналам
- ✅ Регулярно обновляйте образ PostgreSQL (`docker-compose pull`)
- ✅ Настройте `fail2ban` на сервере для защиты SSH
- ✅ Используйте firewall (`ufw` / `iptables`) для ограничения доступа к порту 22

### Проверка firewall

```bash
# Проверить открытые порты
sudo ss -tlnp | grep -E '22|5432'

# Если 5432 виден на 0.0.0.0 — это проблема!
# Должно быть только 127.0.0.1:5432
```

---

## 🛠️ Устранение неполадок

### Туннель не устанавливается

```bash
# Проверить SSH-доступ
ssh -v -i ~/.ssh/botc-tunnel botc-ssh@<SERVER_IP>

# Проверить, что PostgreSQL запущен
ssh botc-ssh@<SERVER_IP> "docker ps | grep botc"

# Проверить, что PostgreSQL слушает localhost
ssh botc-ssh@<SERVER_IP> "docker exec botc-postgres netstat -tlnp | grep 5432"
```

### Ошибка «Connection refused»

- Убедитесь, что Docker-контейнер запущен: `docker-compose ps`
- Проверьте, что PostgreSQL слушает: `docker-compose exec db netstat -tlnp | grep 5432`
- Проверьте `listen_addresses` в конфиге контейнера

### Ошибка «Permission denied (publickey)»

- Проверьте права на ключ: `chmod 600 ~/.ssh/botc-tunnel`
- Убедитесь, что публичный ключ добавлен в `/home/botc-ssh/.ssh/authorized_keys`
- Проверьте логи SSH: `sudo journalctl -u sshd -f`

### Туннель обрывается

- Используйте `autossh` для автоматического переподключения
- Настройте `ServerAliveInterval` в SSH-конфиге
- Проверьте настройки таймаута на сервере

---

## 📄 Лицензия

MIT — используйте, модифицируйте, делитесь.
