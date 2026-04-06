from fastapi import APIRouter, Depends, Query
from sqlalchemy import text
from sqlalchemy.orm import Session

from admin_service.app.db.database import get_db

router = APIRouter(tags=["friends"])


@router.get("/friends")
def list_friendships(
    user_id: int | None = Query(default=None, ge=1),
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[dict]:
    sql = """
    SELECT f.userid, f.friendid, u.name AS friend_name, u.state AS friend_state
    FROM friend f
    LEFT JOIN user u ON u.id = f.friendid
    """

    params: dict = {"limit": limit, "offset": offset}
    if user_id is not None:
        sql += " WHERE f.userid = :user_id"
        params["user_id"] = user_id

    sql += " ORDER BY f.userid, f.friendid LIMIT :limit OFFSET :offset"

    rows = db.execute(text(sql), params).mappings().all()
    return [dict(row) for row in rows]
