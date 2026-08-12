#include <catch2/catch_test_macros.hpp>
#include "nox/config_manager.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/models.hpp"
#include <filesystem>

using namespace nox;
TEST_CASE("encryption roundtrips empty long and Unicode values") {
    CryptoService crypto; const auto key=crypto.random_vault_key();
    for(const std::string value: {std::string{},std::string(100000,'x'),std::string("секрет-🔐")}) {
        Bytes plain(value.begin(),value.end()); auto encrypted=crypto.encrypt(plain,key,"aad");
        REQUIRE(crypto.decrypt(encrypted,key,"aad")==plain);
    }
}
TEST_CASE("tampering ciphertext nonce and AAD fails authentication") {
    CryptoService crypto; auto key=crypto.random_vault_key(); Bytes plain{'s','e','c','r','e','t'}; auto encrypted=crypto.encrypt(plain,key,"aad");
    SECTION("ciphertext") { encrypted.ciphertext[0]^=1; REQUIRE_THROWS_AS(crypto.decrypt(encrypted,key,"aad"),CryptoError); }
    SECTION("nonce") { encrypted.nonce[0]^=1; REQUIRE_THROWS_AS(crypto.decrypt(encrypted,key,"aad"),CryptoError); }
    SECTION("AAD") { REQUIRE_THROWS_AS(crypto.decrypt(encrypted,key,"other"),CryptoError); }
}
TEST_CASE("wrong password cannot unwrap a vault key") {
    CryptoService crypto; auto p=crypto.default_kdf(); auto correct=crypto.derive_kek("correct",p); auto wrong=crypto.derive_kek("wrong",p);
    auto wrapped=crypto.encrypt(crypto.random_vault_key(),correct,"vault-aad"); REQUIRE_THROWS_AS(crypto.decrypt(wrapped,wrong,"vault-aad"),CryptoError);
}
TEST_CASE("each encryption gets a unique nonce") {
    CryptoService crypto; auto key=crypto.random_vault_key(); Bytes value{'x'}; auto a=crypto.encrypt(value,key,"aad"),b=crypto.encrypt(value,key,"aad"); REQUIRE(a.nonce!=b.nonce);
}
TEST_CASE("Base64 validates and roundtrips") {
    Bytes data{0,1,2,253,254,255}; REQUIRE(base64_decode(base64_encode(data))==data); REQUIRE_THROWS_AS(base64_decode("not base64!"),CryptoError);
}
TEST_CASE("secret serialization roundtrips and rejects unknown formats") {
    EncryptedValue value{1,crypto_algorithm,Bytes{1,2,3},Bytes(24,4)}; auto json=serialize_secret("github",value);
    json["id"]="00000000-0000-0000-0000-000000000001"; json["record_version"]=1; auto parsed=parse_secret(json); REQUIRE(parsed.name=="github"); REQUIRE(parsed.value.ciphertext==value.ciphertext);
    json["version"]=2; REQUIRE_THROWS_AS(parse_secret(json),ApiCompatibilityError);
}
TEST_CASE("config defaults override reset and URL policy") {
    auto directory=std::filesystem::temp_directory_path()/"nox-client-test-config"; std::filesystem::remove_all(directory); ConfigManager config(directory);
    REQUIRE(config.effective_server_url()==NOX_DEFAULT_SERVER_URL); REQUIRE_FALSE(config.has_server_override()); config.set("server_url","https://vault.example"); REQUIRE(config.effective_server_url()=="https://vault.example");
    config.unset("server_url"); REQUIRE_FALSE(config.has_server_override()); REQUIRE_NOTHROW(ConfigManager::validate_server_url("http://localhost:8000"));
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("http://example.com"),ConfigurationError); REQUIRE_THROWS_AS(ConfigManager::validate_server_url("http://localhost.evil"),ConfigurationError); std::filesystem::remove_all(directory);
}
