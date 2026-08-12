import uuid
from uuid import UUID

from sqlalchemy import BigInteger, Boolean, ForeignKey, Index, LargeBinary, String, UniqueConstraint
from sqlalchemy.dialects.postgresql import UUID as PGUUID
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base
from app.models.common import TimestampMixin


class Vault(TimestampMixin, Base):
    __tablename__ = "vaults"
    __table_args__ = (
        UniqueConstraint("user_id", name="uq_vaults_user_id"),
        Index("ix_vaults_user_id", "user_id"),
    )

    id: Mapped[UUID] = mapped_column(PGUUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id: Mapped[UUID] = mapped_column(
        PGUUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE")
    )
    encrypted_vault_key: Mapped[bytes] = mapped_column(LargeBinary)
    vault_key_nonce: Mapped[bytes] = mapped_column(LargeBinary)
    kdf_salt: Mapped[bytes] = mapped_column(LargeBinary)
    kdf_algorithm: Mapped[str] = mapped_column(String(50))
    kdf_ops_limit: Mapped[int] = mapped_column(BigInteger)
    kdf_mem_limit: Mapped[int] = mapped_column(BigInteger)
    private_metadata: Mapped[bool] = mapped_column(Boolean, default=False)
    user: Mapped["User"] = relationship(back_populates="vault")
    secrets: Mapped[list["Secret"]] = relationship(back_populates="vault", cascade="all, delete-orphan")

from app.models.secret import Secret  # noqa: E402
from app.models.user import User  # noqa: E402
