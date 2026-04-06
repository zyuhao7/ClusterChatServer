from fastapi import APIRouter, Depends, Query
from sqlalchemy import text
from sqlalchemy.orm import Session

from app.db.database import get_db

router = APIRouter(tags=["groups"])


@router.get("/groups")
def list_groups(
    group_id: int | None = Query(default=None, ge=1),
    limit: int = Query(default=50, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[dict]:
    sql = """
    SELECT g.id, g.groupname, g.groupdesc, gu.userid, gu.grouprole
    FROM allgroup g
    LEFT JOIN groupuser gu ON gu.groupid = g.id
    """

    params: dict = {"limit": limit, "offset": offset}
    if group_id is not None:
        sql += " WHERE g.id = :group_id"
        params["group_id"] = group_id

    sql += " ORDER BY g.id, gu.userid LIMIT :limit OFFSET :offset"
    rows = db.execute(text(sql), params).mappings().all()
    return [dict(row) for row in rows]
