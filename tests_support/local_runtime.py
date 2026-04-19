from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import socket
from typing import Any, Callable, Protocol, Sequence


EPHEMERAL_PORT_MIN = 49152
EPHEMERAL_PORT_MAX = 65535


class LocalTestRuntimeError(RuntimeError):
    """Base error for local test runtime helpers."""


class PreflightDependencyError(LocalTestRuntimeError):
    """Raised when a required local dependency is not ready."""

    def __init__(self, component: str, detail: str):
        super().__init__(f"{component} dependency not ready: {detail}")
        self.component = component


class CursorLike(Protocol):
    def execute(self, sql: str) -> Any:
        ...

    def fetchall(self) -> list[tuple[Any, ...]]:
        ...

    def fetchone(self) -> tuple[Any, ...] | None:
        ...


class ConnectionLike(Protocol):
    def cursor(self) -> Any:
        ...

    def commit(self) -> Any:
        ...

    def close(self) -> Any:
        ...


@dataclass(frozen=True)
class CleanupReport:
    user: int
    friend: int
    groupuser: int
    message_history: int
    offlinemessage: int
    allgroup: int


def allocate_free_localhost_port(
    start: int = EPHEMERAL_PORT_MIN,
    end: int = EPHEMERAL_PORT_MAX,
) -> int:
    """Return the first currently unused localhost TCP port in the ephemeral range."""
    if start < EPHEMERAL_PORT_MIN or end > EPHEMERAL_PORT_MAX or start > end:
        raise ValueError(
            f"port range must stay within {EPHEMERAL_PORT_MIN}-{EPHEMERAL_PORT_MAX}"
        )

    for port in range(start, end + 1):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            try:
                probe.bind(("127.0.0.1", port))
            except OSError:
                continue
            return port

    raise LocalTestRuntimeError(
        f"no free localhost port available in range {start}-{end}"
    )


def generate_runtime_config(
    template_path: Path,
    output_path: Path,
    *,
    server_port: int,
    mysql_host: str,
    mysql_port: int,
    redis_host: str,
    redis_port: int,
    server_host: str = "127.0.0.1",
) -> Path:
    """Render a test-scoped runtime config from a local template."""
    values = {
        "SERVER_HOST": server_host,
        "SERVER_PORT": str(server_port),
        "MYSQL_HOST": mysql_host,
        "MYSQL_PORT": str(mysql_port),
        "REDIS_HOST": redis_host,
        "REDIS_PORT": str(redis_port),
    }

    rendered = template_path.read_text(encoding="utf-8")
    for key, value in values.items():
        rendered = rendered.replace(f"{{{{{key}}}}}", value)

    unresolved = sorted(set(re.findall(r"\{\{[A-Z0-9_]+\}\}", rendered)))
    if unresolved:
        raise LocalTestRuntimeError(
            f"unresolved config placeholders: {', '.join(unresolved)}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered, encoding="utf-8")
    return output_path


class GeneratedRowsCleaner:
    """Remove deterministic test data for repeated local runs."""

    def __init__(self, connect: Callable[[], Any]):
        self._connect = connect

    def cleanup(self, user_ids: Sequence[int]) -> CleanupReport:
        normalized_user_ids = _normalize_ids(user_ids)
        if not normalized_user_ids:
            return CleanupReport(0, 0, 0, 0, 0, 0)

        connection = self._connect()
        try:
            cursor = connection.cursor()
            user_list = _sql_list(normalized_user_ids)
            group_ids = self._select_group_ids(cursor, user_list)
            group_clause = _sql_list(group_ids) if group_ids else None

            self._execute(
                cursor,
                (
                    f"DELETE FROM message_history WHERE sender_id IN ({user_list}) "
                    f"OR receiver_id IN ({user_list})"
                    + (
                        f" OR group_id IN ({group_clause})"
                        if group_clause is not None
                        else ""
                    )
                ),
            )
            self._execute(cursor, f"DELETE FROM offlinemessage WHERE userid IN ({user_list})")
            self._execute(
                cursor,
                f"DELETE FROM friend WHERE userid IN ({user_list}) OR friendid IN ({user_list})",
            )
            self._execute(
                cursor,
                f"DELETE FROM groupuser WHERE userid IN ({user_list})"
                + (
                    f" OR groupid IN ({group_clause})" if group_clause is not None else ""
                ),
            )
            if group_clause is not None:
                self._execute(cursor, f"DELETE FROM allgroup WHERE id IN ({group_clause})")
            self._execute(cursor, f"DELETE FROM user WHERE id IN ({user_list})")
            connection.commit()

            return CleanupReport(
                user=self._count(cursor, f"SELECT COUNT(*) FROM user WHERE id IN ({user_list})"),
                friend=self._count(
                    cursor,
                    (
                        f"SELECT COUNT(*) FROM friend WHERE userid IN ({user_list}) "
                        f"OR friendid IN ({user_list})"
                    ),
                ),
                groupuser=self._count(
                    cursor,
                    f"SELECT COUNT(*) FROM groupuser WHERE userid IN ({user_list})"
                    + (
                        f" OR groupid IN ({group_clause})" if group_clause is not None else ""
                    ),
                ),
                message_history=self._count(
                    cursor,
                    (
                        f"SELECT COUNT(*) FROM message_history WHERE sender_id IN ({user_list}) "
                        f"OR receiver_id IN ({user_list})"
                        + (
                            f" OR group_id IN ({group_clause})"
                            if group_clause is not None
                            else ""
                        )
                    ),
                ),
                offlinemessage=self._count(
                    cursor,
                    f"SELECT COUNT(*) FROM offlinemessage WHERE userid IN ({user_list})",
                ),
                allgroup=self._count(
                    cursor,
                    f"SELECT COUNT(*) FROM allgroup WHERE id IN ({group_clause})"
                    if group_clause is not None
                    else "SELECT COUNT(*) FROM allgroup WHERE 1 = 0",
                ),
            )
        finally:
            connection.close()

    @staticmethod
    def _execute(cursor: CursorLike, sql: str) -> None:
        cursor.execute(sql)

    @staticmethod
    def _count(cursor: CursorLike, sql: str) -> int:
        cursor.execute(sql)
        row = cursor.fetchone()
        return int(row[0] if row is not None else 0)

    @staticmethod
    def _select_group_ids(cursor: CursorLike, user_list: str) -> list[int]:
        cursor.execute(f"SELECT DISTINCT groupid FROM groupuser WHERE userid IN ({user_list})")
        return [int(row[0]) for row in cursor.fetchall()]


def run_preflight_checks(
    *,
    mysql_probe: Callable[[], None],
    redis_probe: Callable[[], None],
    chat_server_probe: Callable[[], None],
) -> None:
    """Fail fast when a required local dependency is unavailable."""
    for component, probe in (
        ("mysql", mysql_probe),
        ("redis", redis_probe),
        ("chat_server", chat_server_probe),
    ):
        try:
            probe()
        except Exception as exc:  # noqa: BLE001
            raise PreflightDependencyError(component, str(exc)) from exc


def _normalize_ids(values: Sequence[int]) -> list[int]:
    normalized: list[int] = []
    for value in values:
        int_value = int(value)
        if int_value <= 0:
            raise ValueError("cleanup ids must be positive integers")
        if int_value not in normalized:
            normalized.append(int_value)
    return normalized


def _sql_list(values: Sequence[int]) -> str:
    return ", ".join(str(value) for value in values)
