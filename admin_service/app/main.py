from fastapi import FastAPI

from admin_service.app.core.config import settings
from admin_service.app.ai.service import ai_worker
from admin_service.app.routers import (
    admin_ops,
    ai,
    friends,
    groups,
    health,
    history,
    metrics,
    offline_messages,
    users,
)


app = FastAPI(
    title=settings.app_name,
    debug=settings.debug,
    version="0.1.0",
)


@app.on_event("startup")
async def on_startup() -> None:
    await ai_worker.start()


@app.on_event("shutdown")
async def on_shutdown() -> None:
    await ai_worker.stop()


@app.get("/")
def root() -> dict:
    return {
        "service": settings.app_name,
        "version": "0.1.0",
        "status": "ok",
    }


app.include_router(health.router)
app.include_router(metrics.router)
app.include_router(users.router, prefix="/api/v1")
app.include_router(friends.router, prefix="/api/v1")
app.include_router(groups.router, prefix="/api/v1")
app.include_router(offline_messages.router, prefix="/api/v1")
app.include_router(history.router, prefix="/api/v1")
app.include_router(admin_ops.router, prefix="/api/v1")
app.include_router(ai.router, prefix="/api/v1")
