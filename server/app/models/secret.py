import uuid
from uuid import UUID

from sqlalchemy import CheckConstraint, ForeignKey, Integer, LargeBinary, String, UniqueConstraint
from sqlalchemy.dialects.postgresql import UUID as PGUUID
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base
from app.models.common import TimestampMixin


class Secret(TimestampMixin, Base):
    __tablename__ = "secrets"
    __table_args__ = (
        CheckConstraint("(name IS NULL) <> (encrypted_name IS NULL)", name="ck_secret_one_name"),
        UniqueConstraint("vault_id", "name", name="uq_secret_vault_name"),
        UniqueConstraint("vault_id", "name_hash", name="uq_secret_vault_name_hash"),
    )

    id: Mapped[UUID] = mapped_column(PGUUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    vault_id: Mapped[UUID] = mapped_column(
        PGUUID(as_uuid=True), ForeignKey("vaults.id", ondelete="CASCADE"), index=True
    )
    name: Mapped[str | None] = mapped_column(String(255), nullable=True)
    encrypted_name: Mapped[bytes | None] = mapped_column(LargeBinary, nullable=True)
    name_hash: Mapped[bytes | None] = mapped_column(LargeBinary, nullable=True)
    ciphertext: Mapped[bytes] = mapped_column(LargeBinary)
    nonce: Mapped[bytes] = mapped_column(LargeBinary)
    algorithm: Mapped[str] = mapped_column(String(50))
    version: Mapped[int] = mapped_column(Integer)
    record_version: Mapped[int] = mapped_column(Integer, default=1)
    vault: Mapped["Vault"] = relationship(back_populates="secrets")

from app.models.vault import Vault  # noqa: E402
