from typing import Annotated

import jwt
from fastapi import Depends
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy.orm import Session

from app.core.database import get_db
from app.core.errors import APIError
from app.core.security import decode_access_token
from app.models import User

DbSession = Annotated[Session, Depends(get_db)]
bearer = HTTPBearer(auto_error=False)


def get_current_user(
    db: DbSession, credentials: Annotated[HTTPAuthorizationCredentials | None, Depends(bearer)]
) -> User:
    if credentials is None:
        raise APIError(401, "authentication_required", "Authentication is required")
    try:
        user_id = decode_access_token(credentials.credentials)
    except (jwt.PyJWTError, ValueError, KeyError):
        raise APIError(401, "invalid_token", "Authentication token is invalid or expired") from None
    user = db.get(User, user_id)
    if user is None:
        raise APIError(401, "invalid_token", "Authentication token is invalid or expired")
    return user


CurrentUser = Annotated[User, Depends(get_current_user)]

