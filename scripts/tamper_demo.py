"""Educational integrity demo: opaque storage and authenticated tamper rejection.

Run after installing pynacl: python scripts/tamper_demo.py
"""

from nacl import bindings, exceptions, utils

plaintext = b"DEMO-KNOWN-PLAINTEXT-DO-NOT-STORE"
key = utils.random(bindings.crypto_aead_xchacha20poly1305_ietf_KEYBYTES)
nonce = utils.random(bindings.crypto_aead_xchacha20poly1305_ietf_NPUBBYTES)
aad = b"nox:v1:demo"
ciphertext = bindings.crypto_aead_xchacha20poly1305_ietf_encrypt(plaintext, aad, nonce, key)
stored_record = {"ciphertext": ciphertext.hex(), "nonce": nonce.hex(), "algorithm": "xchacha20poly1305"}

assert plaintext.decode() not in repr(stored_record)
tampered = bytearray(ciphertext)
tampered[0] ^= 1
try:
    bindings.crypto_aead_xchacha20poly1305_ietf_decrypt(bytes(tampered), aad, nonce, key)
except exceptions.CryptoError:
    print("PASS: stored record contains no known plaintext; tampering was rejected")
else:
    raise SystemExit("FAIL: tampered ciphertext authenticated")
