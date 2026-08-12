from fastapi import APIRouter, Request
from sqlalchemy.exc import IntegrityError

from app.api.dependencies import DbSession
from app.core.config import get_settings
from app.core.errors import APIError
from app.core.middleware import LoginRateLimiter
from app.core.security import create_access_token, hash_password, verify_password
from app.models import User
from app.repositories.users import get_user_by_email
from app.schemas.auth import AuthResponse, Credentials
from app.services.audit import add_audit_event

router = APIRouter(prefix="/auth", tags=["authentication"])
settings = get_settings()
login_limiter = LoginRateLimiter(
    settings.login_rate_limit_attempts, settings.login_rate_limit_window_seconds
)


def auth_response(user: User) -> AuthResponse:
    token, expires_in = create_access_token(user.id)
    return AuthResponse(access_token=token, expires_in=expires_in, user=user)


@router.post("/register", response_model=AuthResponse, status_code=201)
def register(payload: Credentials, db: DbSession) -> AuthResponse:
    user = User(email=str(payload.email), password_hash=hash_password(payload.password))
    db.add(user)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise APIError(409, "email_registered", "An account with this email already exists") from None
    db.refresh(user)
    return auth_response(user)


@router.post("/login", response_model=AuthResponse)
def login(payload: Credentials, request: Request, db: DbSession) -> AuthResponse:
    client_ip = request.client.host if request.client else "unknown"
    key = f"{client_ip}:{payload.email}"
    if login_limiter.is_limited(key):
        raise APIError(429, "login_rate_limited", "Too many login attempts; try again later")
    user = get_user_by_email(db, str(payload.email))
    if user is None or not verify_password(payload.password, user.password_hash):
        login_limiter.add_failure(key)
        raise APIError(401, "invalid_credentials", "Invalid email or password")
    login_limiter.clear(key)
    add_audit_event(db, request, user.id, "user_login", user.id)
    db.commit()
    return auth_response(user)
