#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace nox {
using Bytes = std::vector<unsigned char>;
inline constexpr int crypto_format_version = 1;
inline constexpr const char* crypto_algorithm = "xchacha20poly1305";

struct KdfParameters { std::string algorithm{"argon2id"}; Bytes salt; std::uint64_t ops_limit{}; std::size_t mem_limit{}; };
struct EncryptedValue { int version{crypto_format_version}; std::string algorithm{crypto_algorithm}; Bytes ciphertext; Bytes nonce; };
struct VaultMetadata { std::string id; EncryptedValue wrapped_key; KdfParameters kdf; bool private_metadata{}; };
struct SecretRecord { std::string id; std::optional<std::string> name; std::optional<EncryptedValue> encrypted_name; EncryptedValue value; int record_version{}; };
struct AuthSession { std::string access_token; std::string user_id; std::string email; std::int64_t expires_at{}; };

std::string base64_encode(const Bytes& value);
Bytes base64_decode(const std::string& value);
VaultMetadata parse_vault(const nlohmann::json& json);
SecretRecord parse_secret(const nlohmann::json& json);
nlohmann::json serialize_vault_create(const EncryptedValue& key, const KdfParameters& kdf, bool private_metadata = false);
nlohmann::json serialize_secret(const std::string& id, const std::optional<std::string>& name,
                                const std::optional<EncryptedValue>& encrypted_name, const EncryptedValue& value);
std::string random_uuid();
Bytes pack_encrypted_name(const EncryptedValue& value);
EncryptedValue unpack_encrypted_name(const Bytes& value);
}
