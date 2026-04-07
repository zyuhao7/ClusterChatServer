from fastapi import Header, HTTPException, status

from admin_service.app.core.config import settings


def require_admin_token(x_admin_token: str | None = Header(default=None)) -> str:
    if not x_admin_token or x_admin_token != settings.admin_token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="invalid admin token",
        )
    return "admin"
