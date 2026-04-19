from __future__ import annotations

from pathlib import Path
import socket
import sqlite3
import sys

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tests_support.local_runtime import (  # noqa: E402
    CleanupReport,
    EPHEMERAL_PORT_MAX,
    EPHEMERAL_PORT_MIN,
    GeneratedRowsCleaner,
    PreflightDependencyError,
    allocate_free_localhost_port,
    generate_runtime_config,
    run_preflight_checks,
)


def test_allocate_free_localhost_port_skips_occupied_port() -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as occupied:
        occupied.bind(("127.0.0.1", 52000))
        occupied.listen(1)

        port = allocate_free_localhost_port(52000, 52002)

    assert port == 52001
    assert EPHEMERAL_PORT_MIN <= port <= EPHEMERAL_PORT_MAX
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", port))


def test_generate_runtime_config_writes_test_scoped_values(tmp_path: Path) -> None:
    template_path = REPO_ROOT / "tests" / "fixtures" / "local_runtime_server.conf.in"
    output_path = tmp_path / "runtime.conf"

    generate_runtime_config(
        template_path,
        output_path,
        server_port=53111,
        mysql_host="127.0.0.1",
        mysql_port=3307,
        redis_host="127.0.0.1",
        redis_port=6381,
    )

    rendered = output_path.read_text(encoding="utf-8")
    assert "server.host = 127.0.0.1" in rendered
    assert "server.port = 53111" in rendered
    assert "db.host = 127.0.0.1" in rendered
    assert "db.port = 3307" in rendered
    assert "redis.host = 127.0.0.1" in rendered
    assert "redis.port = 6381" in rendered


def test_cleanup_hook_leaves_zero_residual_rows_after_two_runs(tmp_path: Path) -> None:
    database_path = tmp_path / "cleanup.sqlite3"
    _seed_cleanup_database(database_path)
    cleaner = GeneratedRowsCleaner(lambda: sqlite3.connect(database_path))

    first_report = cleaner.cleanup([101, 102])
    second_report = cleaner.cleanup([101, 102])

    assert first_report == CleanupReport(0, 0, 0, 0, 0, 0)
    assert second_report == CleanupReport(0, 0, 0, 0, 0, 0)


@pytest.mark.parametrize("component_name", ["mysql", "redis", "chat_server"])
def test_preflight_check_fails_fast_with_explicit_component_name(
    component_name: str,
) -> None:
    calls: list[str] = []

    def mysql_probe() -> None:
        calls.append("mysql")
        if component_name == "mysql":
            raise ConnectionError("not reachable")

    def redis_probe() -> None:
        calls.append("redis")
        if component_name == "redis":
            raise ConnectionError("not reachable")

    def chat_server_probe() -> None:
        calls.append("chat_server")
        if component_name == "chat_server":
            raise ConnectionError("not reachable")

    with pytest.raises(PreflightDependencyError, match=component_name):
        run_preflight_checks(
            mysql_probe=mysql_probe,
            redis_probe=redis_probe,
            chat_server_probe=chat_server_probe,
        )

    assert calls == {
        "mysql": ["mysql"],
        "redis": ["mysql", "redis"],
        "chat_server": ["mysql", "redis", "chat_server"],
    }[component_name]


def _seed_cleanup_database(database_path: Path) -> None:
    connection = sqlite3.connect(database_path)
    try:
        cursor = connection.cursor()
        cursor.executescript(
            """
            CREATE TABLE user (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL
            );
            CREATE TABLE friend (
                userid INTEGER NOT NULL,
                friendid INTEGER NOT NULL
            );
            CREATE TABLE allgroup (
                id INTEGER PRIMARY KEY,
                groupname TEXT NOT NULL
            );
            CREATE TABLE groupuser (
                groupid INTEGER NOT NULL,
                userid INTEGER NOT NULL
            );
            CREATE TABLE offlinemessage (
                id INTEGER PRIMARY KEY,
                userid INTEGER NOT NULL,
                message TEXT NOT NULL
            );
            CREATE TABLE message_history (
                id INTEGER PRIMARY KEY,
                sender_id INTEGER,
                receiver_id INTEGER,
                group_id INTEGER,
                message TEXT NOT NULL
            );

            INSERT INTO user(id, name) VALUES (101, 'alice');
            INSERT INTO user(id, name) VALUES (102, 'bob');
            INSERT INTO friend(userid, friendid) VALUES (101, 102);
            INSERT INTO allgroup(id, groupname) VALUES (201, 'fixture-group');
            INSERT INTO groupuser(groupid, userid) VALUES (201, 101);
            INSERT INTO groupuser(groupid, userid) VALUES (201, 102);
            INSERT INTO offlinemessage(id, userid, message) VALUES (1, 101, 'pending');
            INSERT INTO message_history(id, sender_id, receiver_id, group_id, message)
            VALUES (1, 101, 102, NULL, 'hello');
            INSERT INTO message_history(id, sender_id, receiver_id, group_id, message)
            VALUES (2, 101, NULL, 201, 'group hello');
            """
        )
        connection.commit()
    finally:
        connection.close()
