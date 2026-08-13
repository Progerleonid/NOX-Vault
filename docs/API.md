# HTTP API v1

Base URL: `https://api.noxvault.tech/api/v1`. JSON is used throughout. Protected routes require `Authorization: Bearer <access_token>`. FastAPI OpenAPI at `/docs` is the machine-generated contract; this page explains the stable v1 behavior.

## Errors and compatibility

Application errors use `{"error":{"code":"...","message":"..."}}`. Validation errors add `details` with location, message and type. Common statuses are `401` unauthenticated/expired, `404` absent or inaccessible resource, `409` conflict, `413` oversized body, `422` invalid schema and `429` login rate limit. Clients check `/health.api_version == 1`; crypto payload version is independently fixed at `1`.

## Endpoints

| Method and path | Authorization | Request | Success |
|---|---|---|---|
| `GET /health` | No | none | `200` status, DB status, server version, `api_version` |
| `POST /auth/register` | No | email, password (12–128 chars) | `201` bearer token, expiry, user |
| `POST /auth/login` | No | email, password | `200` same auth response |
| `POST /vault` | Yes | wrapped-key/KDF object and `private_metadata` | `201` vault object |
| `GET /vault` | Yes | none | `200` vault object |
| `PUT /vault/key` | Yes | replacement wrapped-key/KDF fields | `200` vault object |
| `POST /secrets` | Yes | optional client UUID, exactly one name form, encrypted value | `201` Secret record |
| `GET /secrets` | Yes | none | `200` array owned by caller |
| `GET /secrets/{uuid}` | Yes | none | `200` owned record |
| `PUT /secrets/{uuid}` | Yes | encrypted value/name plus current `record_version` | `200` version incremented |
| `DELETE /secrets/{uuid}` | Yes | none | `204` |
| `POST /restore` | Yes | versioned opaque vault and Secret records, optional replace | `201` restored count |

Vault crypto fields are `encrypted_vault_key` (48 decoded bytes), `vault_key_nonce` (24), `kdf_salt` (16), `kdf_algorithm: "argon2id"`, positive `kdf_ops_limit`/`kdf_mem_limit`. Binary data is Base64.

Private-metadata Secret records may include `name_hash`, a 32-byte keyed client-side hash used only to enforce unique names. Older private records and backups without this field remain readable.

A Secret contains `name` or `encrypted_name`, `ciphertext` (at least a 16-byte tag), 24-byte `nonce`, `algorithm: "xchacha20poly1305"`, `version: 1`, and server-managed `record_version`. The name representation must match the vault's fixed metadata mode. Updates atomically match the submitted record version; a stale request receives `409 version_conflict`.

Restore accepts only `format_version: 1`, the authenticated user's `source_user_id`, a vault UUID/header, metadata mode, and at most 10,000 opaque records. Replacing an existing vault requires explicit `replace: true` from the client workflow.
