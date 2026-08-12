from sqlalchemy import select

from app.core.database import SessionLocal
from app.models import AuditEvent


def test_vault_create_get_duplicate_and_rewrap(client, user_factory, vault_payload):
    _, headers = user_factory()
    created = client.post("/api/v1/vault", json=vault_payload, headers=headers)
    assert created.status_code == 201
    assert client.get("/api/v1/vault", headers=headers).json()["kdf_algorithm"] == "argon2id"
    assert client.post("/api/v1/vault", json=vault_payload, headers=headers).status_code == 409
    changed = {**vault_payload, "encrypted_vault_key": "bmV3LWtleQ=="}
    response = client.put("/api/v1/vault/key", json=changed, headers=headers)
    assert response.status_code == 200
    assert response.json()["encrypted_vault_key"] == "bmV3LWtleQ=="
    with SessionLocal() as db:
        event_types = list(db.scalars(select(AuditEvent.event_type)))
    assert event_types == ["master_key_rewrapped"]


def test_secret_crud_conflict_and_audit(client, user_factory, vault_payload, secret_payload):
    _, headers = user_factory()
    client.post("/api/v1/vault", json=vault_payload, headers=headers)
    created = client.post("/api/v1/secrets", json=secret_payload, headers=headers)
    assert created.status_code == 201
    secret = created.json()
    secret_id = secret["id"]
    assert client.get("/api/v1/secrets", headers=headers).json() == [secret]
    assert client.get(f"/api/v1/secrets/{secret_id}", headers=headers).json() == secret

    update = {**secret_payload, "ciphertext": "bmV3LWNpcGhlcg==", "record_version": 1}
    updated = client.put(f"/api/v1/secrets/{secret_id}", json=update, headers=headers)
    assert updated.status_code == 200
    assert updated.json()["record_version"] == 2
    assert client.put(f"/api/v1/secrets/{secret_id}", json=update, headers=headers).status_code == 409
    assert client.delete(f"/api/v1/secrets/{secret_id}", headers=headers).status_code == 204
    assert client.get(f"/api/v1/secrets/{secret_id}", headers=headers).status_code == 404

    with SessionLocal() as db:
        events = list(db.scalars(select(AuditEvent.event_type).order_by(AuditEvent.timestamp)))
    assert events == ["secret_created", "secret_updated", "secret_deleted"]


def test_encrypted_name_and_name_validation(client, user_factory, vault_payload, secret_payload):
    _, headers = user_factory()
    client.post("/api/v1/vault", json=vault_payload, headers=headers)
    encrypted = {**secret_payload, "name": None, "encrypted_name": "ZW5jcnlwdGVkLW5hbWU="}
    assert client.post("/api/v1/secrets", json=encrypted, headers=headers).status_code == 201
    assert client.post("/api/v1/secrets", json={**secret_payload, "name": None}, headers=headers).status_code == 422
    both = {**secret_payload, "encrypted_name": "ZW5jcnlwdGVkLW5hbWU="}
    assert client.post("/api/v1/secrets", json=both, headers=headers).status_code == 422
    invalid = {**encrypted, "encrypted_name": "not base64!"}
    assert client.post("/api/v1/secrets", json=invalid, headers=headers).status_code == 422


def test_user_isolation(client, user_factory, vault_payload, secret_payload):
    _, headers_a = user_factory("a@example.com")
    _, headers_b = user_factory("b@example.com")
    client.post("/api/v1/vault", json=vault_payload, headers=headers_a)
    client.post("/api/v1/vault", json=vault_payload, headers=headers_b)
    secret_id = client.post("/api/v1/secrets", json=secret_payload, headers=headers_a).json()["id"]
    assert client.get(f"/api/v1/secrets/{secret_id}", headers=headers_b).status_code == 404
    update = {**secret_payload, "record_version": 1}
    assert client.put(f"/api/v1/secrets/{secret_id}", json=update, headers=headers_b).status_code == 404
    assert client.delete(f"/api/v1/secrets/{secret_id}", headers=headers_b).status_code == 404
    assert client.get("/api/v1/secrets", headers=headers_b).json() == []
