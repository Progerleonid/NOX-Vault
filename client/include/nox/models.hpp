#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace nox {
using Bytes = std::vector<unsigned char>;
inline constexpr int crypto_format_version = 1;
inline constexpr const char* crypto_algorithm = "xchacha20poly1305";

struct KdfParameters { std::string algorithm{"argon2id"}; Bytes salt; std::uint64_t ops_limit{}; std::size_t mem_limit{}; };
struct EncryptedValue { int version{crypto_format_version}; std::string algorithm{crypto_algorithm}; Bytes ciphertext; Bytes nonce; };
struct VaultMetadata { std::string id; EncryptedValue wrapped_key; KdfParameters kdf; };
struct SecretRecord { std::string id; std::string name; EncryptedValue value; int record_version{}; };
struct AuthSession { std::string access_token; std::string user_id; std::string email; std::int64_t expires_at{}; };

std::string base64_encode(const Bytes& value);
Bytes base64_decode(const std::string& value);
VaultMetadata parse_vault(const nlohmann::json& json);
SecretRecord parse_secret(const nlohmann::json& json);
nlohmann::json serialize_vault_create(const EncryptedValue& key, const KdfParameters& kdf);
nlohmann::json serialize_secret(const std::string& name, const EncryptedValue& value);
}
