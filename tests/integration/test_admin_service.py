from pathlib import Path
import sys

from fastapi.testclient import TestClient


REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from admin_service.app.main import app


client = TestClient(app)


def test_root() -> None:
    resp = client.get("/")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_health() -> None:
    resp = client.get("/health")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_ai_moderation_blocks_sensitive_word() -> None:
    resp = client.post("/api/v1/ai/moderate", json={"text": "this is spam"})
    assert resp.status_code == 200
    payload = resp.json()
    assert payload["ok"] is True
    assert payload["action"] == "block"


def test_ai_summary() -> None:
    resp = client.post("/api/v1/ai/summary", json={"messages": ["hello", "world"]})
    assert resp.status_code == 200
    payload = resp.json()
    assert payload["ok"] is True
    assert "messages=2" in payload["summary"]
