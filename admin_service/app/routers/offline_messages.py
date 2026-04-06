from fastapi import APIRouter, Depends, Query
from sqlalchemy import text
from sqlalchemy.orm import Session

from admin_service.app.db.database import get_db

router = APIRouter(tags=["offline-messages"])


@router.get("/offline-messages")
def list_offline_messages(
    user_id: int | None = Query(default=None, ge=1),
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[dict]:
    sql = """
    SELECT userid, message
    FROM offlinemessage
    """

    params: dict = {"limit": limit, "offset": offset}
    if user_id is not None:
        sql += " WHERE userid = :user_id"
        params["user_id"] = user_id

    sql += " ORDER BY userid LIMIT :limit OFFSET :offset"
    rows = db.execute(text(sql), params).mappings().all()
    return [dict(row) for row in rows]
