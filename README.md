# NOX VAULT

NOX VAULT is a hosted multi-user secrets vault. Encryption and decryption belong to
the future C++ client; the API stores opaque encrypted records only.

## Run

```bash
copy .env.example .env
docker compose up --build
```

The API is available at `http://localhost:8000`, with OpenAPI at `/docs` and health
at `/api/v1/health`. Alembic migrations run before the API process starts.

## Backend development

```bash
cd server
python -m pip install -e ".[dev]"
alembic upgrade head
pytest
ruff check .
```

Tests require PostgreSQL and use `TEST_DATABASE_URL` when set, otherwise
`postgresql+psycopg://nox:change-me@localhost:5432/nox_vault_test`.

