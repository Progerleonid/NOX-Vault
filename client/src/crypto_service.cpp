#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include <limits>
#include <sodium.h>

namespace nox {
CryptoService::CryptoService() { if (sodium_init() < 0) throw CryptoError("Unable to initialize libsodium"); }
KdfParameters CryptoService::default_kdf() const {
    KdfParameters result; result.salt.resize(crypto_pwhash_SALTBYTES); randombytes_buf(result.salt.data(), result.salt.size());
    result.ops_limit = crypto_pwhash_OPSLIMIT_INTERACTIVE; result.mem_limit = crypto_pwhash_MEMLIMIT_INTERACTIVE; return result;
}
Bytes CryptoService::random_vault_key() const { Bytes key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES); randombytes_buf(key.data(), key.size()); return key; }
Bytes CryptoService::derive_kek(const std::string& password, const KdfParameters& p) const {
    if (p.algorithm != "argon2id" || p.salt.size() != crypto_pwhash_SALTBYTES || p.ops_limit > std::numeric_limits<unsigned long long>::max())
        throw CryptoError("Invalid vault KDF parameters");
    Bytes key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
    if (crypto_pwhash(key.data(), key.size(), password.data(), static_cast<unsigned long long>(password.size()), p.salt.data(),
                      static_cast<unsigned long long>(p.ops_limit), p.mem_limit, crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw CryptoError("Unable to derive the vault key encryption key");
    return key;
}
EncryptedValue CryptoService::encrypt(const Bytes& plaintext, const Bytes& key, const std::string& aad) const {
    if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) throw CryptoError("Invalid encryption key");
    EncryptedValue out; out.nonce.resize(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    randombytes_buf(out.nonce.data(), out.nonce.size()); out.ciphertext.resize(plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long size = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(out.ciphertext.data(), &size, plaintext.data(), plaintext.size(),
            reinterpret_cast<const unsigned char*>(aad.data()), aad.size(), nullptr, out.nonce.data(), key.data()) != 0)
        throw CryptoError("Encryption failed");
    out.ciphertext.resize(static_cast<std::size_t>(size)); return out;
}
Bytes CryptoService::decrypt(const EncryptedValue& value, const Bytes& key, const std::string& aad) const {
    if (value.version != crypto_format_version || value.algorithm != crypto_algorithm ||
        key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES || value.nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES ||
        value.ciphertext.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES) throw CryptoError("Invalid encrypted value");
    Bytes out(value.ciphertext.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES); unsigned long long size = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(out.data(), &size, nullptr, value.ciphertext.data(), value.ciphertext.size(),
            reinterpret_cast<const unsigned char*>(aad.data()), aad.size(), value.nonce.data(), key.data()) != 0)
        throw CryptoError("Encrypted value authentication failed");
    out.resize(static_cast<std::size_t>(size)); return out;
}
std::string CryptoService::vault_key_aad(const std::string& user_id) { return "nox:v1:vault-key:" + user_id; }
std::string CryptoService::secret_aad(const std::string& vault_id, const std::string& name) { return "nox:v1:secret:" + vault_id + ":" + name; }
void CryptoService::wipe(Bytes& value) noexcept { if (!value.empty()) sodium_memzero(value.data(), value.size()); value.clear(); }
void CryptoService::wipe(std::string& value) noexcept { if (!value.empty()) sodium_memzero(value.data(), value.size()); value.clear(); }
}
