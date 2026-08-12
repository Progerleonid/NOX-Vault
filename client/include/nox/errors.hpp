#pragma once

#include <stdexcept>
#include <string>

namespace nox {
class NoxError : public std::runtime_error {
  public:
    using runtime_error::runtime_error;
};
class NetworkError : public NoxError {
  public:
    using NoxError::NoxError;
};
class AuthenticationError : public NoxError {
  public:
    using NoxError::NoxError;
};
class CryptoError : public NoxError {
  public:
    using NoxError::NoxError;
};
class InvalidMasterPassword : public CryptoError {
  public:
    using CryptoError::CryptoError;
};
class SecretNotFound : public NoxError {
  public:
    using NoxError::NoxError;
};
class ConfigurationError : public NoxError {
  public:
    using NoxError::NoxError;
};
class ServerError : public NoxError {
  public:
    ServerError(int status, std::string code, const std::string &message)
        : NoxError(message), status_(status), code_(std::move(code)) {
    }
    [[nodiscard]] int status() const noexcept {
        return status_;
    }
    [[nodiscard]] const std::string &code() const noexcept {
        return code_;
    }

  private:
    int status_;
    std::string code_;
};
class VersionConflict : public ServerError {
  public:
    using ServerError::ServerError;
};
class ApiCompatibilityError : public NoxError {
  public:
    using NoxError::NoxError;
};
} // namespace nox
