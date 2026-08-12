import base64
from datetime import datetime
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field, field_serializer, model_validator

from app.schemas.common import Base64Bytes


class SecretPayload(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=255)
    encrypted_name: Base64Bytes | None = None
    ciphertext: Base64Bytes
    nonce: Base64Bytes
    algorithm: str = Field(min_length=1, max_length=50)
    version: int = Field(ge=1)

    @model_validator(mode="after")
    def exactly_one_name(self) -> "SecretPayload":
        if (self.name is None) == (self.encrypted_name is None):
            raise ValueError("exactly one of name and encrypted_name is required")
        if len(self.nonce) != 24 or len(self.ciphertext) < 16:
            raise ValueError("secret nonce or ciphertext has an invalid size")
        if self.encrypted_name is not None and len(self.encrypted_name) < 45:
            raise ValueError("encrypted name has an invalid size")
        return self


class SecretCreate(SecretPayload):
    id: UUID | None = None


class SecretUpdate(SecretPayload):
    record_version: int = Field(ge=1)


class SecretResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: UUID
    name: str | None
    encrypted_name: bytes | None
    ciphertext: bytes
    nonce: bytes
    algorithm: str
    version: int
    record_version: int
    created_at: datetime
    updated_at: datetime

    @field_serializer("encrypted_name", "ciphertext", "nonce")
    def serialize_bytes(self, value: bytes | None) -> str | None:
        return base64.b64encode(value).decode("ascii") if value is not None else None
