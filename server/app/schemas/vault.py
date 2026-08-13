import base64
from datetime import datetime
from typing import Literal
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field, field_serializer, model_validator

from app.schemas.common import Base64Bytes


class VaultCreate(BaseModel):
    encrypted_vault_key: Base64Bytes
    vault_key_nonce: Base64Bytes
    kdf_salt: Base64Bytes
    kdf_algorithm: Literal["argon2id"]
    kdf_ops_limit: int = Field(gt=0)
    kdf_mem_limit: int = Field(gt=0)
    private_metadata: bool = False

    @model_validator(mode="after")
    def validate_crypto_sizes(self) -> "VaultCreate":
        if len(self.vault_key_nonce) != 24 or len(self.kdf_salt) != 16:
            raise ValueError("vault nonce or KDF salt has an invalid size")
        if len(self.encrypted_vault_key) != 48:
            raise ValueError("wrapped Vault Key has an invalid size")
        return self


class VaultKeyUpdate(BaseModel):
    encrypted_vault_key: Base64Bytes
    vault_key_nonce: Base64Bytes
    kdf_salt: Base64Bytes
    kdf_algorithm: Literal["argon2id"]
    kdf_ops_limit: int = Field(gt=0)
    kdf_mem_limit: int = Field(gt=0)

    @model_validator(mode="after")
    def validate_crypto_sizes(self) -> "VaultKeyUpdate":
        if len(self.vault_key_nonce) != 24 or len(self.kdf_salt) != 16:
            raise ValueError("vault nonce or KDF salt has an invalid size")
        if len(self.encrypted_vault_key) != 48:
            raise ValueError("wrapped Vault Key has an invalid size")
        return self


class VaultResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: UUID
    encrypted_vault_key: bytes
    vault_key_nonce: bytes
    kdf_salt: bytes
    kdf_algorithm: str
    kdf_ops_limit: int
    kdf_mem_limit: int
    private_metadata: bool
    created_at: datetime
    updated_at: datetime

    @field_serializer("encrypted_vault_key", "vault_key_nonce", "kdf_salt")
    def serialize_bytes(self, value: bytes) -> str:
        return base64.b64encode(value).decode("ascii")
