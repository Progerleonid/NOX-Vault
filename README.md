# NOX VAULT

```text
 _   _  ___ __  __  __     ___   _   _ _   _____
| \ | |/ _ \\ \/ /  \ \   / /_\ | | | | | |_   _|
|  \| | (_) |>  <    \ \ / / _ \| |_| | |__ | |
|_|\__|\___//_/\_\    \_/_/ \_\_\___/|____||_|
```

**Your secrets leave your machine only as ciphertext.**

NOX VAULT is an educational multi-user secrets vault. Its C++20 client encrypts and decrypts records locally; FastAPI and PostgreSQL store authenticated opaque records.

> Nox Vault is an educational coursework project. It demonstrates secure design principles but has not undergone an independent professional security audit and should not be treated as a replacement for a professionally audited password manager.

## Features and security boundary

- Argon2id derives a KEK from the vault master password and a random salt.
- A random 256-bit Vault Key is wrapped by the KEK (envelope encryption).
- XChaCha20-Poly1305 provides authenticated Secret, name and backup encryption with fresh nonces and contextual AAD.
- Password rotation re-wraps the Vault Key without re-encrypting every Secret.
- Optional private metadata encrypts Secret names locally.
- Optimistic record versions prevent silent stale updates.
- A user-scoped local agent supports `unlock` across terminals with idle and absolute timeouts.
- Encrypted export/import never creates a plaintext backup.

The API never defines fields for the vault master password, plaintext Secret, KEK or decrypted Vault Key. It does receive the separate account password over HTTPS for server authentication. See [cryptography](docs/CRYPTOGRAPHY.md), [threat model](docs/THREAT_MODEL.md), [architecture](docs/ARCHITECTURE.md) and [API](docs/API.md).

```mermaid
flowchart LR
    T["Terminal"] --> N["nox + libsodium"]
    N -->|"HTTPS ciphertext only"| C["Caddy"]
    C --> F["FastAPI"]
    F --> P[("PostgreSQL")]
```

## Repository

```text
client/                 C++20 CLI, local agent, crypto and Catch2 tests
server/                 FastAPI, SQLAlchemy, Alembic and pytest tests
docs/                   architecture, crypto, threat model and API
deploy/                 Caddy and production environment template
.github/workflows/      CI
scripts/tamper_demo.py  educational AEAD tampering demonstration
docker-compose.yml      local development stack
docker-compose.prod.yml production stack
```

## Install a release

Tagged releases provide native, system-wide installers for the CLI client. The
backend is not bundled; installed clients use `https://api.noxvault.tech` by
default.

| Platform | Supported systems | Release file |
| --- | --- | --- |
| Windows | Windows 10/11, x64 | `NOX-Vault-X.Y.Z-windows-x64.msi` |
| macOS | macOS 15+, Apple Silicon | `NOX-Vault-X.Y.Z-macos-arm64.pkg` |
| Debian/Ubuntu | Debian 12+ or Ubuntu 22.04+, x64 | `nox-vault_X.Y.Z_amd64.deb` |
| Debian/Ubuntu | Debian 12+ or Ubuntu 22.04+, ARM64 | `nox-vault_X.Y.Z_arm64.deb` |

