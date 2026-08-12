import os
from collections.abc import Generator

os.environ["DATABASE_URL"] = os.getenv(
    "TEST_DATABASE_URL", "postgresql+psycopg://nox:change-me@localhost:5432/nox_vault_test"
)
os.environ["JWT_SECRET"] = "test-secret-with-at-least-thirty-two-characters"
os.environ["LOGIN_RATE_LIMIT_ATTEMPTS"] = "3"
os.environ["LOGIN_RATE_LIMIT_WINDOW_SECONDS"] = "60"
os.environ["MAX_REQUEST_SIZE"] = "2048"

import pytest
from alembic import command
from alembic.config import Config
from fastapi.testclient import TestClient
from sqlalchemy import text

from app.api.auth import login_limiter
from app.core.database import Base, engine
from app.main import app


@pytest.fixture(scope="session", autouse=True)
def schema() -> Generator[None, None, None]:
    alembic_config = Config("alembic.ini")
    command.upgrade(alembic_config, "head")
    yield
    command.downgrade(alembic_config, "base")


@pytest.fixture(autouse=True)
def clean_database() -> Generator[None, None, None]:
    login_limiter.reset()
    with engine.begin() as connection:
        for table in reversed(Base.metadata.sorted_tables):
            connection.execute(text(f'TRUNCATE TABLE "{table.name}" RESTART IDENTITY CASCADE'))
    yield


@pytest.fixture
def client() -> TestClient:
    return TestClient(app)


@pytest.fixture
def user_factory(client: TestClient):
    counter = 0

    def create(email: str | None = None) -> tuple[dict, dict[str, str]]:
        nonlocal counter
        counter += 1
        credentials = {"email": email or f"user{counter}@example.com", "password": "correct horse battery"}
        response = client.post("/api/v1/auth/register", json=credentials)
        assert response.status_code == 201
        body = response.json()
        return credentials, {"Authorization": f"Bearer {body['access_token']}"}

    return create


@pytest.fixture
def vault_payload() -> dict[str, object]:
    return {
        "encrypted_vault_key": "a2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tr",
        "vault_key_nonce": "bm5ubm5ubm5ubm5ubm5ubm5ubm5ubm5u",
        "kdf_salt": "c3Nzc3Nzc3Nzc3Nzc3Nzcw==",
        "kdf_algorithm": "argon2id",
        "kdf_ops_limit": 3,
        "kdf_mem_limit": 67108864,
        "private_metadata": False,
    }


@pytest.fixture
def secret_payload() -> dict[str, object]:
    return {
        "name": "github",
        "ciphertext": "Y2NjY2NjY2NjY2NjY2NjYw==",
        "nonce": "bm5ubm5ubm5ubm5ubm5ubm5ubm5ubm5u",
        "algorithm": "xchacha20poly1305",
        "version": 1,
    }
