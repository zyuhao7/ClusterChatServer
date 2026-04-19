# AGENTS.md - ClusterChatServer

Compact, high-signal repo guidance. Keep commands aligned with repository files.

## Build

```bash
cmake -S . -B build
cmake --build build -j

cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j --target cluster_chat_unit
```

Outputs: `bin/chat_server`, `bin/chat_client`, `bin/cluster_chat_unit`.

## Run

```bash
./bin/chat_server --config config/server.conf 127.0.0.1 9120
./bin/chat_server --config config/server.conf 127.0.0.1 9121
./bin/chat_client 127.0.0.1 9120
./bin/chat_client 127.0.0.1 8000
```

Use config files, not hardcoded constants: `config/server.conf`, `config/server.docker.conf`.
Client port `9120` is direct server access; `8000` is nginx stream access.

## Test

```bash
./bin/cluster_chat_unit
source admin_service/.venv/bin/activate
pytest tests/integration/test_admin_service.py -q
bash scripts/check_admin_service.sh
bash scripts/check_admin_metrics.sh
python3 scripts/check_server_roundtrip.py
```

## Database (order-sensitive)

```bash
mysql -uroot -p < chat.sql
mysql -uroot -p chat < db/migrations/001_constraints_and_indexes.sql
mysql -uroot -p chat < db/migrations/002_message_history_and_admin_tables.sql
mysql -uroot -p chat < db/migrations/003_password_hashing_prep.sql
mysql -uroot -p chat < db/migrations/004_user_blacklist.sql
python3 scripts/migrate_passwords.py --user root --database chat
```

Ubuntu root auth_socket fallback:

```bash
sudo mysql < chat.sql
```

## Docker

```bash
bash scripts/run_stack.sh
```

## Boundaries

- `install.sh` is an OpenCode installer script, not this project's setup entrypoint.
- `example/` uses its own `makefile` workflow and is separate from top-level CMake targets.

## Gotchas

- Common path mistake: use `tests/integration/test_admin_service.py` (not `test/...`).
- Migration order is strict: `001 -> 002 -> 003`, then run `scripts/migrate_passwords.py` once.
- `./bin/cluster_chat_unit` exists only after building with `-DBUILD_TESTS=ON`.