Download the package and `SHA256SUMS` from the matching
[GitHub Release](https://github.com/Progerleonid/NOX-Vault/releases), then verify it before installation:

```bash
sha256sum --check SHA256SUMS --ignore-missing
```

On Windows, PowerShell can verify an individual download:

```powershell
Get-FileHash .\NOX-Vault-0.2.6-windows-x64.msi -Algorithm SHA256
```

Run the Windows MSI or macOS PKG and approve the administrator prompt. The
packages are currently unsigned, so Windows SmartScreen or macOS Gatekeeper may
warn before installation. On macOS, use the supported **Open Anyway** control
in System Settings > Privacy & Security after the first blocked attempt. Do not
disable Gatekeeper globally.

Install a Debian package with APT so its declared dependencies are resolved:

```bash
sudo apt install ./nox-vault_0.2.6_amd64.deb
```

Open a new terminal after installation, then run:

```bash
nox --version
nox --help
nox doctor
nox register
```

`nox doctor` contacts the official API health endpoint. It does not send a
vault password or plaintext Secret. `nox get --copy` currently has a native
clipboard backend only on Windows; macOS and Linux builds return an explicit
error instead of invoking an external clipboard process.

To uninstall on Windows, use **Installed apps**. On Linux, run
`sudo apt remove nox-vault`. On macOS, run:

```bash
sudo /usr/local/share/nox-vault/uninstall-nox-vault
```

Uninstallers preserve per-user configuration and stored sessions in
`%APPDATA%\Nox` on Windows or `~/.config/nox` on macOS/Linux.

## Building from source

Build the client locally with CMake and vcpkg.

Ubuntu/Debian build prerequisites:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ git pkg-config \
  libsodium-dev libcurl4-openssl-dev
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
cmake -S client -B client/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build client/build
ctest --test-dir client/build --output-on-failure
./client/build/nox --version
```

Windows with Visual Studio 2022 and vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
cmake -S client -B client/build -A x64 `
  -DBUILD_TESTING=ON `
  -DVCPKG_MANIFEST_FEATURES=tests `
  -DCMAKE_TOOLCHAIN_FILE="$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build client/build --config Release
ctest --test-dir client/build -C Release --output-on-failure
& ".\client\build\Release\nox.exe" --version
```

The vcpkg manifest supplies CLI11, libcurl, nlohmann-json and libsodium. Enable
the `tests` manifest feature when configuring a vcpkg build with
`-DVCPKG_MANIFEST_FEATURES=tests`; it adds Catch2.

## Creating a release

The client version has two deliberate source-of-truth declarations:
`project(... VERSION X.Y.Z)` in `client/CMakeLists.txt` and `version-string` in
`client/vcpkg.json`. Set both to the same value and push an annotated tag with
that version:

```bash
git tag -a v0.2.6 -m "NOX Vault 0.2.6"
git push origin v0.2.6
```

The release workflow rejects malformed or mismatched versions, builds and tests
all four platform artifacts, verifies their basic package contents, creates
`SHA256SUMS`, and publishes only after every build succeeds. Re-running an
existing version replaces same-named release assets but preserves unrelated
legacy files. Signing hooks are
kept disabled until Windows and Apple signing credentials are configured. The
repository variables `ENABLE_WINDOWS_SIGNING` and `ENABLE_APPLE_SIGNING` are
guard rails: setting either to `true` intentionally fails the release until its
certificate-backed command is added, preventing an accidentally unsigned
release from being published as signed.

## First use and normal workflow

```text
$ nox register
Email: alice@example.com
Account password: ************
Account created as alice@example.com.

$ nox init
Vault master password: ************
Confirm vault master password: ************
Encrypted vault initialized.

$ nox unlock
Vault master password: ************
Vault unlocked for 15 minutes of inactivity.

$ nox add github
Secret value: ************
Done.

$ nox get github
ghp_example
```

After `nox unlock`, another terminal for the same OS user can run `nox list`, `nox get github`, `nox update github` and `nox remove github`. `nox lock` or `nox logout` clears the local unlocked session. `nox status`, `nox doctor`, `nox passwd`, `nox export backup.nox`, `nox import backup.nox`, and `nox shell` cover diagnostics, rotation, portability and an interactive session. Values/passwords are prompted with terminal echo disabled; do not put them on command lines.

`nox config set server_url https://host` is only for developers, tests and self-hosting. Plain non-loopback HTTP, embedded user information, URL paths, queries and fragments are rejected. `nox config unset server_url` restores the official service.

## Backend development

```bash
cp .env.example .env
docker compose up --build -d
curl http://localhost:8000/api/v1/health

cd server
python -m pip install -e ".[dev]"
alembic upgrade head
pytest
ruff check .
```

The local Compose binds PostgreSQL and FastAPI to loopback only. Production uses `docker-compose.prod.yml`, Caddy automatic TLS, internal PostgreSQL networking, uncommitted environment secrets and persistent volumes.

On the dedicated Ubuntu VPS, copy the repository to `/opt/nox-vault` and run `sudo bash deploy/deploy.sh`. The script installs Docker from its official Ubuntu repository when needed, generates the uncommitted production secrets on the VPS, restricts UFW to SSH/80/443, starts migrations and containers, and fails unless the public HTTPS health endpoint succeeds. It deliberately preserves the selected root/password SSH policy.

For C++ diagnostics with compatible GCC/Clang builds, add `-DNOX_ENABLE_SANITIZERS=ON`. Warnings are `/W4 /permissive-` on MSVC and `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` elsewhere.

## Tampering demonstration

Install PyNaCl in an isolated environment and run:

```bash
python scripts/tamper_demo.py
```

The script creates a record whose representation does not contain the known plaintext, flips one ciphertext byte, and verifies that XChaCha20-Poly1305 rejects it. The actual C++ test suite separately covers ciphertext, nonce and AAD modification.

## Limitations

- Client malware, keyloggers or a compromised binary can capture plaintext and keys.
- An attacker knowing the master password can decrypt a stolen vault record set.
- Private metadata does not conceal identifiers, sizes, timing or access patterns.
- Login rate limiting is in-process, not a distributed production-grade limiter.
- Secure wiping is best effort because standard C++/JSON/OS components can copy or page memory.
- The server can delete, replay or withhold ciphertext; this design is not a complete rollback-resistant protocol.
- Automatic updates are not implemented.
