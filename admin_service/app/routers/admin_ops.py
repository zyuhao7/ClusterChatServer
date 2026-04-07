from fastapi import APIRouter, Depends, Query
from sqlalchemy import text
from sqlalchemy.orm import Session

from admin_service.app.core.security import require_admin_token
from admin_service.app.db.database import get_db

router = APIRouter(prefix="/admin", tags=["admin"])


def write_logs(
    db: Session,
    actor: str,
    action: str,
    target_type: str,
    target_id: str,
    detail: str,
    status: str,
) -> None:
    db.execute(
        text(
            """
            INSERT INTO admin_audit_log(actor, action, target_type, target_id, detail)
            VALUES (:actor, :action, :target_type, :target_id, :detail)
            """
        ),
        {
            "actor": actor,
            "action": action,
            "target_type": target_type,
            "target_id": target_id,
            "detail": detail,
        },
    )
    db.execute(
        text(
            """
            INSERT INTO admin_operation_log(operator_name, operation, status, detail)
            VALUES (:operator_name, :operation, :status, :detail)
            """
        ),
        {
            "operator_name": actor,
            "operation": action,
            "status": status,
            "detail": detail,
        },
    )


@router.post("/users/{user_id}/ban")
def ban_user(
    user_id: int,
    reason: str = Query(default="manual moderation"),
    actor: str = Depends(require_admin_token),
    db: Session = Depends(get_db),
) -> dict:
    row = db.execute(text("SELECT id FROM user WHERE id=:id"), {"id": user_id}).first()
    if row is None:
        write_logs(db, actor, "ban_user", "user", str(user_id), "user not found", "failed")
        db.commit()
        return {"ok": False, "errmsg": "user not found"}

    db.execute(text("UPDATE user SET state='banned' WHERE id=:id"), {"id": user_id})
    write_logs(db, actor, "ban_user", "user", str(user_id), reason, "success")
    db.commit()
    return {"ok": True, "user_id": user_id, "state": "banned"}


@router.post("/users/{user_id}/unban")
def unban_user(
    user_id: int,
    actor: str = Depends(require_admin_token),
    db: Session = Depends(get_db),
) -> dict:
    row = db.execute(text("SELECT id FROM user WHERE id=:id"), {"id": user_id}).first()
    if row is None:
        write_logs(db, actor, "unban_user", "user", str(user_id), "user not found", "failed")
        db.commit()
        return {"ok": False, "errmsg": "user not found"}

    db.execute(text("UPDATE user SET state='offline' WHERE id=:id"), {"id": user_id})
    write_logs(db, actor, "unban_user", "user", str(user_id), "manual unban", "success")
    db.commit()
    return {"ok": True, "user_id": user_id, "state": "offline"}


@router.get("/audit-logs")
def list_audit_logs(
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    actor: str = Depends(require_admin_token),
    db: Session = Depends(get_db),
) -> list[dict]:
    rows = db.execute(
        text(
            """
            SELECT id, actor, action, target_type, target_id, detail, created_at
            FROM admin_audit_log
            ORDER BY id DESC
            LIMIT :limit OFFSET :offset
            """
        ),
        {"limit": limit, "offset": offset},
    ).mappings()
    return [dict(row) for row in rows]


@router.get("/operation-logs")
def list_operation_logs(
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    actor: str = Depends(require_admin_token),
    db: Session = Depends(get_db),
) -> list[dict]:
    rows = db.execute(
        text(
            """
            SELECT id, operator_name, operation, status, detail, created_at
            FROM admin_operation_log
            ORDER BY id DESC
            LIMIT :limit OFFSET :offset
            """
        ),
        {"limit": limit, "offset": offset},
    ).mappings()
    return [dict(row) for row in rows]
