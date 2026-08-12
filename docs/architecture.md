# Security boundary

The FastAPI service authenticates accounts and stores opaque encrypted vault keys,
secret ciphertext, nonces, and optional encrypted names. It never accepts the vault
master password, a decrypted Vault Key, or plaintext secret values. Account password
authentication is independent from client-side vault encryption.

## C++ client

`main.cpp` owns CLI presentation. `ConfigManager` resolves per-user defaults and
the auth session, `AuthManager` handles account login, `ApiClient` is the validating
libcurl transport, `VaultService` owns workflows, and `CryptoService` is the sole
libsodium boundary.

Initialization generates a random 256-bit Vault Key. Argon2id derives a KEK from
the master password; XChaCha20-Poly1305 wraps the key with
`nox:v1:vault-key:<user UUID>` as AAD. Secrets use the Vault Key, a fresh nonce, and
`nox:v1:secret:<vault UUID>:<visible name>` as AAD. Binary fields use the Day 1
API's standard Base64 wire format. Master passwords, decrypted Vault Keys, and
plaintext secrets never cross `ApiClient`.
