import uuid
from uuid import UUID

from sqlalchemy import Index, String, UniqueConstraint
from sqlalchemy.dialects.postgresql import UUID as PGUUID
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base
from app.models.common import TimestampMixin


class User(TimestampMixin, Base):
    __tablename__ = "users"
    __table_args__ = (
        UniqueConstraint("email", name="uq_users_email"),
        Index("ix_users_email", "email"),
    )

    id: Mapped[UUID] = mapped_column(PGUUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    email: Mapped[str] = mapped_column(String(320))
    password_hash: Mapped[str] = mapped_column(String(512))
    vault: Mapped["Vault | None"] = relationship(back_populates="user", cascade="all, delete-orphan")
    audit_events: Mapped[list["AuditEvent"]] = relationship(cascade="all, delete-orphan")

from app.models.audit import AuditEvent  # noqa: E402
from app.models.vault import Vault  # noqa: E402
