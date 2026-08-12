from fastapi import APIRouter

from app.api import auth, health, secrets, vaults

api_router = APIRouter(prefix="/api/v1")
api_router.include_router(auth.router)
api_router.include_router(vaults.router)
api_router.include_router(secrets.router)
api_router.include_router(health.router)

