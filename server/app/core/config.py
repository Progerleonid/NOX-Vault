from functools import lru_cache

from pydantic import Field, field_validator
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+psycopg://nox:change-me@localhost:5432/nox_vault"
    jwt_secret: str = "development-only-secret-change-me-now"
    access_token_expire_minutes: int = Field(default=15, ge=1, le=1440)
    cors_origins: list[str] = []
    max_request_size: int = Field(default=1_048_576, ge=1024)
    max_restore_request_size: int = Field(default=100_663_296, ge=1024)
    login_rate_limit_attempts: int = Field(default=5, ge=1)
    login_rate_limit_window_seconds: int = Field(default=60, ge=1)

    @field_validator("jwt_secret")
    @classmethod
    def secure_secret_length(cls, value: str) -> str:
        if len(value) < 32:
            raise ValueError("JWT_SECRET must contain at least 32 characters")
        return value


@lru_cache
def get_settings() -> Settings:
    return Settings()
