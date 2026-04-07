# AGENTS.md - ClusterChatServer

## Build Commands

```bash
# C++ build (requires muduo, MySQL dev, hiredis)
cmake -S . -B build
cmake --build build -j

# Build with custom dependency prefixes
cmake -S . -B build \
  -DMUDUO_ROOT=/path/to/muduo-install \
  -DMYSQL_ROOT=/path/to/mysql-prefix \
  -DHIREDIS_ROOT=/path/to/hiredis-prefix

# Build C++ smoke tests
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j --target cluster_chat_unit
```

Outputs: `bin/chat_server`, `bin/chat_client`

## Config

Server config is file-based now (not hardcoded).

- Default: `config/server.conf`
- Example template: `config/server.conf.example`
- Docker config: `config/server.docker.conf`

Run with explicit config path:

```bash
./bin/chat_server --config config/server.conf
./bin/chat_server --config config/server.conf 127.0.0.1 9120
```

## Database Setup

```bash
# Initialize database
mysql -uroot -p < chat.sql

# Ubuntu quirk: root may use auth_socket, use sudo instead
sudo mysql < chat.sql

# Run schema/data migrations in order
mysql -uroot -p chat < db/migrations/001_constraints_and_indexes.sql
mysql -uroot -p chat < db/migrations/002_message_history_and_admin_tables.sql
mysql -uroot -p chat < db/migrations/003_password_hashing_prep.sql

# One-time plaintext->bcrypt migration (after 003)
python3 scripts/migrate_passwords.py --user root --database chat
```

## Running

```bash
# Server
./bin/chat_server --config config/server.conf 127.0.0.1 9120
./bin/chat_server --config config/server.conf 127.0.0.1 9121  # second instance

# Client (direct or via nginx)
./bin/chat_client 127.0.0.1 9120
./bin/chat_client 127.0.0.1 8000  # via nginx load balancer
```

## Dependencies

- muduo (C++ network library)
- MySQL / mariadb client
- hiredis (Redis client)
- nginx (TCP load balancing)
- Redis server

## Python Admin Service

```bash
# from repo root
python3 -m venv admin_service/.venv
source admin_service/.venv/bin/activate
pip install -r admin_service/requirements.txt
cp admin_service/.env.example admin_service/.env
uvicorn admin_service.app.main:app --reload --host 127.0.0.1 --port 8010
```

Config: `admin_service/.env` (copy from `.env.example`)

Admin endpoints under `/api/v1/admin/*` require:

```text
X-Admin-Token: <ADMIN_TOKEN>
```

## Tests and Health Checks

```bash
# C++ smoke test binary
./build/cluster_chat_unit

# FastAPI integration tests
source admin_service/.venv/bin/activate
pytest tests/integration/test_admin_service.py -q

# Server roundtrip check (register/login/logout)
python3 scripts/check_server_roundtrip.py

# Admin service health/metrics quick checks
bash scripts/check_admin_service.sh
bash scripts/check_admin_metrics.sh
```

## Docker Stack

```bash
docker compose up --build -d
# or
bash scripts/run_stack.sh
```

## Example Code

```bash
cd example
make  # uses separate makefile, not cmake
# or with custom muduo:
make MUDUO_PREFIX=/path/to/muduo
```

## Important Notes

- `install.sh` is the **opencode installer**, not related to this project
- MySQL root on Ubuntu 24.04 uses `auth_socket` by default - use `sudo mysql` or reconfigure
- nginx TCP load balancing requires `libnginx-mod-stream` and stream config in separate file under `/etc/nginx/stream-conf.d/`
- prefer editing `config/server.conf` for DB/Redis/server params instead of code constants
