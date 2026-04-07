#!/usr/bin/env python3
"""
One-time password migration script.

Migrates legacy plaintext passwords in chat.user to bcrypt hashes.
"""

from __future__ import annotations

import argparse
import getpass
import sys

import bcrypt
import pymysql


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Migrate plaintext passwords to bcrypt")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3306)
    parser.add_argument("--user", default="root")
    parser.add_argument("--password", default=None, help="MySQL password; prompt if omitted")
    parser.add_argument("--database", default="chat")
    parser.add_argument("--cost", type=int, default=10)
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def is_bcrypt_hash(value: str) -> bool:
    return value.startswith("$2a$") or value.startswith("$2b$") or value.startswith("$2y$")


def main() -> int:
    args = parse_args()
    password = args.password
    if password is None:
        password = getpass.getpass("MySQL password: ")

    conn = pymysql.connect(
        host=args.host,
        port=args.port,
        user=args.user,
        password=password,
        database=args.database,
        charset="utf8mb4",
        autocommit=False,
    )

    migrated = 0
    skipped = 0
    try:
        with conn.cursor() as cur:
            cur.execute("SELECT id, password FROM user")
            rows = cur.fetchall()
            for user_id, raw_password in rows:
                if not raw_password or is_bcrypt_hash(raw_password):
                    skipped += 1
                    continue

                hashed = bcrypt.hashpw(
                    raw_password.encode("utf-8"),
                    bcrypt.gensalt(rounds=args.cost),
                ).decode("utf-8")

                if not args.dry_run:
                    cur.execute("UPDATE user SET password=%s WHERE id=%s", (hashed, user_id))
                migrated += 1

        if args.dry_run:
            conn.rollback()
        else:
            conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

    mode = "DRY-RUN" if args.dry_run else "COMMITTED"
    print(f"[{mode}] migrated={migrated}, skipped={skipped}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
