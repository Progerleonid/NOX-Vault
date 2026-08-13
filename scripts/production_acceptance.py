"""Run a disposable real-service acceptance flow with the installed Linux client.

This script is intended for the dedicated production VPS. It never prints generated
passwords or Secret values and removes its disposable database account at the end.
"""

from __future__ import annotations

import json
import os
import secrets
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import pexpect


def run(command: list[str], prompts: list[tuple[str, str]] | None = None, expect_failure: bool = False) -> str:
    child = pexpect.spawn(command[0], command[1:], env=CLIENT_ENV, encoding="utf-8", timeout=30)
    for prompt, response in prompts or []:
        child.expect_exact(prompt)
        child.sendline(response)
    child.expect(pexpect.EOF)
    output = child.before.replace("\r", "")
    exit_code = child.wait()
    if expect_failure:
        if exit_code == 0:
            raise RuntimeError(f"Expected command to fail: {command[:2]}")
    elif exit_code != 0:
        raise RuntimeError(f"Command failed ({exit_code}): {command[:2]}: {output}")
    return output


def database_sql(sql: str) -> str:
    command = [
        "docker", "compose", "--env-file", "deploy/.env.production",
        "-f", "docker-compose.prod.yml", "exec", "-T", "postgres",
        "psql", "-X", "-A", "-t", "-U", "nox", "-d", "nox_vault", "-c", sql,
    ]
    return subprocess.run(command, cwd="/opt/nox-vault", text=True, capture_output=True, check=True).stdout.strip()


WORK = Path(tempfile.mkdtemp(prefix="nox-production-acceptance-"))
CLIENT_ENV = os.environ.copy()
CLIENT_ENV.update({
    "HOME": str(WORK / "home"),
    "XDG_CONFIG_HOME": str(WORK / "config"),
    "XDG_RUNTIME_DIR": str(WORK / "run"),
})
for key in ("HOME", "XDG_CONFIG_HOME", "XDG_RUNTIME_DIR"):
    Path(CLIENT_ENV[key]).mkdir(parents=True, exist_ok=True, mode=0o700)

stamp = f"{int(time.time())}-{secrets.token_hex(4)}"
email = f"acceptance-{stamp}@example.com"
account_password = secrets.token_urlsafe(24)
old_master = secrets.token_urlsafe(24)
new_master = secrets.token_urlsafe(24)
initial_secret = f"NOX-DAY4-{secrets.token_urlsafe(32)}"
updated_secret = f"NOX-DAY4-UPDATED-{secrets.token_urlsafe(32)}"
backup = WORK / "acceptance.nox"
results: dict[str, object] = {}

try:
    results["version"] = run(["/usr/local/bin/nox", "--version"]).strip()
    results["official_url"] = run(["/usr/local/bin/nox", "config", "get", "server_url"]).strip()
    results["override_file_absent"] = not (WORK / "config" / "nox" / "config.json").exists()
    run(["/usr/local/bin/nox", "doctor"])

    run(["/usr/local/bin/nox", "register", email], [("Account password: ", account_password)])
    run(["/usr/local/bin/nox", "init"], [
        ("Vault master password: ", old_master),
        ("Confirm vault master password: ", old_master),
    ])
    run(["/usr/local/bin/nox", "unlock"], [("Vault master password: ", old_master)])
    run(["/usr/local/bin/nox", "add", "github"], [("Secret value: ", initial_secret)])
    results["roundtrip"] = initial_secret in run(["/usr/local/bin/nox", "get", "github"])

    results["cross_process_status"] = "unlocked" in run(["/usr/local/bin/nox", "status"])
    results["cross_process_get"] = initial_secret in run(["/usr/local/bin/nox", "get", "github"])

    run(["/usr/local/bin/nox", "update", "github"], [("New secret value: ", updated_secret)])
    results["update"] = updated_secret in run(["/usr/local/bin/nox", "get", "github"])

    run(["/usr/local/bin/nox", "passwd"], [
        ("Old vault master password: ", old_master),
        ("New vault master password: ", new_master),
        ("Confirm new master password: ", new_master),
    ])
    run(["/usr/local/bin/nox", "unlock"], [("Vault master password: ", new_master)])
    results["rotation_preserved_secret"] = updated_secret in run(["/usr/local/bin/nox", "get", "github"])

    run(["/usr/local/bin/nox", "export", str(backup)], [("Vault master password: ", new_master)])
    results["encrypted_backup"] = backup.exists() and updated_secret not in backup.read_text(encoding="utf-8")

    record = database_sql(
        "SELECT s.name || '|' || encode(s.ciphertext, 'hex') || '|' || encode(s.nonce, 'hex') "
        "FROM secrets s JOIN vaults v ON v.id=s.vault_id JOIN users u ON u.id=v.user_id "
        f"WHERE u.email='{email}' AND s.name='github'"
    )
    name, ciphertext_hex, nonce_hex = record.split("|")
    results["database_record"] = {
        "name": name,
        "ciphertext_present": len(ciphertext_hex) >= 32,
        "nonce_bytes": len(nonce_hex) // 2,
        "plaintext_absent": updated_secret.encode().hex() not in ciphertext_hex,
    }

    database_sql(
        "UPDATE secrets SET ciphertext=set_byte(ciphertext, 0, get_byte(ciphertext, 0) # 1) "
        "WHERE id=(SELECT s.id FROM secrets s JOIN vaults v ON v.id=s.vault_id "
        f"JOIN users u ON u.id=v.user_id WHERE u.email='{email}' AND s.name='github')"
    )
    tampered = run(["/usr/local/bin/nox", "get", "github"], expect_failure=True)
    results["tampering_rejected"] = "authentication failed" in tampered.lower()

    run(["/usr/local/bin/nox", "import", str(backup), "--replace", "--yes"],
        [("Vault master password: ", new_master)])
    results["backup_restore"] = updated_secret in run(["/usr/local/bin/nox", "get", "github"])

    run(["/usr/local/bin/nox", "remove", "github", "--yes"])
    results["remove"] = "github" not in run(["/usr/local/bin/nox", "list"])
    run(["/usr/local/bin/nox", "logout"])
    status = run(["/usr/local/bin/nox", "status"])
    results["logout_locked"] = "logged out" in status and "locked" in status

    if not all(value is True for key, value in results.items() if key not in {"version", "official_url", "database_record"}):
        raise RuntimeError("One or more acceptance assertions failed")
    if not all(results["database_record"].values()):
        raise RuntimeError("Database opacity assertion failed")
    print(json.dumps(results, indent=2, sort_keys=True))
finally:
    try:
        database_sql(f"DELETE FROM users WHERE email='{email}'")
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
