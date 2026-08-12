# NOX VAULT

NOX VAULT is a hosted multi-user secrets vault. Encryption and decryption are
performed by the C++20 client; the API stores opaque encrypted records only.

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

## Build and install the client

Use the included vcpkg manifest:

```bash
cmake -S client -B client/build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build client/build --config Release
ctest --test-dir client/build -C Release --output-on-failure
cmake --install client/build --config Release --prefix "$HOME/.local"
```

Tests are a default vcpkg feature. For an application-only dependency install use
`-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON -DBUILD_TESTING=OFF`.

Add `$HOME/.local/bin` to `PATH` for a per-user Linux installation, or install to
`/usr/local`. On Windows, use a stable prefix such as
`%LOCALAPPDATA%\\Programs\\Nox` and add its `bin` directory to the user `PATH` once.
The installed executable is `nox` (`nox.exe` on Windows).

Production builds set the official endpoint with
`-DNOX_DEFAULT_SERVER_URL=https://vault.example.com`. Development defaults to
`http://localhost:8000`. `nox config set server_url ...` is an advanced override;
`nox config unset server_url` restores the compiled default. Non-loopback HTTP is
rejected and TLS verification cannot be disabled.

## Client workflow

```text
nox register
nox init
nox add github
nox list
nox get github --stdout
nox update github
nox remove github
```

Passwords and values are read with terminal echo disabled. Plaintext output
requires `--stdout`. Safe diagnostics are provided by `nox status`, `nox doctor`,
and `nox --verbose <command>`.
