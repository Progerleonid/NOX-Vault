from fastapi import APIRouter, Request
from sqlalchemy.exc import IntegrityError

from app.api.dependencies import CurrentUser, DbSession
from app.core.errors import APIError
from app.models import Secret, Vault
from app.repositories.vaults import get_vault_for_user
from app.schemas.restore import RestoreRequest
from app.services.audit import add_audit_event

router = APIRouter(prefix="/restore", tags=["backup"])


@router.post("", status_code=201)
def restore_backup(payload: RestoreRequest, request: Request, user: CurrentUser, db: DbSession) -> dict[str, int]:
    if payload.source_user_id != user.id:
        raise APIError(403, "backup_account_mismatch", "Backup belongs to a different account")
    try:
        existing = get_vault_for_user(db, user.id)
        if existing is not None and not payload.replace:
            raise APIError(409, "vault_not_empty", "A vault already exists; explicit replacement is required")
        if existing is not None:
            db.delete(existing)
            db.flush()
        vault = Vault(
            id=payload.vault_id,
            user_id=user.id,
            encrypted_vault_key=payload.encrypted_vault_key,
            vault_key_nonce=payload.vault_key_nonce,
            kdf_salt=payload.kdf_salt,
            kdf_algorithm=payload.kdf_algorithm,
            kdf_ops_limit=payload.kdf_ops_limit,
            kdf_mem_limit=payload.kdf_mem_limit,
            private_metadata=payload.private_metadata,
        )
        db.add(vault)
        db.flush()
        for item in payload.secrets:
            if payload.private_metadata != (item.encrypted_name is not None):
                raise APIError(422, "metadata_mode_mismatch", "Backup mixes secret metadata modes")
            db.add(Secret(vault_id=vault.id, **item.model_dump()))
        db.flush()
        add_audit_event(db, request, user.id, "vault_restored", vault.id)
        db.commit()
    except IntegrityError:
        db.rollback()
        raise APIError(409, "restore_conflict", "Backup contains conflicting identifiers or names") from None
    except APIError:
        db.rollback()
        raise
    return {"restored_secrets": len(payload.secrets)}
