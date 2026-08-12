from uuid import UUID

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.models import Secret, Vault


def get_secret_for_user(db: Session, secret_id: UUID, user_id: UUID) -> Secret | None:
    return db.scalar(
        select(Secret)
        .join(Vault, Secret.vault_id == Vault.id)
        .where(Secret.id == secret_id, Vault.user_id == user_id)
    )


def list_secrets_for_user(db: Session, user_id: UUID) -> list[Secret]:
    return list(
        db.scalars(
            select(Secret)
            .join(Vault, Secret.vault_id == Vault.id)
            .where(Vault.user_id == user_id)
            .order_by(Secret.created_at, Secret.id)
        )
    )

