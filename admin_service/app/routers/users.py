from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.orm import Session

from app.db.database import get_db
from app.models import User

router = APIRouter(tags=["users"])


@router.get("/users")
def list_users(
    limit: int = Query(default=20, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[dict]:
    rows = db.query(User).offset(offset).limit(limit).all()
    return [
        {
            "id": row.id,
            "name": row.name,
            "state": row.state,
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
    }
