# Threat model

## Security goals

NOX VAULT aims to keep Secret plaintext confidential when the PostgreSQL database, a database backup, or stored API records are stolen; when a server administrator inspects database contents; and when network traffic is intercepted while HTTPS is correctly configured and certificate verification succeeds. AEAD also detects record modification before plaintext is returned.

The server necessarily sees account email, account authentication input during login, visible Secret names when private metadata is disabled, record sizes/timing, identifiers and ciphertext. Account authentication is not zero knowledge.

## Adversaries and exclusions

The design does not fully protect against malware or keyloggers on the client, an attacker who knows the master password, a compromised C++ binary or dependency, or an attacker controlling the client while the vault is unlocked. It also does not hide traffic patterns or all metadata. This educational project has not received an independent audit.

Online account-login guessing is limited per process by client IP plus email and produces generic credential errors. It is not distributed rate limiting and resets on API restart; a production system should use a shared limiter and edge controls. Offline vault-password guessing is different: a stolen wrapped key, salt and KDF parameters let an attacker attempt guesses without the server. Argon2id's time and memory cost is the relevant mitigation, so users still need a strong unique master password.

## Key scenarios

- Database leak: the attacker obtains encrypted Vault Keys, Secret ciphertext/nonces and metadata, but not the master password or plaintext Vault Key.
- Ciphertext tampering: XChaCha20-Poly1305 authentication fails for changes to ciphertext, nonce or bound context.
- Network interception: Caddy provides TLS and the client keeps peer/host verification enabled. A compromised trusted CA, host or client remains outside this guarantee.
- Malicious server: it can delete, replay, withhold or correlate records and attempt denial of service. Record versioning detects concurrent stale updates, not every rollback attack.
- Unlocked workstation: the local agent and requesting processes can access Secrets until timeout or explicit lock/logout.

## Operational assumptions

PostgreSQL is confined to an internal Docker network in production; only SSH/HTTP/HTTPS are allowed by the host firewall. Production secrets live in an uncommitted VPS environment file. Root password SSH remains enabled by deployment choice and is a known hardening limitation.
