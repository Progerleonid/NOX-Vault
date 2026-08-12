# Security boundary

The FastAPI service authenticates accounts and stores opaque encrypted vault keys,
secret ciphertext, nonces, and optional encrypted names. It never accepts the vault
master password, a decrypted Vault Key, or plaintext secret values. Account password
authentication is independent from client-side vault encryption.

