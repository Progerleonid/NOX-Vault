from ipaddress import ip_address as parse_ip_address
from uuid import UUID

from fastapi import Request
from sqlalchemy.orm import Session

from app.models import AuditEvent


def add_audit_event(
    db: Session, request: Request, user_id: UUID, event_type: str, resource_id: UUID | None = None
) -> None:
    forwarded = request.headers.get("x-forwarded-for")
    ip_address = forwarded.split(",", 1)[0].strip() if forwarded else None
    if ip_address is None and request.client:
        ip_address = request.client.host
    try:
        ip_address = str(parse_ip_address(ip_address)) if ip_address else None
    except ValueError:
        # ASGI test clients and some trusted proxies may expose a hostname instead of an IP.
        ip_address = None
    db.add(
        AuditEvent(
            user_id=user_id,
            event_type=event_type,
            resource_id=resource_id,
            ip_address=ip_address,
            user_agent=request.headers.get("user-agent"),
        )
    )
