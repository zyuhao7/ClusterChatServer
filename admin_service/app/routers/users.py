from pathlib import Path
import shutil

from fastapi import APIRouter, Depends, File, HTTPException, Query, UploadFile
from sqlalchemy.orm import Session

from admin_service.app.core.config import settings
from admin_service.app.db.database import get_db
from admin_service.app.models import User

router = APIRouter(tags=["users"])
AVATAR_ROOT = settings.media_root / "avatars"


def avatar_url_for(row: User) -> str:
    if not row.avatar_path:
        return ""
    return f"/media/{row.avatar_path}"


@router.get("/users")
def list_users(
    state: str | None = Query(default=None),
    limit: int = Query(default=20, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[dict]:
    query = db.query(User)
    if state is not None:
        query = query.filter(User.state == state)

    rows = query.offset(offset).limit(limit).all()
    return [
        {
            "id": row.id,
            "name": row.name,
            "state": row.state,
            "avatar_url": avatar_url_for(row),
        }
        for row in rows
    ]


@router.get("/users/{user_id}")
def get_user(user_id: int, db: Session = Depends(get_db)) -> dict:
    row = db.query(User).filter(User.id == user_id).first()
    if row is None:
        raise HTTPException(status_code=404, detail="user not found")

    return {
        "id": row.id,
        "name": row.name,
        "state": row.state,
        "avatar_url": avatar_url_for(row),
    }


@router.post("/users/{user_id}/avatar")
def upload_avatar(
    user_id: int,
    avatar: UploadFile = File(...),
    db: Session = Depends(get_db),
) -> dict:
    row = db.query(User).filter(User.id == user_id).first()
    if row is None:
        raise HTTPException(status_code=404, detail="user not found")
    if not avatar.content_type or not avatar.content_type.startswith("image/"):
        raise HTTPException(status_code=400, detail="avatar must be an image")

    suffix = Path(avatar.filename or "avatar.png").suffix or ".png"
    AVATAR_ROOT.mkdir(parents=True, exist_ok=True)

    if row.avatar_path:
        old_path = settings.media_root / row.avatar_path
        if old_path.exists():
            old_path.unlink()

    relative_path = Path("avatars") / f"user-{user_id}{suffix.lower()}"
    target_path = settings.media_root / relative_path
    with target_path.open("wb") as file_obj:
        shutil.copyfileobj(avatar.file, file_obj)

    row.avatar_path = relative_path.as_posix()
    db.commit()
    db.refresh(row)
    return {
        "id": row.id,
        "name": row.name,
        "state": row.state,
        "avatar_url": avatar_url_for(row),
    }


@router.post("/uploads/attachments")
def upload_attachment(attachment: UploadFile = File(...)) -> dict:
    if not attachment.filename:
        raise HTTPException(status_code=400, detail="attachment filename is required")

    attachments_root = settings.media_root / "attachments"
    attachments_root.mkdir(parents=True, exist_ok=True)
    suffix = Path(attachment.filename).suffix or ""
    safe_name = Path(attachment.filename).stem.replace(" ", "_")
    relative_path = Path("attachments") / f"{safe_name}-{attachment.size or 0}{suffix.lower()}"
    target_path = settings.media_root / relative_path
    with target_path.open("wb") as file_obj:
        shutil.copyfileobj(attachment.file, file_obj)

    kind = "image" if attachment.content_type and attachment.content_type.startswith("image/") else "file"
    return {
        "name": attachment.filename,
        "kind": kind,
        "mime": attachment.content_type or "application/octet-stream",
        "size": attachment.size or 0,
        "url": f"/media/{relative_path.as_posix()}",
    }
