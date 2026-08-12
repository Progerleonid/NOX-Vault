import base64
from datetime import datetime
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field, field_serializer

from app.schemas.common import Base64Bytes


class VaultCreate(BaseModel):
    encrypted_vault_key: Base64Bytes
    vault_key_nonce: Base64Bytes
    kdf_salt: Base64Bytes
    kdf_algorithm: str = Field(min_length=1, max_length=50)
    kdf_ops_limit: int = Field(gt=0)
    kdf_mem_limit: int = Field(gt=0)


class VaultKeyUpdate(VaultCreate):
    pass


class VaultResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: UUID
    encrypted_vault_key: bytes
    vault_key_nonce: bytes
    kdf_salt: bytes
    kdf_algorithm: str
    kdf_ops_limit: int
    kdf_mem_limit: int
    created_at: datetime
    updated_at: datetime

    @field_serializer("encrypted_vault_key", "vault_key_nonce", "kdf_salt")
    def serialize_bytes(self, value: bytes) -> str:
        return base64.b64encode(value).decode("ascii")

