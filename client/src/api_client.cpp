#include "nox/api_client.hpp"
#include "nox/errors.hpp"
#include <curl/curl.h>
#include <iostream>
#include <memory>

namespace nox {
namespace {
size_t receive(char *data, size_t size, size_t count, void *target) {
    static_cast<std::string *>(target)->append(data, size * count);
    return size * count;
}
struct CurlDeleter {
    void operator()(CURL *handle) const {
        curl_easy_cleanup(handle);
    }
};
struct HeaderDeleter {
    void operator()(curl_slist *value) const {
        curl_slist_free_all(value);
    }
};
} // namespace
ApiClient::ApiClient(std::string server_url, long timeout, bool verbose)
    : server_url_(std::move(server_url)), timeout_seconds_(timeout), verbose_(verbose) {
    while (server_url_.ends_with('/'))
        server_url_.pop_back();
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        throw NetworkError("Unable to initialize HTTP client");
}
void ApiClient::set_token(std::optional<std::string> token) {
    token_ = std::move(token);
}
nlohmann::json ApiClient::get(const std::string &path) const {
    return request("GET", path, std::nullopt);
}
nlohmann::json ApiClient::post(const std::string &path, const nlohmann::json &body) const {
    return request("POST", path, body);
}
nlohmann::json ApiClient::put(const std::string &path, const nlohmann::json &body) const {
    return request("PUT", path, body);
}
void ApiClient::remove(const std::string &path) const {
    (void)request("DELETE", path, std::nullopt);
}

nlohmann::json ApiClient::request(const std::string &method, const std::string &path,
                                  const std::optional<nlohmann::json> &body) const {
    std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
    if (!curl)
        throw NetworkError("Unable to create HTTP request");
    std::string response;
    const std::string url = server_url_ + "/api/v1" + path;
    const std::string payload = body ? body->dump() : "";
    curl_slist *raw_headers = nullptr;
    raw_headers = curl_slist_append(raw_headers, "Accept: application/json");
    raw_headers = curl_slist_append(raw_headers, "Content-Type: application/json");
    if (token_)
        raw_headers = curl_slist_append(raw_headers, ("Authorization: Bearer " + *token_).c_str());
    std::unique_ptr<curl_slist, HeaderDeleter> headers(raw_headers);
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    if (body) {
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, payload.data());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    }
    if (verbose_)
        std::cerr << "[verbose] " << method << ' ' << url << " (sensitive payload redacted)\n";
    const auto result = curl_easy_perform(curl.get());
    if (result != CURLE_OK)
        throw NetworkError(std::string("Network request failed: ") + curl_easy_strerror(result));
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (verbose_)
        std::cerr << "[verbose] HTTP " << status << '\n';
    nlohmann::json parsed;
    if (!response.empty()) {
        try {
            parsed = nlohmann::json::parse(response);
        } catch (...) {
            throw ServerError(static_cast<int>(status), "invalid_response", "Server returned invalid JSON");
        }
    }
    if (status >= 400) {
        std::string code = "server_error", message = "Server request failed";
        try {
            code = parsed.at("error").at("code");
            message = parsed.at("error").at("message");
        } catch (...) {
        }
        if (status == 401)
            throw AuthenticationError(message);
        if (status == 409 && code == "version_conflict")
            throw VersionConflict(static_cast<int>(status), code, message);
        throw ServerError(static_cast<int>(status), code, message);
    }
    if (status < 200 || status >= 300)
        throw ServerError(static_cast<int>(status), "unexpected_status", "Server returned an unexpected status");
    return parsed;
}
void ApiClient::check_compatibility() const {
    auto result = get("/health");
    try {
        const auto version = result.at("api_version").get<int>();
        if (result.at("status") != "ok" || version != 1)
            throw ApiCompatibilityError("Server API version " + std::to_string(version) +
                                        " is not supported by this client. Supported versions: 1");
    } catch (const nlohmann::json::exception &) {
        throw ApiCompatibilityError("Server health response is invalid");
    }
}
} // namespace nox
