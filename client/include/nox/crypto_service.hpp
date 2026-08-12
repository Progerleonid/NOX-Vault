#pragma once

#include <string>
#include "nox/models.hpp"

namespace nox {
class CryptoService {
public:
    CryptoService();
    [[nodiscard]] KdfParameters default_kdf() const;
    [[nodiscard]] Bytes random_vault_key() const;
    [[nodiscard]] Bytes derive_kek(const std::string& password, const KdfParameters& parameters) const;
    [[nodiscard]] EncryptedValue encrypt(const Bytes& plaintext, const Bytes& key, const std::string& aad) const;
    [[nodiscard]] Bytes decrypt(const EncryptedValue& encrypted, const Bytes& key, const std::string& aad) const;
    [[nodiscard]] static std::string vault_key_aad(const std::string& user_id);
    [[nodiscard]] static std::string secret_aad(const std::string& vault_id, const std::string& name);
    [[nodiscard]] static std::string private_secret_aad(const std::string& vault_id, const std::string& secret_id);
    [[nodiscard]] static std::string private_name_aad(const std::string& vault_id, const std::string& secret_id);
    [[nodiscard]] static std::string backup_aad(const std::string& user_id);
    static void wipe(Bytes& value) noexcept;
    static void wipe(std::string& value) noexcept;
};
}
