from collections import defaultdict, deque
from collections.abc import Awaitable, Callable
from threading import Lock
from time import monotonic

from fastapi import Request, Response
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.responses import JSONResponse


class SecurityHeadersMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next: Callable[[Request], Awaitable[Response]]) -> Response:
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Referrer-Policy"] = "no-referrer"
        response.headers["Cache-Control"] = "no-store"
        return response


class RequestSizeLimitMiddleware(BaseHTTPMiddleware):
    def __init__(self, app: object, max_bytes: int, max_restore_bytes: int) -> None:
        super().__init__(app)  # type: ignore[arg-type]
        self.max_bytes = max_bytes
        self.max_restore_bytes = max_restore_bytes

    async def dispatch(self, request: Request, call_next: Callable[[Request], Awaitable[Response]]) -> Response:
        content_length = request.headers.get("content-length")
        if content_length:
            try:
                declared_length = int(content_length)
            except ValueError:
                return JSONResponse(
                    status_code=400,
                    content={
                        "error": {
                            "code": "invalid_content_length",
                            "message": "Invalid Content-Length header",
                        }
                    },
                )
            if declared_length < 0:
                return JSONResponse(
                    status_code=400,
                    content={
                        "error": {
                            "code": "invalid_content_length",
                            "message": "Invalid Content-Length header",
                        }
                    },
                )
        else:
            declared_length = 0
        max_bytes = self.max_restore_bytes if request.url.path == "/api/v1/restore" else self.max_bytes
        if declared_length > max_bytes:
            return JSONResponse(
                status_code=413,
                content={
                    "error": {
                        "code": "request_too_large",
                        "message": "Request body is too large",
                    }
                },
            )
        body = await request.body()
        if len(body) > max_bytes:
            return JSONResponse(
                status_code=413,
                content={
                    "error": {
                        "code": "request_too_large",
                        "message": "Request body is too large",
                    }
                },
            )
        return await call_next(request)


class LoginRateLimiter:
    def __init__(self, attempts: int, window_seconds: int) -> None:
        self.attempts = attempts
        self.window_seconds = window_seconds
        self._entries: defaultdict[str, deque[float]] = defaultdict(deque)
        self._lock = Lock()

    def is_limited(self, key: str) -> bool:
        now = monotonic()
        with self._lock:
            entries = self._entries[key]
            while entries and now - entries[0] >= self.window_seconds:
                entries.popleft()
            return len(entries) >= self.attempts

    def add_failure(self, key: str) -> None:
        with self._lock:
            if key not in self._entries and len(self._entries) >= 10_000:
                self._entries.pop(next(iter(self._entries)))
            self._entries[key].append(monotonic())

    def clear(self, key: str) -> None:
        with self._lock:
            self._entries.pop(key, None)

    def reset(self) -> None:
        with self._lock:
            self._entries.clear()
