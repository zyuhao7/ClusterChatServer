from fastapi import FastAPI

from admin_service.app.core.config import settings
from admin_service.app.routers import friends, groups, health, offline_messages, users


app = FastAPI(
    title=settings.app_name,
    debug=settings.debug,
    version="0.1.0",
)


@app.get("/")
def root() -> dict:
    return {
        "service": settings.app_name,
        "version": "0.1.0",
        "status": "ok",
    }


app.include_router(health.router)
app.include_router(users.router, prefix="/api/v1")
app.include_router(friends.router, prefix="/api/v1")
app.include_router(groups.router, prefix="/api/v1")
app.include_router(offline_messages.router, prefix="/api/v1")
