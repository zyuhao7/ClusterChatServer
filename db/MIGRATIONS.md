# Database Migrations

Run migrations in order on database `chat`.

## Prerequisites

Initialize base schema first:

```bash
mysql -uroot -p < chat.sql
```

Ubuntu 24.04 may use `auth_socket` for root:

```bash
sudo mysql < chat.sql
```

## Apply Migrations

```bash
mysql -uroot -p chat < db/migrations/001_constraints_and_indexes.sql
mysql -uroot -p chat < db/migrations/002_message_history_and_admin_tables.sql
mysql -uroot -p chat < db/migrations/003_password_hashing_prep.sql
mysql -uroot -p chat < db/migrations/004_user_blacklist.sql
mysql -uroot -p chat < db/migrations/005_group_moderation_features.sql
```

## 001_constraints_and_indexes.sql

Adds missing constraints and indexes for:

- `groupuser`
- `offlinemessage`

## 002_message_history_and_admin_tables.sql

Creates:

- `message_history`
- `admin_audit_log`
- `admin_operation_log`

## 003_password_hashing_prep.sql

Expands `user.password` column size for bcrypt hashes.

After migration `003`, run one-time plaintext password migration:

```bash
python3 scripts/migrate_passwords.py --user root --database chat
```

## 004_user_blacklist.sql

Creates:

- `user_blacklist`

## 005_group_moderation_features.sql

Adds:

- `allgroup.announcement`
- `groupuser.muted_until`

## Rollback Guidance

- Keep a SQL dump before migration:
  `mysqldump -uroot -p chat > backup_chat_before_migration.sql`
- Roll back by restoring dump:
  `mysql -uroot -p chat < backup_chat_before_migration.sql`
