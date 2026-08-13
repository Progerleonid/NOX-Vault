#include "nox/vault_service.hpp"
#include "nox/errors.hpp"
#include <algorithm>

namespace nox {
void VaultService::initialize(std::string password, bool private_metadata) {
    auto kdf = crypto_.default_kdf();
    auto vault_key = crypto_.random_vault_key();
    auto kek = crypto_.derive_kek(password, kdf);
    try {
        auto wrapped = crypto_.encrypt(vault_key, kek, CryptoService::vault_key_aad(user_id_));
        (void)api_.post("/vault", serialize_vault_create(wrapped, kdf, private_metadata));
    } catch (...) {
        CryptoService::wipe(password);
        CryptoService::wipe(kek);
        CryptoService::wipe(vault_key);
        throw;
    }
    CryptoService::wipe(password);
    CryptoService::wipe(kek);
    CryptoService::wipe(vault_key);
}
VaultMetadata VaultService::metadata() const {
    return parse_vault(api_.get("/vault"));
}
std::vector<SecretRecord> VaultService::list() const {
    auto body = api_.get("/secrets");
    if (!body.is_array())
        throw ServerError(0, "invalid_response", "Server returned an invalid secret list");
    std::vector<SecretRecord> result;
    result.reserve(body.size());
    for (const auto &item : body)
        result.push_back(parse_secret(item));
    return result;
}
Bytes VaultService::unlock(const VaultMetadata &vault, std::string &password) const {
    auto kek = crypto_.derive_kek(password, vault.kdf);
    CryptoService::wipe(password);
    try {
        auto key = crypto_.decrypt(vault.wrapped_key, kek, CryptoService::vault_key_aad(user_id_));
        CryptoService::wipe(kek);
        return key;
    } catch (const CryptoError &) {
        CryptoService::wipe(kek);
        throw InvalidMasterPassword("Unable to decrypt vault: incorrect master password or corrupted vault metadata");
    }
}
SecretRecord VaultService::find(const std::string &name, const VaultMetadata &vault, const Bytes *key) const {
    auto records = list();
    auto it = std::find_if(records.begin(), records.end(), [&](const auto &r) {
        if (r.name)
            return *r.name == name;
        if (!key || !r.encrypted_name)
            return false;
        auto p = crypto_.decrypt(*r.encrypted_name, *key, CryptoService::private_name_aad(vault.id, r.id));
        std::string decoded(p.begin(), p.end());
        CryptoService::wipe(p);
        const bool matches = decoded == name;
        CryptoService::wipe(decoded);
        return matches;
    });
    if (it == records.end())
        throw SecretNotFound("Secret not found: " + name);
    return *it;
}
bool VaultService::contains_name(const std::string &name, const VaultMetadata &vault, const Bytes &key) const {
    try {
        (void)find(name, vault, &key);
        return true;
    } catch (const SecretNotFound &) {
        return false;
    }
}
void VaultService::add(const std::string &name, const std::string &plaintext, std::string password) {
    if (name.empty())
        throw NoxError("Secret name cannot be empty");
    auto vault = metadata();
    auto key = unlock(vault, password);
    Bytes plain(plaintext.begin(), plaintext.end());
    try {
        if (contains_name(name, vault, key))
            throw NoxError("A secret with this name already exists");
        const auto id = random_uuid();
        std::optional<std::string> public_name;
        std::optional<EncryptedValue> private_name;
        std::optional<Bytes> name_hash;
        auto aad = CryptoService::secret_aad(vault.id, name);
        if (vault.private_metadata) {
            Bytes n(name.begin(), name.end());
            private_name = crypto_.encrypt(n, key, CryptoService::private_name_aad(vault.id, id));
            name_hash = crypto_.private_name_hash(name, key);
            CryptoService::wipe(n);
            aad = CryptoService::private_secret_aad(vault.id, id);
        } else
            public_name = name;
        auto encrypted = crypto_.encrypt(plain, key, aad);
        (void)api_.post("/secrets", serialize_secret(id, public_name, private_name, encrypted, name_hash));
    } catch (...) {
        CryptoService::wipe(plain);
        CryptoService::wipe(key);
        throw;
    }
    CryptoService::wipe(plain);
    CryptoService::wipe(key);
}
std::string VaultService::get(const std::string &name, std::string password) {
    auto vault = metadata();
    auto key = unlock(vault, password);
    auto record = find(name, vault, &key);
    Bytes plain;
    try {
        plain = crypto_.decrypt(record.value, key,
                                vault.private_metadata ? CryptoService::private_secret_aad(vault.id, record.id)
                                                       : CryptoService::secret_aad(vault.id, name));
    } catch (...) {
        CryptoService::wipe(key);
        throw;
    }
    CryptoService::wipe(key);
    std::string result(plain.begin(), plain.end());
    CryptoService::wipe(plain);
    return result;
}
void VaultService::update(const std::string &name, const std::string &plaintext, std::string password) {
    auto vault = metadata();
    auto key = unlock(vault, password);
    auto record = find(name, vault, &key);
    Bytes plain(plaintext.begin(), plaintext.end());
    try {
        auto encrypted = crypto_.encrypt(plain, key,
                                         vault.private_metadata ? CryptoService::private_secret_aad(vault.id, record.id)
                                                                : CryptoService::secret_aad(vault.id, name));
        auto body = serialize_secret("", record.name, record.encrypted_name, encrypted, record.name_hash);
        body["record_version"] = record.record_version;
        (void)api_.put("/secrets/" + record.id, body);
    } catch (...) {
        CryptoService::wipe(plain);
        CryptoService::wipe(key);
        throw;
    }
    CryptoService::wipe(plain);
    CryptoService::wipe(key);
}
void VaultService::remove(const std::string &name) {
    auto vault = metadata();
    if (vault.private_metadata)
        throw NoxError("Private metadata vault must be unlocked before removing secrets");
    auto record = find(name, vault);
    api_.remove("/secrets/" + record.id);
}
Bytes VaultService::unlock_with_password(std::string password) const {
    auto vault = metadata();
    return unlock(vault, password);
}
void VaultService::rotate_password(std::string old_password, std::string new_password) {
    auto vault = metadata();
    auto key = unlock(vault, old_password);
    auto kdf = crypto_.default_kdf();
    auto kek = crypto_.derive_kek(new_password, kdf);
    CryptoService::wipe(new_password);
    try {
        auto wrapped = crypto_.encrypt(key, kek, CryptoService::vault_key_aad(user_id_));
        (void)api_.put("/vault/key", serialize_vault_create(wrapped, kdf, vault.private_metadata));
    } catch (...) {
        CryptoService::wipe(key);
        CryptoService::wipe(kek);
        throw;
    }
    CryptoService::wipe(key);
    CryptoService::wipe(kek);
}
void VaultService::add_unlocked(const std::string &n, const std::string &p, const Bytes &k) {
    auto v = metadata();
    if (contains_name(n, v, k))
        throw NoxError("A secret with this name already exists");
    Bytes plain(p.begin(), p.end());
    const auto id = random_uuid();
    std::optional<std::string> pn;
    std::optional<EncryptedValue> en;
    std::optional<Bytes> name_hash;
    auto aad = CryptoService::secret_aad(v.id, n);
    if (v.private_metadata) {
        Bytes nb(n.begin(), n.end());
        en = crypto_.encrypt(nb, k, CryptoService::private_name_aad(v.id, id));
        name_hash = crypto_.private_name_hash(n, k);
        CryptoService::wipe(nb);
        aad = CryptoService::private_secret_aad(v.id, id);
    } else
        pn = n;
    try {
        auto e = crypto_.encrypt(plain, k, aad);
        CryptoService::wipe(plain);
        (void)api_.post("/secrets", serialize_secret(id, pn, en, e, name_hash));
    } catch (...) {
        CryptoService::wipe(plain);
        throw;
    }
}
std::string VaultService::get_unlocked(const std::string &n, const Bytes &k) {
    auto v = metadata();
    auto r = find(n, v, &k);
    auto p = crypto_.decrypt(r.value, k,
                             v.private_metadata ? CryptoService::private_secret_aad(v.id, r.id)
                                                : CryptoService::secret_aad(v.id, n));
    std::string out(p.begin(), p.end());
    CryptoService::wipe(p);
    return out;
}
void VaultService::update_unlocked(const std::string &n, const std::string &p, const Bytes &k) {
    auto v = metadata();
    auto r = find(n, v, &k);
    Bytes b(p.begin(), p.end());
    try {
        auto e = crypto_.encrypt(
            b, k, v.private_metadata ? CryptoService::private_secret_aad(v.id, r.id) : CryptoService::secret_aad(v.id, n));
        CryptoService::wipe(b);
        auto body = serialize_secret("", r.name, r.encrypted_name, e, r.name_hash);
        body["record_version"] = r.record_version;
        (void)api_.put("/secrets/" + r.id, body);
    } catch (...) {
        CryptoService::wipe(b);
        throw;
    }
}
void VaultService::remove_unlocked(const std::string &n, const Bytes &k) {
    auto v = metadata();
    auto r = find(n, v, &k);
    api_.remove("/secrets/" + r.id);
}
std::vector<std::string> VaultService::list_unlocked(const Bytes &k) const {
    auto v = metadata();
    std::vector<std::string> out;
    for (auto &r : list()) {
        if (r.name)
            out.push_back(*r.name);
        else {
            auto p = crypto_.decrypt(*r.encrypted_name, k, CryptoService::private_name_aad(v.id, r.id));
            out.emplace_back(p.begin(), p.end());
            CryptoService::wipe(p);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
} // namespace nox
