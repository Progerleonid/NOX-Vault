import base64
import binascii
from typing import Annotated

from pydantic import BeforeValidator


def decode_base64(value: object) -> bytes:
    if not isinstance(value, str) or not value:
        raise ValueError("must be a non-empty Base64 string")
    try:
        return base64.b64decode(value, validate=True)
    except (binascii.Error, ValueError) as exc:
        raise ValueError("must be valid Base64") from exc


Base64Bytes = Annotated[bytes, BeforeValidator(decode_base64)]

