from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app import __version__
from app.api.router import api_router
from app.core.config import get_settings
from app.core.errors import install_error_handlers
from app.core.middleware import RequestSizeLimitMiddleware, SecurityHeadersMiddleware

settings = get_settings()
app = FastAPI(title="NOX VAULT API", version=__version__)
app.add_middleware(SecurityHeadersMiddleware)
app.add_middleware(RequestSizeLimitMiddleware, max_bytes=settings.max_request_size)
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=False,
    allow_methods=["GET", "POST", "PUT", "DELETE"],
    allow_headers=["Authorization", "Content-Type"],
)
install_error_handlers(app)
app.include_router(api_router)

