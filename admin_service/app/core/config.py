from pathlib import Path

from pydantic import field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict


BASE_DIR = Path(__file__).resolve().parents[2]


class Settings(BaseSettings):
    app_name: str = "ClusterChat Admin Service"
    app_host: str = "127.0.0.1"
    app_port: int = 8010
    debug: bool = True

    mysql_host: str = "127.0.0.1"
    mysql_port: int = 3306
    mysql_user: str = "root"
    mysql_password: str = "123456"
    mysql_database: str = "chat"
    admin_token: str = "change-me"
    redis_host: str = "127.0.0.1"
    redis_port: int = 6379
    moderation_sensitive_words: str = "spam,ad,scam"
    media_root: Path = BASE_DIR / "uploads"

    model_config = SettingsConfigDict(
        env_file=BASE_DIR / ".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    @field_validator("debug", mode="before")
    @classmethod
    def parse_debug(cls, value: object) -> bool:
        if isinstance(value, bool):
            return value

        if isinstance(value, str):
            normalized = value.strip().lower()
            if normalized in {"1", "true", "yes", "on", "debug"}:
                return True
            if normalized in {"0", "false", "no", "off", "release"}:
                return False

        return bool(value)

    @property
    def database_url(self) -> str:
        return (
            f"mysql+pymysql://{self.mysql_user}:{self.mysql_password}"
            f"@{self.mysql_host}:{self.mysql_port}/{self.mysql_database}"
        )

    @property
    def sensitive_words(self) -> list[str]:
        return [w.strip().lower() for w in self.moderation_sensitive_words.split(",") if w.strip()]


settings = Settings()
