from datetime import datetime

from sqlalchemy import DateTime, ForeignKey, Integer, String, Text
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class User(Base):
    __tablename__ = "user"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(50), nullable=False)
    password: Mapped[str] = mapped_column(String(255), nullable=False)
    state: Mapped[str] = mapped_column(String(20), nullable=False, default="offline")
    avatar_path: Mapped[str] = mapped_column(String(255), nullable=False, default="")
    bio: Mapped[str] = mapped_column(String(255), nullable=False, default="")
    location: Mapped[str] = mapped_column(String(64), nullable=False, default="")


class Friend(Base):
    __tablename__ = "friend"

    userid: Mapped[int] = mapped_column(
        Integer, ForeignKey("user.id", ondelete="CASCADE"), primary_key=True
    )
    friendid: Mapped[int] = mapped_column(
        Integer, ForeignKey("user.id", ondelete="CASCADE"), primary_key=True
    )


class Group(Base):
    __tablename__ = "allgroup"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    groupname: Mapped[str] = mapped_column(String(50), nullable=False)
    groupdesc: Mapped[str] = mapped_column(String(255), nullable=False, default="")


class GroupUser(Base):
    __tablename__ = "groupuser"

    groupid: Mapped[int] = mapped_column(
        Integer, ForeignKey("allgroup.id", ondelete="CASCADE"), primary_key=True
    )
    userid: Mapped[int] = mapped_column(
        Integer, ForeignKey("user.id", ondelete="CASCADE"), primary_key=True
    )
    grouprole: Mapped[str] = mapped_column(String(20), nullable=False, default="normal")


class OfflineMessage(Base):
    __tablename__ = "offlinemessage"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    userid: Mapped[int] = mapped_column(
        Integer, ForeignKey("user.id", ondelete="CASCADE"), nullable=False
    )
    message: Mapped[str] = mapped_column(String(1024), nullable=False)
    request_id: Mapped[str] = mapped_column(String(64), nullable=False, default="")


class MessageHistory(Base):
    __tablename__ = "message_history"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    request_id: Mapped[str] = mapped_column(String(64), nullable=False, default="")
    sender_id: Mapped[int] = mapped_column(Integer, ForeignKey("user.id", ondelete="CASCADE"))
    receiver_id: Mapped[int | None] = mapped_column(Integer, ForeignKey("user.id", ondelete="CASCADE"))
    group_id: Mapped[int | None] = mapped_column(Integer, ForeignKey("allgroup.id", ondelete="CASCADE"))
    message: Mapped[str] = mapped_column(String(1024), nullable=False)
    msg_type: Mapped[str] = mapped_column(String(16), nullable=False)
    read_state: Mapped[str] = mapped_column(String(16), nullable=False, default="unread")
    recalled: Mapped[int] = mapped_column(Integer, nullable=False, default=0)
    created_at: Mapped[datetime | None] = mapped_column(DateTime)
    read_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    recalled_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)


class AdminAuditLog(Base):
    __tablename__ = "admin_audit_log"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    actor: Mapped[str] = mapped_column(String(64), nullable=False)
    action: Mapped[str] = mapped_column(String(64), nullable=False)
    target_type: Mapped[str] = mapped_column(String(32), nullable=False)
    target_id: Mapped[str] = mapped_column(String(64), nullable=False)
    detail: Mapped[str] = mapped_column(Text, nullable=False, default="")
    created_at: Mapped[datetime | None] = mapped_column(DateTime)


class AdminOperationLog(Base):
    __tablename__ = "admin_operation_log"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    operator_name: Mapped[str] = mapped_column(String(64), nullable=False)
    operation: Mapped[str] = mapped_column(String(128), nullable=False)
    status: Mapped[str] = mapped_column(String(16), nullable=False)
    detail: Mapped[str] = mapped_column(Text, nullable=False, default="")
    created_at: Mapped[datetime | None] = mapped_column(DateTime)
