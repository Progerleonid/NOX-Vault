from datetime import UTC, datetime, timedelta

from app.core.security import create_access_token


def test_health(client):
    response = client.get("/api/v1/health")
    assert response.status_code == 200
    assert response.json() == {"status": "ok", "database": "ok", "version": "0.1.0", "api_version": 1}
    assert response.headers["x-content-type-options"] == "nosniff"


def test_registration_normalizes_email_and_duplicate(client):
    credentials = {"email": "Alice@Example.COM", "password": "correct horse battery"}
    response = client.post("/api/v1/auth/register", json=credentials)
    assert response.status_code == 201
    assert response.json()["user"]["email"] == "alice@example.com"
    duplicate = client.post("/api/v1/auth/register", json={**credentials, "email": "alice@example.com"})
    assert duplicate.status_code == 409
    assert duplicate.json()["error"]["code"] == "email_registered"


def test_login_and_generic_incorrect_credentials(client, user_factory):
    credentials, _ = user_factory("login@example.com")
    response = client.post("/api/v1/auth/login", json=credentials)
    assert response.status_code == 200
    wrong = client.post("/api/v1/auth/login", json={**credentials, "password": "incorrect password"})
    missing = client.post("/api/v1/auth/login", json={**credentials, "email": "missing@example.com"})
    assert wrong.status_code == missing.status_code == 401
    assert wrong.json() == missing.json()


def test_unauthenticated_invalid_and_expired_token(client, user_factory):
    assert client.get("/api/v1/vault").status_code == 401
    assert client.get("/api/v1/vault", headers={"Authorization": "Bearer nonsense"}).status_code == 401
    _, headers = user_factory()
    user_id = client.get("/api/v1/vault", headers=headers)  # validates the normal token shape
    assert user_id.status_code == 404
    from app.core.security import decode_access_token
    token_id = decode_access_token(headers["Authorization"].split()[1])
    expired, _ = create_access_token(token_id, now=datetime.now(UTC) - timedelta(days=1))
    assert client.get("/api/v1/vault", headers={"Authorization": f"Bearer {expired}"}).status_code == 401


def test_login_rate_limit(client, user_factory):
    credentials, _ = user_factory("rate@example.com")
    bad = {**credentials, "password": "incorrect password"}
    for _ in range(3):
        assert client.post("/api/v1/auth/login", json=bad).status_code == 401
    response = client.post("/api/v1/auth/login", json=bad)
    assert response.status_code == 429


def test_successful_logins_do_not_trigger_rate_limit(client, user_factory):
    credentials, _ = user_factory("successful@example.com")
    for _ in range(5):
        assert client.post("/api/v1/auth/login", json=credentials).status_code == 200


def test_request_size_limit(client):
    response = client.post(
        "/api/v1/auth/register",
        content=b"x" * 2049,
        headers={"content-type": "application/json", "content-length": "2049"},
    )
    assert response.status_code == 413
