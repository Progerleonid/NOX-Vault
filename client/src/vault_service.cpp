#include "nox/vault_service.hpp"
#include "nox/errors.hpp"
#include <algorithm>

namespace nox {
void VaultService::initialize(std::string password) {
    auto kdf = crypto_.default_kdf(); auto vault_key = crypto_.random_vault_key(); auto kek = crypto_.derive_kek(password, kdf);
    try { auto wrapped = crypto_.encrypt(vault_key, kek, CryptoService::vault_key_aad(user_id_)); (void)api_.post("/vault", serialize_vault_create(wrapped, kdf)); }
    catch (...) { CryptoService::wipe(password); CryptoService::wipe(kek); CryptoService::wipe(vault_key); throw; }
    CryptoService::wipe(password); CryptoService::wipe(kek); CryptoService::wipe(vault_key);
}
VaultMetadata VaultService::metadata() const { return parse_vault(api_.get("/vault")); }
std::vector<SecretRecord> VaultService::list() const {
    auto body = api_.get("/secrets"); if (!body.is_array()) throw ServerError(0,"invalid_response","Server returned an invalid secret list");
    std::vector<SecretRecord> result; result.reserve(body.size()); for (const auto& item : body) result.push_back(parse_secret(item)); return result;
}
Bytes VaultService::unlock(const VaultMetadata& vault, std::string& password) const {
    auto kek = crypto_.derive_kek(password, vault.kdf); CryptoService::wipe(password);
    try { auto key = crypto_.decrypt(vault.wrapped_key, kek, CryptoService::vault_key_aad(user_id_)); CryptoService::wipe(kek); return key; }
    catch (const CryptoError&) { CryptoService::wipe(kek); throw InvalidMasterPassword("Unable to decrypt vault: incorrect master password or corrupted vault metadata"); }
}
SecretRecord VaultService::find(const std::string& name) const {
    auto records = list(); auto it = std::find_if(records.begin(),records.end(),[&](const auto& r){return r.name==name;});
    if (it == records.end()) throw SecretNotFound("Secret not found: " + name);
    return *it;
}
void VaultService::add(const std::string& name, const std::string& plaintext, std::string password) {
    if (name.empty()) throw NoxError("Secret name cannot be empty");
    auto vault = metadata(); auto key = unlock(vault,password);
    Bytes plain(plaintext.begin(),plaintext.end());
    try { auto encrypted=crypto_.encrypt(plain,key,CryptoService::secret_aad(vault.id,name)); (void)api_.post("/secrets",serialize_secret(name,encrypted)); }
    catch (...) { CryptoService::wipe(plain); CryptoService::wipe(key); throw; } CryptoService::wipe(plain); CryptoService::wipe(key);
}
std::string VaultService::get(const std::string& name, std::string password) {
    auto vault=metadata(); auto record=find(name); auto key=unlock(vault,password); Bytes plain;
    try { plain=crypto_.decrypt(record.value,key,CryptoService::secret_aad(vault.id,name)); } catch (...) { CryptoService::wipe(key); throw; }
    CryptoService::wipe(key); std::string result(plain.begin(),plain.end()); CryptoService::wipe(plain); return result;
}
void VaultService::update(const std::string& name,const std::string& plaintext,std::string password) {
    auto vault=metadata(); auto record=find(name); auto key=unlock(vault,password); Bytes plain(plaintext.begin(),plaintext.end());
    try { auto encrypted=crypto_.encrypt(plain,key,CryptoService::secret_aad(vault.id,name)); auto body=serialize_secret(name,encrypted); body["record_version"]=record.record_version; (void)api_.put("/secrets/"+record.id,body); }
    catch (...) { CryptoService::wipe(plain); CryptoService::wipe(key); throw; } CryptoService::wipe(plain); CryptoService::wipe(key);
}
void VaultService::remove(const std::string& name) { auto record=find(name); api_.remove("/secrets/"+record.id); }
}
