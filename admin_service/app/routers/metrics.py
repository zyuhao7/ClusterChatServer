import time

import redis
from fastapi import APIRouter, Depends, Response
from prometheus_client import CONTENT_TYPE_LATEST, Gauge, Histogram, generate_latest
from sqlalchemy import text
from sqlalchemy.orm import Session

from admin_service.app.core.config import settings
from admin_service.app.db.database import get_db

router = APIRouter(tags=["metrics"])

g_online_users = Gauge("chat_online_users", "Number of users in online state")
g_total_messages = Gauge("chat_total_messages", "Total persisted messages")
h_mysql_latency = Histogram("chat_mysql_latency_seconds", "MySQL query latency in seconds")
h_redis_latency = Histogram("chat_redis_latency_seconds", "Redis ping latency in seconds")


@router.get("/metrics")
def metrics(db: Session = Depends(get_db)) -> Response:
    start = time.perf_counter()
    online_users = db.execute(text("SELECT COUNT(*) FROM user WHERE state='online'")).scalar_one()
    total_messages = db.execute(text("SELECT COUNT(*) FROM message_history")).scalar_one()
    mysql_elapsed = time.perf_counter() - start
    h_mysql_latency.observe(mysql_elapsed)

    g_online_users.set(float(online_users))
    g_total_messages.set(float(total_messages))

    redis_elapsed = 0.0
    try:
        r = redis.Redis(host=settings.redis_host, port=settings.redis_port, socket_timeout=1.0)
        start = time.perf_counter()
        r.ping()
        redis_elapsed = time.perf_counter() - start
        h_redis_latency.observe(redis_elapsed)
    except Exception:
        # Keep exporting metrics even when redis is unreachable.
        pass

    payload = generate_latest()
    return Response(content=payload, media_type=CONTENT_TYPE_LATEST)
