from pathlib import Path
import sys

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from admin_service.app.db.database import get_db
from admin_service.app.main import app
from admin_service.app.models.models import Base, User


@pytest.fixture
def client(tmp_path: Path):
    database_path = tmp_path / "admin-service.sqlite3"
    engine = create_engine(
        f"sqlite:///{database_path}",
        connect_args={"check_same_thread": False},
    )
    testing_session_local = sessionmaker(autocommit=False, autoflush=False, bind=engine)
    Base.metadata.create_all(bind=engine)

    session = testing_session_local()
    try:
        session.add_all(
            [
                User(name="alice", password="hash-a", state="online"),
                User(name="bob", password="hash-b", state="offline"),
                User(name="carol", password="hash-c", state="banned"),
            ]
        )
        session.commit()
    finally:
        session.close()

    def override_get_db():
        db = testing_session_local()
        try:
            yield db
        finally:
            db.close()

    app.dependency_overrides[get_db] = override_get_db
    try:
        with TestClient(app) as test_client:
            yield test_client
    finally:
        app.dependency_overrides.clear()
        Base.metadata.drop_all(bind=engine)
        engine.dispose()


def test_root(client: TestClient) -> None:
    resp = client.get("/")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_health(client: TestClient) -> None:
    resp = client.get("/health")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_ai_moderation_blocks_sensitive_word(client: TestClient) -> None:
    resp = client.post("/api/v1/ai/moderate", json={"text": "this is spam"})
    assert resp.status_code == 200
    payload = resp.json()
    assert payload["ok"] is True
    assert payload["action"] == "block"


def test_ai_summary(client: TestClient) -> None:
    resp = client.post("/api/v1/ai/summary", json={"messages": ["hello", "world"]})
    assert resp.status_code == 200
    payload = resp.json()
    assert payload["ok"] is True
    assert "messages=2" in payload["summary"]


def test_list_users_filters_by_state(client: TestClient) -> None:
    resp = client.get("/api/v1/users", params={"state": "online"})
    assert resp.status_code == 200
    payload = resp.json()
    assert len(payload) == 1
    assert payload[0]["name"] == "alice"
    assert payload[0]["state"] == "online"


def test_ban_user_requires_admin_token(client: TestClient) -> None:
    resp = client.post("/api/v1/admin/users/1/ban")
    assert resp.status_code == 401
    assert resp.json()["detail"] == "invalid admin token"


def test_ban_and_unban_user_updates_state(client: TestClient) -> None:
    headers = {"X-Admin-Token": "change-me"}

    ban_resp = client.post(
        "/api/v1/admin/users/2/ban",
        params={"reason": "manual-check"},
        headers=headers,
    )
    assert ban_resp.status_code == 200
    assert ban_resp.json()["state"] == "banned"

    banned_users = client.get("/api/v1/users", params={"state": "banned"})
    banned_payload = banned_users.json()
    assert any(user["id"] == 2 and user["state"] == "banned" for user in banned_payload)

    unban_resp = client.post("/api/v1/admin/users/2/unban", headers=headers)
    assert unban_resp.status_code == 200
    assert unban_resp.json()["state"] == "offline"

    offline_users = client.get("/api/v1/users", params={"state": "offline"})
    offline_payload = offline_users.json()
    assert any(user["id"] == 2 and user["state"] == "offline" for user in offline_payload)
