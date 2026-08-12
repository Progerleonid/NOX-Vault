from fastapi import APIRouter
from sqlalchemy import text

from app import __version__
from app.api.dependencies import DbSession

router = APIRouter(tags=["service"])


@router.get("/health")
def health(db: DbSession) -> dict[str, str | int]:
    db.execute(text("SELECT 1"))
    return {"status": "ok", "database": "ok", "version": __version__, "api_version": 1}

