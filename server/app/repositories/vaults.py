from uuid import UUID

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.models import Vault


def get_vault_for_user(db: Session, user_id: UUID) -> Vault | None:
    return db.scalar(select(Vault).where(Vault.user_id == user_id))

