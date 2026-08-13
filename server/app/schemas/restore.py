from typing import Literal
from uuid import UUID

from pydantic import BaseModel, Field, model_validator

from app.schemas.common import Base64Bytes


class RestoreSecret(BaseModel):
    id: UUID
    name: str | None = Field(default=None, min_length=1, max_length=255)
    encrypted_name: Base64Bytes | None = None
    ciphertext: Base64Bytes
    nonce: Base64Bytes
    algorithm: Literal["xchacha20poly1305"]
    version: Literal[1]
    record_version: int = Field(ge=1)

    @model_validator(mode="after")
    def exactly_one_name(self) -> "RestoreSecret":
        if (self.name is None) == (self.encrypted_name is None):
            raise ValueError("exactly one of name and encrypted_name is required")
        return self


class RestoreRequest(BaseModel):
    format_version: Literal[1]
    source_user_id: UUID
    replace: bool = False
    vault_id: UUID
    encrypted_vault_key: Base64Bytes
    vault_key_nonce: Base64Bytes
    kdf_salt: Base64Bytes
    kdf_algorithm: Literal["argon2id"]
    kdf_ops_limit: int = Field(gt=0)
    kdf_mem_limit: int = Field(gt=0)
    private_metadata: bool
    secrets: list[RestoreSecret] = Field(max_length=10000)

    @model_validator(mode="after")
    def validate_crypto_sizes(self) -> "RestoreRequest":
        if len(self.vault_key_nonce) != 24 or len(self.kdf_salt) != 16:
            raise ValueError("vault nonce or KDF salt has an invalid size")
        if len(self.encrypted_vault_key) != 48:
            raise ValueError("wrapped Vault Key has an invalid size")
        for secret in self.secrets:
            if len(secret.nonce) != 24 or len(secret.ciphertext) < 16:
                raise ValueError("secret nonce or ciphertext has an invalid size")
            if secret.encrypted_name is not None and len(secret.encrypted_name) < 45:
                raise ValueError("encrypted name has an invalid size")
        return self
