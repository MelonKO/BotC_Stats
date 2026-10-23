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
│  │   botc-ssh       │         │  (127.0.0.1 on host) │       │
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
- Docker публикует порт PostgreSQL только на `127.0.0.1:5432` хоста. Внутри контейнера PostgreSQL слушает Docker-интерфейсы (`listen_addresses=*`), но хостовый порт недоступен напрямую из внешней сети.
- Удалённое администрирование PostgreSQL выполняется через SSH-туннель.
- SSH-пользователь `botc-ssh` ограничен туннелированием (нет shell, нет TTY); запускайте SSH с `-N`, чтобы не запрашивать shell-сессию.

---

## 🚀 Быстрая настройка

### Шаг 1: Генерация SSH-ключа на клиенте

Рекомендуется создать ключ на клиентской машине, чтобы закрытый ключ никогда не попадал на сервер:

```bash
ssh-keygen -t ed25519 -C "botc-tunnel" -f ~/.ssh/botc-tunnel
```

Передайте на сервер только публичный ключ `~/.ssh/botc-tunnel.pub` (например, через защищённый административный SSH-сеанс).

### Шаг 2: Создание SSH-пользователя на сервере

На удалённом сервере, из каталога `core/`, запустите setup-скрипт с путём к публичному ключу:

```bash
sudo bash scripts/setup-ssh-user.sh botc-ssh /path/to/botc-tunnel.pub
```

Скрипт автоматически:
1. Создаёт пользователя `botc-ssh` с ограниченным доступом
2. Настраивает `authorized_keys` для подключения по ключу
3. Ограничивает SSH туннелированием к `localhost:5432`
4. Запрещает TTY и интерактивный shell

Если запустить скрипт без публичного ключа, он создаст пару ключей на сервере и сохранит закрытый ключ в `/tmp/botc-ssh-key-botc-ssh`. В этом случае безопасно перенесите закрытый ключ на клиентскую машину и сразу удалите его копию с сервера.

### Шаг 3: Запуск Docker-контейнера

```bash
# On the server, from the core/ directory
cd /path/to/BotC/core
cp .env.example .env
# Set unique passwords/secrets and configure SERVER_IP in .env.
docker-compose up -d
```

### Шаг 4: Подключение через SSH-туннель

```bash
# На клиентской машине — создать туннель
ssh -i ~/.ssh/botc-tunnel -N -o ExitOnForwardFailure=yes -L 5432:localhost:5432 botc-ssh@<SERVER_IP>

# В ДРУГОМ ТЕРМИНАЛЕ — подключиться к БД
psql -h localhost -p 5432 -U botc_user -d botc_stats
```

---

## 🔧 Подробная инструкция

### Создание SSH-пользователя вручную

Если вы предпочитаете настроить пользователя вручную:

```bash
# 1. Create a restricted user and its SSH directory
sudo useradd --system --no-create-home --home-dir /home/botc-ssh --shell /bin/false botc-ssh
sudo install -d -m 700 -o botc-ssh /home/botc-ssh
sudo install -d -m 700 -o botc-ssh /home/botc-ssh/.ssh

# 2. Add the client's public key (replace the example key)
printf '%s\n' 'ssh-ed25519 AAAA... client@machine' | sudo tee -a /home/botc-ssh/.ssh/authorized_keys >/dev/null
sudo chown botc-ssh /home/botc-ssh/.ssh/authorized_keys
sudo chmod 600 /home/botc-ssh/.ssh/authorized_keys

# 3. Add tunnel-only restrictions in sshd_config
sudo tee /etc/ssh/sshd_config.d/botc-ssh.conf << 'EOF'
Match User botc-ssh
    ForceCommand /bin/false
    PermitOpen localhost:5432
    AllowTcpForwarding yes
    PermitTTY no
    X11Forwarding no
EOF

# 4. Restart the SSH service
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
ssh -N botc-db
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
| Сеть | Порт 5432 опубликован только на loopback хоста (`127.0.0.1`) |
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

# On the Docker host, the published port should be 127.0.0.1:5432,
# not 0.0.0.0:5432. Inside the PostgreSQL container, listen_addresses=*
# is expected; external access is restricted by the host port binding.
```

---

## 🛠️ Устранение неполадок

### Туннель не устанавливается

Пользователь `botc-ssh` намеренно не имеет shell-доступа, поэтому команды `docker` нельзя выполнять через него. Проверяйте туннель с клиентской машины:

```bash
# Запустить туннель в отдельном терминале
ssh -vv -i ~/.ssh/botc-tunnel -N -o ExitOnForwardFailure=yes \
  -L 15432:localhost:5432 botc-ssh@<SERVER_IP>

# В другом терминале проверить доступность PostgreSQL через туннель
psql -h 127.0.0.1 -p 15432 -U botc_user -d botc_stats
```

Состояние Docker и порт хоста проверяйте на сервере через административную учётную запись, а не через `botc-ssh`.

### Ошибка «Connection refused»

- На сервере (с административной учётной записью) проверьте `docker-compose ps` и `docker-compose exec db pg_isready -U postgres -d botc_stats`.
- Убедитесь, что порт опубликован только на loopback хоста: `sudo ss -ltnp | grep ':5432'` должно показывать `127.0.0.1:5432`.
- Внутри контейнера `listen_addresses=*` ожидаемо; ограничение внешнего доступа обеспечивается привязкой порта хоста к `127.0.0.1`.

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

GNU GPLv3
