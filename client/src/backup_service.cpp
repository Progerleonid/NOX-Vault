#include "nox/backup_service.hpp"
#include "nox/errors.hpp"
#include "nox/models.hpp"
#include <fstream>

namespace nox {
namespace {
constexpr std::uintmax_t max_backup_size = 64U * 1024U * 1024U;
nlohmann::json encrypted_json(const EncryptedValue &e) {
    return {{"version", e.version},
            {"algorithm", e.algorithm},
            {"nonce", base64_encode(e.nonce)},
            {"ciphertext", base64_encode(e.ciphertext)}};
}
EncryptedValue parse_encrypted(const nlohmann::json &j) {
    return {j.at("version").get<int>(), j.at("algorithm").get<std::string>(), base64_decode(j.at("ciphertext")),
            base64_decode(j.at("nonce"))};
}
VaultMetadata parse_header_vault(const nlohmann::json &h) {
    nlohmann::json j = {{"id", h.at("vault_id")},
                        {"encrypted_vault_key", h.at("encrypted_vault_key")},
                        {"vault_key_nonce", h.at("vault_key_nonce")},
                        {"kdf_salt", h.at("kdf_salt")},
                        {"kdf_algorithm", h.at("kdf_algorithm")},
                        {"kdf_ops_limit", h.at("kdf_ops_limit")},
                        {"kdf_mem_limit", h.at("kdf_mem_limit")},
                        {"private_metadata", h.at("private_metadata")}};
    return parse_vault(j);
}
} // namespace
void BackupService::export_file(const std::filesystem::path &path, std::string password) const {
    auto vault = parse_vault(api_.get("/vault"));
    auto kek = crypto_.derive_kek(password, vault.kdf);
    CryptoService::wipe(password);
    Bytes key;
    try {
        key = crypto_.decrypt(vault.wrapped_key, kek, CryptoService::vault_key_aad(user_id_));
    } catch (...) {
        CryptoService::wipe(kek);
        throw InvalidMasterPassword("Unable to unlock vault for export");
    }
    CryptoService::wipe(kek);
    auto records = api_.get("/secrets");
    if (!records.is_array() || records.size() > 10000) {
        CryptoService::wipe(key);
        throw NoxError("Server returned an invalid backup record set");
    }
    nlohmann::json payload = {
        {"vault_id", vault.id}, {"private_metadata", vault.private_metadata}, {"secrets", records}};
    auto raw = payload.dump();
    Bytes bytes(raw.begin(), raw.end());
    auto encrypted = crypto_.encrypt(bytes, key, CryptoService::backup_aad(user_id_));
    CryptoService::wipe(bytes);
    CryptoService::wipe(key);
    auto wrapped = serialize_vault_create(vault.wrapped_key, vault.kdf, vault.private_metadata);
    nlohmann::json out = {{"magic", "NOXBACKUP"},
                          {"format_version", 1},
                          {"source_user_id", user_id_},
                          {"vault_id", vault.id},
                          {"encrypted_payload", encrypted_json(encrypted)}};
    for (auto it = wrapped.begin(); it != wrapped.end(); ++it)
        out[it.key()] = it.value();
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream f(temporary, std::ios::binary | std::ios::trunc);
        if (!f)
            throw NoxError("Unable to create backup file");
        f << out.dump();
        if (!f)
            throw NoxError("Unable to write backup file");
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::rename(temporary, path);
}
void BackupService::import_file(const std::filesystem::path &path, std::string password, bool replace) const {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > max_backup_size)
        throw NoxError("Backup file has an invalid size");
    nlohmann::json file;
    try {
        std::ifstream f(path, std::ios::binary);
        f >> file;
    } catch (...) {
        throw NoxError("Backup file is not valid JSON");
    }
    try {
        if (file.at("magic") != "NOXBACKUP" || file.at("format_version").get<int>() != 1)
            throw NoxError("Unsupported backup format");
        if (file.at("source_user_id").get<std::string>() != user_id_)
            throw NoxError("Backup belongs to a different account");
        auto vault = parse_header_vault(file);
        auto kek = crypto_.derive_kek(password, vault.kdf);
        CryptoService::wipe(password);
        Bytes key;
        try {
            key = crypto_.decrypt(vault.wrapped_key, kek, CryptoService::vault_key_aad(user_id_));
        } catch (...) {
            CryptoService::wipe(kek);
            throw InvalidMasterPassword("Unable to unlock backup");
        }
        CryptoService::wipe(kek);
        auto encrypted = parse_encrypted(file.at("encrypted_payload"));
        auto plain = crypto_.decrypt(encrypted, key, CryptoService::backup_aad(user_id_));
        CryptoService::wipe(key);
        nlohmann::json payload;
        try {
            payload = nlohmann::json::parse(plain);
        } catch (...) {
            CryptoService::wipe(plain);
            throw NoxError("Backup payload is malformed");
        }
        CryptoService::wipe(plain);
        if (payload.at("vault_id") != vault.id || payload.at("private_metadata") != vault.private_metadata ||
            !payload.at("secrets").is_array() || payload.at("secrets").size() > 10000)
            throw NoxError("Backup payload metadata is inconsistent");
        auto restore = serialize_vault_create(vault.wrapped_key, vault.kdf, vault.private_metadata);
        restore["format_version"] = 1;
        restore["source_user_id"] = user_id_;
        restore["vault_id"] = vault.id;
        restore["replace"] = replace;
        restore["secrets"] = payload.at("secrets");
        (void)api_.post("/restore", restore);
    } catch (const nlohmann::json::exception &) {
        throw NoxError("Backup file is missing required fields");
    }
}
} // namespace nox
