#include "nox/models.hpp"
#include "nox/errors.hpp"
#include <sodium.h>
#include <array>

namespace nox {
std::string base64_encode(const Bytes &value) {
    if (value.empty())
        return {};
    std::string result(sodium_base64_ENCODED_LEN(value.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(result.data(), result.size(), value.data(), value.size(), sodium_base64_VARIANT_ORIGINAL);
    result.resize(result.find('\0'));
    return result;
}

Bytes base64_decode(const std::string &value) {
    if (value.empty())
        throw CryptoError("Invalid empty Base64 value");
    Bytes result(value.size());
    std::size_t size = 0;
    if (sodium_base642bin(result.data(), result.size(), value.data(), value.size(), nullptr, &size, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0)
        throw CryptoError("Invalid Base64 value");
    result.resize(size);
    return result;
}

static void validate_format(const int version, const std::string &algorithm) {
    if (version != crypto_format_version)
        throw ApiCompatibilityError("Unsupported encrypted record version");
    if (algorithm != crypto_algorithm)
        throw ApiCompatibilityError("Unsupported encryption algorithm");
}

VaultMetadata parse_vault(const nlohmann::json &j) {
    try {
        VaultMetadata value;
        value.id = j.at("id").get<std::string>();
        value.wrapped_key = {crypto_format_version, crypto_algorithm,
                             base64_decode(j.at("encrypted_vault_key").get<std::string>()),
                             base64_decode(j.at("vault_key_nonce").get<std::string>())};
        value.kdf = {j.at("kdf_algorithm").get<std::string>(), base64_decode(j.at("kdf_salt").get<std::string>()),
                     j.at("kdf_ops_limit").get<std::uint64_t>(), j.at("kdf_mem_limit").get<std::size_t>()};
        value.private_metadata = j.value("private_metadata", false);
        if (value.kdf.algorithm != "argon2id")
            throw ApiCompatibilityError("Unsupported KDF algorithm");
        return value;
    } catch (const nlohmann::json::exception &) {
        throw ServerError(0, "invalid_response", "Server returned invalid vault metadata");
    } catch (const CryptoError &) {
        throw ServerError(0, "invalid_response", "Server returned invalid vault metadata");
    }
}

SecretRecord parse_secret(const nlohmann::json &j) {
    try {
        SecretRecord value;
        value.id = j.at("id").get<std::string>();
        if (j.contains("name") && !j.at("name").is_null())
            value.name = j.at("name").get<std::string>();
        if (j.contains("encrypted_name") && !j.at("encrypted_name").is_null())
            value.encrypted_name = unpack_encrypted_name(base64_decode(j.at("encrypted_name").get<std::string>()));
        if (j.contains("name_hash") && !j.at("name_hash").is_null())
            value.name_hash = base64_decode(j.at("name_hash").get<std::string>());
        if (value.name.has_value() == value.encrypted_name.has_value())
            throw CryptoError("Invalid secret metadata mode");
        value.value = {j.at("version").get<int>(), j.at("algorithm").get<std::string>(),
                       base64_decode(j.at("ciphertext").get<std::string>()),
                       base64_decode(j.at("nonce").get<std::string>())};
        value.record_version = j.at("record_version").get<int>();
        validate_format(value.value.version, value.value.algorithm);
        return value;
    } catch (const nlohmann::json::exception &) {
        throw ServerError(0, "invalid_response", "Server returned an invalid secret record");
    } catch (const CryptoError &) {
        throw ServerError(0, "invalid_response", "Server returned an invalid secret record");
    }
}

nlohmann::json serialize_vault_create(const EncryptedValue &key, const KdfParameters &kdf, bool private_metadata) {
    validate_format(key.version, key.algorithm);
    return {{"encrypted_vault_key", base64_encode(key.ciphertext)},
            {"vault_key_nonce", base64_encode(key.nonce)},
            {"kdf_salt", base64_encode(kdf.salt)},
            {"kdf_algorithm", kdf.algorithm},
            {"kdf_ops_limit", kdf.ops_limit},
            {"kdf_mem_limit", kdf.mem_limit},
            {"private_metadata", private_metadata}};
}
nlohmann::json serialize_secret(const std::string &id, const std::optional<std::string> &name,
                                const std::optional<EncryptedValue> &encrypted_name, const EncryptedValue &value,
                                const std::optional<Bytes> &name_hash) {
    validate_format(value.version, value.algorithm);
    nlohmann::json result = {{"ciphertext", base64_encode(value.ciphertext)},
                             {"nonce", base64_encode(value.nonce)},
                             {"algorithm", value.algorithm},
                             {"version", value.version}};
    if (!id.empty())
        result["id"] = id;
    if (name) {
        result["name"] = *name;
        result["encrypted_name"] = nullptr;
    } else if (encrypted_name) {
        result["name"] = nullptr;
        result["encrypted_name"] = base64_encode(pack_encrypted_name(*encrypted_name));
        if (name_hash)
            result["name_hash"] = base64_encode(*name_hash);
    } else
        throw CryptoError("Secret name is missing");
    return result;
}

std::string random_uuid() {
    std::array<unsigned char, 16> b{};
    randombytes_buf(b.data(), b.size());
    b[6] = (b[6] & 0x0f) | 0x40;
    b[8] = (b[8] & 0x3f) | 0x80;
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < b.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out.push_back('-');
        out.push_back(hex[b[i] >> 4]);
        out.push_back(hex[b[i] & 15]);
    }
    return out;
}
Bytes pack_encrypted_name(const EncryptedValue &value) {
    validate_format(value.version, value.algorithm);
    if (value.nonce.size() != 24)
        throw CryptoError("Invalid encrypted name nonce");
    static constexpr unsigned char magic[] = {'N', 'X', 'N', 'M', 1};
    Bytes out(std::begin(magic), std::end(magic));
    out.insert(out.end(), value.nonce.begin(), value.nonce.end());
    out.insert(out.end(), value.ciphertext.begin(), value.ciphertext.end());
    return out;
}
EncryptedValue unpack_encrypted_name(const Bytes &value) {
    if (value.size() < 5 + 24 + 16 || value[0] != 'N' || value[1] != 'X' || value[2] != 'N' || value[3] != 'M' ||
        value[4] != 1)
        throw CryptoError("Invalid encrypted name format");
    return {1, crypto_algorithm, Bytes(value.begin() + 29, value.end()), Bytes(value.begin() + 5, value.begin() + 29)};
}
} // namespace nox
