from fastapi import APIRouter, Depends, Query
from sqlalchemy import text
from sqlalchemy.orm import Session

from admin_service.app.db.database import get_db

router = APIRouter(tags=["history"])


@router.get("/history")
def list_message_history(
    user_a: int | None = Query(default=None, ge=1),
    user_b: int | None = Query(default=None, ge=1),
    group_id: int | None = Query(default=None, ge=1),
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    order: str = Query(default="desc", pattern="^(asc|desc)$"),
    db: Session = Depends(get_db),
) -> list[dict]:
    sql = """
    SELECT id, request_id, sender_id, receiver_id, group_id, message, msg_type,
           read_state, recalled, created_at, read_at, recalled_at
    FROM message_history
    """

    conditions: list[str] = []
    params: dict[str, int] = {"limit": limit, "offset": offset}

    if user_a is not None and user_b is not None:
        conditions.append(
            "((sender_id=:user_a AND receiver_id=:user_b) OR (sender_id=:user_b AND receiver_id=:user_a))"
        )
        params["user_a"] = user_a
        params["user_b"] = user_b
    elif user_a is not None:
        conditions.append("(sender_id=:user_a OR receiver_id=:user_a)")
        params["user_a"] = user_a

    if group_id is not None:
        conditions.append("group_id=:group_id")
        params["group_id"] = group_id

    if conditions:
        sql += " WHERE " + " AND ".join(conditions)

    sql += f" ORDER BY created_at {order}, id {order} LIMIT :limit OFFSET :offset"
    rows = db.execute(text(sql), params).mappings().all()
    return [dict(row) for row in rows]
