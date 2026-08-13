# Cryptography

## Implemented construction

The account password and vault master password are independent. The account password is sent only to the authentication endpoints over HTTPS and is hashed by the server with `argon2-cffi`. The master password never enters an HTTP request.

Vault initialization generates a 16-byte random salt and applies libsodium `crypto_pwhash` with Argon2id v1.3 (`crypto_pwhash_ALG_ARGON2ID13`). The stored operation and memory limits are libsodium's interactive defaults selected by the installed libsodium build. The 32-byte result is the key-encryption key (KEK).

A separately random 32-byte Vault Key encrypts all records. It is wrapped by the KEK using XChaCha20-Poly1305 and stored with its random 24-byte nonce. Password rotation authenticates and unwraps that key with the old KEK, generates a new salt/KEK, and re-wraps the same Vault Key. Secret ciphertext therefore does not need re-encryption.

Every encryption uses `randombytes_buf` for a fresh 192-bit XChaCha nonce. The 16-byte Poly1305 authentication tag is included in libsodium's combined ciphertext. Decryption checks the format, key/nonce/tag sizes and libsodium return value; modified ciphertext, nonce or AAD fails without returning plaintext. Binary fields use standard padded Base64 on JSON wires; Base64 is encoding, not encryption.

## Formats and authenticated context

Supported crypto format version is `1`; supported algorithm is `xchacha20poly1305`. The API rejects other identifiers and versions.

AAD is UTF-8 text and is not secret:

| Object | Exact AAD |
|---|---|
| Wrapped Vault Key | `nox:v1:vault-key:<user UUID>` |
| Visible-name Secret | `nox:v1:secret:<vault UUID>:<name>` |
| Private-name Secret | `nox:v1:private-secret:<vault UUID>:<record UUID>` |
| Encrypted private name | `nox:v1:private-name:<vault UUID>:<record UUID>` |
| Backup payload | `nox:v1:backup:<user UUID>` |

Private names are serialized as `NXNM`, one version byte, a 24-byte nonce and combined ciphertext. Backups use JSON magic `NOXBACKUP`, format version `1`, an authenticated encrypted payload and the already-wrapped Vault Key/KDF header. Import is restricted to the originating account UUID.

## Limits

The code overwrites many temporary `std::vector<unsigned char>` and mutable string buffers with `sodium_memzero`, but ordinary C++ strings, JSON libraries, terminal buffers, allocator copies, clipboard managers and OS paging prevent a hard guarantee that plaintext never remains in client memory. The local unlock agent deliberately retains the Vault Key until idle/absolute timeout or lock/logout. A compromised client process or binary can read data while unlocked.

The server cannot derive the KEK because it receives neither the master password nor a derived KEK, and cannot unwrap or use the Vault Key. This statement assumes the distributed client binary and endpoint are not malicious and HTTPS verification is intact.
