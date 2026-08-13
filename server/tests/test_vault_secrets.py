from sqlalchemy import select

from app.core.database import SessionLocal
from app.models import AuditEvent


def test_vault_create_get_duplicate_and_rewrap(client, user_factory, vault_payload):
    _, headers = user_factory()
    created = client.post("/api/v1/vault", json=vault_payload, headers=headers)
    assert created.status_code == 201
    assert client.get("/api/v1/vault", headers=headers).json()["kdf_algorithm"] == "argon2id"
    assert client.post("/api/v1/vault", json=vault_payload, headers=headers).status_code == 409
    changed = {
        **vault_payload,
        "encrypted_vault_key": "b2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2tr",
    }
    changed.pop("private_metadata")
    response = client.put("/api/v1/vault/key", json=changed, headers=headers)
    assert response.status_code == 200
    assert response.json()["encrypted_vault_key"] == changed["encrypted_vault_key"]
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

    update = {**secret_payload, "ciphertext": "eHh4eHh4eHh4eHh4eHh4eA==", "record_version": 1}
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
    client.post("/api/v1/vault", json={**vault_payload, "private_metadata": True}, headers=headers)
    encrypted = {
        **secret_payload,
        "name": None,
        "encrypted_name": "ZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVl",
    }
    assert client.post("/api/v1/secrets", json=encrypted, headers=headers).status_code == 201
    assert client.post("/api/v1/secrets", json={**secret_payload, "name": None}, headers=headers).status_code == 422
    both = {**secret_payload, "encrypted_name": "ZW5jcnlwdGVkLW5hbWU="}
    assert client.post("/api/v1/secrets", json=both, headers=headers).status_code == 422
    invalid = {**encrypted, "encrypted_name": "not base64!"}
    assert client.post("/api/v1/secrets", json=invalid, headers=headers).status_code == 422


def test_metadata_mode_is_enforced(client, user_factory, vault_payload, secret_payload):
    _, headers = user_factory()
    client.post("/api/v1/vault", json=vault_payload, headers=headers)
    encrypted = {
        **secret_payload,
        "name": None,
        "encrypted_name": "ZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVl",
    }
    response = client.post("/api/v1/secrets", json=encrypted, headers=headers)
    assert response.status_code == 422
    assert response.json()["error"]["code"] == "metadata_mode_mismatch"


def test_restore_requires_same_account_and_explicit_replace(client, user_factory, vault_payload, secret_payload):
    import uuid

    _, headers = user_factory()
    token_payload = client.post("/api/v1/vault", json=vault_payload, headers=headers).json()
    from app.core.security import decode_access_token
    user_id = decode_access_token(headers["Authorization"].split()[1])
    restore = {
        "format_version": 1,
        "source_user_id": str(user_id),
        "replace": False,
        "vault_id": str(uuid.uuid4()),
        **vault_payload,
        "secrets": [{"id": str(uuid.uuid4()), "record_version": 1, **secret_payload}],
    }
    assert token_payload["private_metadata"] is False
    assert client.post("/api/v1/restore", json=restore, headers=headers).status_code == 409
    restore["replace"] = True
    response = client.post("/api/v1/restore", json=restore, headers=headers)
    assert response.status_code == 201
    assert response.json() == {"restored_secrets": 1}
    assert client.get("/api/v1/vault", headers=headers).json()["id"] == restore["vault_id"]


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


def test_rejects_unsupported_crypto_contracts(client, user_factory, vault_payload, secret_payload):
    _, headers = user_factory("formats@example.com")
    bad_kdf = {**vault_payload, "kdf_algorithm": "pbkdf2"}
    assert client.post("/api/v1/vault", json=bad_kdf, headers=headers).status_code == 422
    assert client.post("/api/v1/vault", json=vault_payload, headers=headers).status_code == 201
    bad_algorithm = {**secret_payload, "algorithm": "aes-gcm"}
    bad_version = {**secret_payload, "version": 2}
    assert client.post("/api/v1/secrets", json=bad_algorithm, headers=headers).status_code == 422
    assert client.post("/api/v1/secrets", json=bad_version, headers=headers).status_code == 422


def test_api_payload_is_opaque_and_has_no_client_keys(client, user_factory, vault_payload, secret_payload):
    _, headers = user_factory("opaque@example.com")
    known_plaintext = "DAY4-PLAINTEXT-MUST-NOT-REACH-SERVER"
    forbidden_fields = {"master_password", "vault_key", "plaintext", "secret_value"}
    assert forbidden_fields.isdisjoint(vault_payload)
    assert forbidden_fields.isdisjoint(secret_payload)
    assert known_plaintext not in str(vault_payload)
    assert known_plaintext not in str(secret_payload)
    client.post("/api/v1/vault", json=vault_payload, headers=headers)
    stored = client.post("/api/v1/secrets", json=secret_payload, headers=headers).json()
    assert known_plaintext not in str(stored)
