#include "nox/auth_manager.hpp"
#include "nox/errors.hpp"
#include <chrono>

namespace nox {
AuthSession AuthManager::authenticate(const std::string &email, const std::string &password, bool registration) {
    auto body = api_.post(registration ? "/auth/register" : "/auth/login", {{"email", email}, {"password", password}});
    try {
        const auto now =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        AuthSession session{body.at("access_token"), body.at("user").at("id"), body.at("user").at("email"),
                            now + body.at("expires_in").get<std::int64_t>()};
        config_.save_session(session);
        api_.set_token(session.access_token);
        return session;
    } catch (const nlohmann::json::exception &) {
        throw ServerError(0, "invalid_response", "Server returned invalid authentication data");
    }
}
void AuthManager::logout() {
    config_.clear_session();
    api_.set_token(std::nullopt);
}
AuthSession AuthManager::require_session() const {
    auto session = config_.load_session();
    if (!session)
        throw AuthenticationError("Please log in first");
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (session->expires_at <= now)
        throw AuthenticationError("Authentication session expired; please log in again");
    return *session;
}
} // namespace nox
