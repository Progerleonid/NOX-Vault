from fastapi import APIRouter, Request
from sqlalchemy.exc import IntegrityError

from app.api.dependencies import CurrentUser, DbSession
from app.core.errors import APIError
from app.models import Vault
from app.repositories.vaults import get_vault_for_user
from app.schemas.vault import VaultCreate, VaultKeyUpdate, VaultResponse
from app.services.audit import add_audit_event

router = APIRouter(prefix="/vault", tags=["vault"])


@router.post("", response_model=VaultResponse, status_code=201)
def create_vault(payload: VaultCreate, user: CurrentUser, db: DbSession) -> Vault:
    vault = Vault(user_id=user.id, **payload.model_dump())
    db.add(vault)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise APIError(409, "vault_exists", "A vault already exists for this account") from None
    db.refresh(vault)
    return vault


@router.get("", response_model=VaultResponse)
def get_vault(user: CurrentUser, db: DbSession) -> Vault:
    vault = get_vault_for_user(db, user.id)
    if vault is None:
        raise APIError(404, "vault_not_found", "Vault was not found")
    return vault


@router.put("/key", response_model=VaultResponse)
def update_vault_key(
    payload: VaultKeyUpdate, request: Request, user: CurrentUser, db: DbSession
) -> Vault:
    vault = get_vault_for_user(db, user.id)
    if vault is None:
        raise APIError(404, "vault_not_found", "Vault was not found")
    for field, value in payload.model_dump().items():
        setattr(vault, field, value)
    add_audit_event(db, request, user.id, "master_key_rewrapped", vault.id)
    db.commit()
    db.refresh(vault)
    return vault

