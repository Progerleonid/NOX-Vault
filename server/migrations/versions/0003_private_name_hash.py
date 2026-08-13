"""Add keyed private-name hashes for uniqueness."""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "0003"
down_revision: str | None = "0002"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column("secrets", sa.Column("name_hash", sa.LargeBinary(), nullable=True))
    op.create_unique_constraint("uq_secret_vault_name_hash", "secrets", ["vault_id", "name_hash"])


def downgrade() -> None:
    op.drop_constraint("uq_secret_vault_name_hash", "secrets", type_="unique")
    op.drop_column("secrets", "name_hash")
