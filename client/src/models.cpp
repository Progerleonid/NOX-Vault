#include "nox/models.hpp"
#include "nox/errors.hpp"
#include <sodium.h>

namespace nox {
std::string base64_encode(const Bytes& value) {
    if (value.empty()) return {};
    std::string result(sodium_base64_ENCODED_LEN(value.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(result.data(), result.size(), value.data(), value.size(), sodium_base64_VARIANT_ORIGINAL);
    result.resize(result.find('\0'));
    return result;
}

Bytes base64_decode(const std::string& value) {
    if (value.empty()) throw CryptoError("Invalid empty Base64 value");
    Bytes result(value.size()); std::size_t size = 0;
    if (sodium_base642bin(result.data(), result.size(), value.data(), value.size(), nullptr, &size, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0) throw CryptoError("Invalid Base64 value");
    result.resize(size); return result;
}

static void validate_format(const int version, const std::string& algorithm) {
    if (version != crypto_format_version) throw ApiCompatibilityError("Unsupported encrypted record version");
    if (algorithm != crypto_algorithm) throw ApiCompatibilityError("Unsupported encryption algorithm");
}

VaultMetadata parse_vault(const nlohmann::json& j) {
    try {
        VaultMetadata value;
        value.id = j.at("id").get<std::string>();
        value.wrapped_key = {crypto_format_version, crypto_algorithm,
            base64_decode(j.at("encrypted_vault_key").get<std::string>()),
            base64_decode(j.at("vault_key_nonce").get<std::string>())};
        value.kdf = {j.at("kdf_algorithm").get<std::string>(), base64_decode(j.at("kdf_salt").get<std::string>()),
            j.at("kdf_ops_limit").get<std::uint64_t>(), j.at("kdf_mem_limit").get<std::size_t>()};
        if (value.kdf.algorithm != "argon2id") throw ApiCompatibilityError("Unsupported KDF algorithm");
        return value;
    } catch (const nlohmann::json::exception&) { throw ServerError(0, "invalid_response", "Server returned invalid vault metadata"); }
      catch (const CryptoError&) { throw ServerError(0, "invalid_response", "Server returned invalid vault metadata"); }
}

SecretRecord parse_secret(const nlohmann::json& j) {
    try {
        SecretRecord value;
        value.id = j.at("id").get<std::string>();
        value.name = j.at("name").get<std::string>();
        value.value = {j.at("version").get<int>(), j.at("algorithm").get<std::string>(),
            base64_decode(j.at("ciphertext").get<std::string>()), base64_decode(j.at("nonce").get<std::string>())};
        value.record_version = j.at("record_version").get<int>();
        validate_format(value.value.version, value.value.algorithm);
        return value;
    } catch (const nlohmann::json::exception&) { throw ServerError(0, "invalid_response", "Server returned an invalid secret record"); }
      catch (const CryptoError&) { throw ServerError(0, "invalid_response", "Server returned an invalid secret record"); }
}

nlohmann::json serialize_vault_create(const EncryptedValue& key, const KdfParameters& kdf) {
    validate_format(key.version, key.algorithm);
    return {{"encrypted_vault_key", base64_encode(key.ciphertext)}, {"vault_key_nonce", base64_encode(key.nonce)},
        {"kdf_salt", base64_encode(kdf.salt)}, {"kdf_algorithm", kdf.algorithm},
        {"kdf_ops_limit", kdf.ops_limit}, {"kdf_mem_limit", kdf.mem_limit}};
}
nlohmann::json serialize_secret(const std::string& name, const EncryptedValue& value) {
    validate_format(value.version, value.algorithm);
    return {{"name", name}, {"ciphertext", base64_encode(value.ciphertext)}, {"nonce", base64_encode(value.nonce)},
        {"algorithm", value.algorithm}, {"version", value.version}};
}
}
