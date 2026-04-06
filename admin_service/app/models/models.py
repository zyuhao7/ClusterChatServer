from sqlalchemy import ForeignKey, Integer, String
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


class User(Base):
    __tablename__ = "user"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(50), nullable=False)
    password: Mapped[str] = mapped_column(String(50), nullable=False)
    state: Mapped[str] = mapped_column(String(20), nullable=False, default="offline")


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

    userid: Mapped[int] = mapped_column(
        Integer, ForeignKey("user.id", ondelete="CASCADE"), primary_key=True
    )
    message: Mapped[str] = mapped_column(String(1024), nullable=False)
