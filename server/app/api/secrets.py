from uuid import UUID

from fastapi import APIRouter, Request, Response
from sqlalchemy import update
from sqlalchemy.exc import IntegrityError

from app.api.dependencies import CurrentUser, DbSession
from app.core.errors import APIError
from app.models import Secret
from app.repositories.secrets import get_secret_for_user, list_secrets_for_user
from app.repositories.vaults import get_vault_for_user
from app.schemas.secret import SecretCreate, SecretResponse, SecretUpdate
from app.services.audit import add_audit_event

router = APIRouter(prefix="/secrets", tags=["secrets"])


@router.post("", response_model=SecretResponse, status_code=201)
def create_secret(
    payload: SecretCreate, request: Request, user: CurrentUser, db: DbSession
) -> Secret:
    vault = get_vault_for_user(db, user.id)
    if vault is None:
        raise APIError(404, "vault_not_found", "Vault was not found")
    secret = Secret(vault_id=vault.id, record_version=1, **payload.model_dump())
    db.add(secret)
    try:
        db.flush()
    except IntegrityError:
        db.rollback()
        raise APIError(409, "secret_name_exists", "A secret with this name already exists") from None
    add_audit_event(db, request, user.id, "secret_created", secret.id)
    db.commit()
    db.refresh(secret)
    return secret


@router.get("", response_model=list[SecretResponse])
def list_secrets(user: CurrentUser, db: DbSession) -> list[Secret]:
    return list_secrets_for_user(db, user.id)


@router.get("/{secret_id}", response_model=SecretResponse)
def get_secret(secret_id: UUID, user: CurrentUser, db: DbSession) -> Secret:
    secret = get_secret_for_user(db, secret_id, user.id)
    if secret is None:
        raise APIError(404, "secret_not_found", "Secret was not found")
    return secret


@router.put("/{secret_id}", response_model=SecretResponse)
def update_secret(
    secret_id: UUID, payload: SecretUpdate, request: Request, user: CurrentUser, db: DbSession
) -> Secret:
    secret = get_secret_for_user(db, secret_id, user.id)
    if secret is None:
        raise APIError(404, "secret_not_found", "Secret was not found")
    values = payload.model_dump(exclude={"record_version"})
    values["record_version"] = Secret.record_version + 1
    try:
        result = db.execute(
            update(Secret)
            .where(Secret.id == secret_id, Secret.record_version == payload.record_version)
            .values(**values)
        )
        if result.rowcount != 1:
            db.rollback()
            raise APIError(409, "version_conflict", "Secret was modified by another client")
        add_audit_event(db, request, user.id, "secret_updated", secret_id)
        db.commit()
    except IntegrityError:
        db.rollback()
        raise APIError(409, "secret_name_exists", "A secret with this name already exists") from None
    updated = get_secret_for_user(db, secret_id, user.id)
    assert updated is not None
    return updated


@router.delete("/{secret_id}", status_code=204)
def delete_secret(
    secret_id: UUID, request: Request, user: CurrentUser, db: DbSession
) -> Response:
    secret = get_secret_for_user(db, secret_id, user.id)
    if secret is None:
        raise APIError(404, "secret_not_found", "Secret was not found")
    db.delete(secret)
    add_audit_event(db, request, user.id, "secret_deleted", secret_id)
    db.commit()
    return Response(status_code=204)

