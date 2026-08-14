#include "nox/gui/app_controller.hpp"
#include "nox/gui/password_generator.hpp"
#include "nox/agent.hpp"
#include "nox/api_client.hpp"
#include "nox/auth_manager.hpp"
#include "nox/backup_service.hpp"
#include "nox/clipboard.hpp"
#include "nox/config_manager.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/vault_service.hpp"
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QDateTime>
#include <QUrl>
#include <QtConcurrentRun>
#include <filesystem>

namespace nox::gui {
namespace {
std::filesystem::path gui_executable() {
#ifdef _WIN32
    return std::filesystem::path(QCoreApplication::applicationFilePath().toStdWString());
#else
    return std::filesystem::path(QCoreApplication::applicationFilePath().toStdString());
#endif
}

nlohmann::json agent_request(const std::string &operation, const AuthSession &session,
                             const ConfigManager &config) {
    const auto settings = config.load();
    return {{"op", operation}, {"server_url", config.effective_server_url()},
            {"timeout", settings.timeout_seconds}, {"token", session.access_token},
            {"user_id", session.user_id}};
}

QString qstring(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string string(const QString &value) {
    const auto utf8 = value.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

void add_settings(QVariantMap &result, const ConfigManager &config) {
    const auto settings = config.load();
    result["serverUrl"] = qstring(config.effective_server_url());
    result["requestTimeout"] = static_cast<int>(settings.timeout_seconds);
    result["autoLockTimeout"] = static_cast<int>(settings.unlock_timeout_seconds);
    result["clipboardTimeout"] = static_cast<int>(settings.clipboard_timeout_seconds);
    result["startLocked"] = settings.start_locked;
    result["language"] = qstring(settings.language);
    QVariantList accounts;
    const auto active = config.load_session();
    for (const auto &session : config.list_sessions())
        accounts.push_back(QVariantMap{{"userId", qstring(session.user_id)}, {"email", qstring(session.email)},
                                       {"active", active && active->user_id == session.user_id}});
    result["accounts"] = accounts;
}

QString records_revision(const std::vector<SecretRecord> &records) {
    QString result;
    for (const auto &record : records)
        result += qstring(record.id) + ':' + QString::number(record.record_version) + ':' +
                  qstring(record.updated_at.value_or("")) + ';';
    return result;
}

QVariantList records_with_timestamps(const QStringList &names, const std::vector<SecretRecord> &records) {
    QHash<QString, QString> timestamps;
    for (const auto &record : records) {
        if (record.name && record.updated_at)
            timestamps.insert(qstring(*record.name), qstring(*record.updated_at));
    }
    QVariantList result;
    for (const auto &name : names)
        result.push_back(QVariantMap{{"name", name}, {"updatedAt", timestamps.value(name)}});
    return result;
}
} // namespace

AppController::AppController(QObject *parent) : QObject(parent), model_(this) {
    revealTimer_.setSingleShot(true);
    revealTimer_.setInterval(15000);
    connect(&revealTimer_, &QTimer::timeout, this, &AppController::hideSensitive);
    clipboardTimer_.setSingleShot(true);
    connect(&clipboardTimer_, &QTimer::timeout, this, [this] {
        const auto value = string(copied_);
        (void)clear_clipboard_if_matches(value);
        copied_.fill(QChar(0));
        copied_.clear();
        setNotice("Clipboard cleared");
    });
    healthTimer_.setInterval(60000);
    connect(&healthTimer_, &QTimer::timeout, this, [this] {
        if (!busy_ && screen_ == "app")
            checkHealth();
    });
    syncTimer_.setInterval(4000);
    connect(&syncTimer_, &QTimer::timeout, this, &AppController::syncSharedState);
}

AppController::~AppController() {
    shuttingDown_ = true;
    healthTimer_.stop();
    syncTimer_.stop();
    revealTimer_.stop();
    clipboardTimer_.stop();
    if (!copied_.isEmpty())
        (void)clear_clipboard_if_matches(string(copied_));
    clearSensitiveState();
}

QString AppController::screen() const { return screen_; }
QString AppController::currentPage() const { return page_; }
bool AppController::busy() const { return busy_; }
bool AppController::serverHealthy() const { return healthy_; }
bool AppController::vaultUnlocked() const { return unlocked_; }
QString AppController::email() const { return email_; }
QString AppController::errorMessage() const { return error_; }
QString AppController::notice() const { return notice_; }
QString AppController::selectedSecret() const { return selected_; }
QString AppController::revealedSecret() const { return revealed_; }
QString AppController::generatedPassword() const { return generated_; }
QString AppController::diagnostics() const { return diagnostics_; }
QString AppController::serverUrl() const { return serverUrl_; }
int AppController::requestTimeout() const { return requestTimeout_; }
int AppController::autoLockTimeout() const { return autoLockTimeout_; }
int AppController::clipboardTimeout() const { return clipboardTimeout_; }
bool AppController::startLocked() const { return startLocked_; }
QString AppController::language() const { return language_; }
QVariantList AppController::accounts() const { return accounts_; }
QString AppController::version() const { return QStringLiteral(NOX_VERSION); }
SecretsModel *AppController::secretsModel() { return &model_; }

void AppController::dispatch(std::function<QVariantMap()> operation, Completion completion) {
    if (busy_ || shuttingDown_)
        return;
    ++stateGeneration_;
    busy_ = true;
    error_.clear();
    notice_.clear();
    emit stateChanged();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, completion] {
        QVariantMap result = watcher->result();
        watcher->deleteLater();
        if (shuttingDown_)
            return;
        busy_ = false;
        if (result.value("sessionExpired").toBool()) {
            clearSensitiveState();
            model_.clear();
            selected_.clear();
            email_.clear();
            unlocked_ = false;
            screen_ = "login";
        }
        if (result.contains("error"))
            error_ = result.value("error").toString();
        else if (completion)
            completion(result);
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([operation = std::move(operation)] {
        try {
            return operation();
        } catch (const AuthenticationError &) {
            try {
                ConfigManager config;
                config.clear_session();
                AgentClient agent(gui_executable());
                if (agent.available())
                    (void)agent.request({{"op", "lock"}});
            } catch (...) {
            }
            return QVariantMap{{"error", "Session expired. Please sign in again."}, {"sessionExpired", true}};
        } catch (const NetworkError &error) {
            const auto message = std::string(error.what());
            return QVariantMap{{"error", qstring(message.find("timed out") != std::string::npos
                                                      ? "Request timed out."
                                                      : "Unable to reach NOX Vault server.")}};
        } catch (const InvalidMasterPassword &) {
            return QVariantMap{{"error", "Incorrect master password or damaged vault metadata."}};
        } catch (const VersionConflict &) {
            return QVariantMap{{"error", "Secret changed on another client. Refresh and try again."}};
        } catch (const SecretNotFound &) {
            return QVariantMap{{"error", "Secret not found."}};
        } catch (const NoxError &error) {
            return QVariantMap{{"error", qstring(error.what())}};
        } catch (const std::exception &) {
            return QVariantMap{{"error", "Unexpected client failure."}};
        }
    }));
}

QVariantMap AppController::startupSnapshot() {
    QVariantMap result;
    ConfigManager config;
    add_settings(result, config);
    const auto settings = config.load();
    ApiClient api(config.effective_server_url(), settings.timeout_seconds);
    api.check_compatibility();
    result["healthy"] = true;
    const auto session = config.load_session();
    if (!session) {
        result["screen"] = "login";
        return result;
    }
    result["email"] = qstring(session->email);
    result["userId"] = qstring(session->user_id);
    api.set_token(session->access_token);
    CryptoService crypto;
    VaultService vault(api, crypto, session->user_id);
    try {
        (void)vault.metadata();
    } catch (const ServerError &error) {
        if (error.status() == 404) {
            result["screen"] = "setup";
            return result;
        }
        throw;
    }
    AgentClient agent(gui_executable());
    if (settings.start_locked && agent.available())
        (void)agent.request({{"op", "lock"}});
    const bool unlocked = agent.available() && agent.request({{"op", "status"}}).value("unlocked", false);
    result["unlocked"] = unlocked;
    result["screen"] = unlocked ? "app" : "unlock";
    if (unlocked) {
        auto request = agent_request("list", *session, config);
        const auto response = agent.request(request);
        QStringList names;
        for (const auto &name : response.at("names"))
            names.push_back(qstring(name.get<std::string>()));
        const auto records = vault.list();
        result["records"] = records_with_timestamps(names, records);
        result["revision"] = records_revision(records);
    }
    return result;
}

void AppController::applyStartup(const QVariantMap &result) {
    healthy_ = result.value("healthy").toBool();
    screen_ = result.value("screen", "login").toString();
    email_ = result.value("email").toString();
    userId_ = result.value("userId").toString();
    unlocked_ = result.value("unlocked").toBool();
    serverUrl_ = result.value("serverUrl").toString();
    requestTimeout_ = result.value("requestTimeout", 15).toInt();
    autoLockTimeout_ = result.value("autoLockTimeout", 900).toInt();
    clipboardTimeout_ = result.value("clipboardTimeout", 30).toInt();
    startLocked_ = result.value("startLocked").toBool();
    language_ = result.value("language", "en").toString();
    accounts_ = result.value("accounts").toList();
    revision_ = result.value("revision").toString();
    model_.setRecords(result.value("records").toList());
    healthTimer_.start();
    syncTimer_.start();
}

void AppController::start() {
    screen_ = "loading";
    emit stateChanged();
    dispatch([] { return startupSnapshot(); }, [this](const QVariantMap &result) { applyStartup(result); });
}
void AppController::retry() {
    dispatch([] { return startupSnapshot(); }, [this](const QVariantMap &result) { applyStartup(result); });
}

void AppController::checkHealth() {
    if (healthRunning_ || shuttingDown_)
        return;
    healthRunning_ = true;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        healthRunning_ = false;
        if (shuttingDown_) return;
        healthy_ = result.value("healthy").toBool();
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([] {
        try {
            ConfigManager config;
            const auto settings = config.load();
            ApiClient api(config.effective_server_url(), settings.timeout_seconds);
            api.check_compatibility();
            return QVariantMap{{"healthy", true}};
        } catch (...) {
            return QVariantMap{{"healthy", false}};
        }
    }));
}

void AppController::syncSharedState() {
    if (syncRunning_ || busy_ || shuttingDown_ || addingAccount_)
        return;
    syncRunning_ = true;
    const auto previousUser = userId_;
    const auto previousRevision = revision_;
    const bool needsRecords = screen_ != "app";
    const auto generation = stateGeneration_;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        const auto result = watcher->result();
        watcher->deleteLater();
        syncRunning_ = false;
        if (shuttingDown_ || generation != stateGeneration_ || result.contains("error"))
            return;
        accounts_ = result.value("accounts").toList();
        language_ = result.value("language", language_).toString();
        if (!result.value("hasSession").toBool()) {
            if (!userId_.isEmpty()) {
                clearSensitiveState(); model_.clear(); selected_.clear(); userId_.clear(); email_.clear();
                revision_.clear(); unlocked_ = false; screen_ = "login";
            }
            emit stateChanged();
            return;
        }
        if (result.value("missingVault").toBool()) {
            clearSensitiveState(); model_.clear(); selected_.clear(); revision_.clear();
            unlocked_ = false; screen_ = "setup";
            emit stateChanged();
            return;
        }
        if (result.value("fullStartup").toBool()) {
            applyStartup(result);
            emit stateChanged();
            return;
        }
        email_ = result.value("email").toString();
        const bool nowUnlocked = result.value("unlocked").toBool();
        if (!nowUnlocked) {
            if (unlocked_) { clearSensitiveState(); model_.clear(); selected_.clear(); }
            unlocked_ = false; screen_ = "unlock";
        } else {
            unlocked_ = true; screen_ = "app";
            if (result.contains("records"))
                model_.setRecords(result.value("records").toList());
            revision_ = result.value("revision", revision_).toString();
        }
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([previousUser, previousRevision, needsRecords] {
        try {
            ConfigManager config;
            QVariantMap result;
            add_settings(result, config);
            const auto session = config.load_session();
            result["hasSession"] = session.has_value();
            if (!session)
                return result;
            if (qstring(session->user_id) != previousUser) {
                AgentClient previousAgent(gui_executable());
                if (previousAgent.available()) (void)previousAgent.request({{"op", "lock"}});
                result = AppController::startupSnapshot();
                result["hasSession"] = true;
                result["fullStartup"] = true;
                return result;
            }
            result["email"] = qstring(session->email);
            const auto settings = config.load();
            ApiClient api(config.effective_server_url(), settings.timeout_seconds);
            api.set_token(session->access_token);
            CryptoService crypto;
            VaultService vault(api, crypto, session->user_id);
            try {
                (void)vault.metadata();
            } catch (const ServerError &error) {
                if (error.status() != 404) throw;
                AgentClient staleAgent(gui_executable());
                if (staleAgent.available()) (void)staleAgent.request({{"op", "lock"}});
                result["missingVault"] = true;
                result["unlocked"] = false;
                return result;
            }
            AgentClient agent(gui_executable());
            const bool unlocked = agent.available() && agent.request({{"op", "status"}}).value("unlocked", false);
            result["unlocked"] = unlocked;
            if (!unlocked)
                return result;
            const auto records = vault.list();
            const auto revision = records_revision(records);
            result["revision"] = revision;
            if (needsRecords || revision != previousRevision) {
                auto request = agent_request("list", *session, config);
                const auto response = agent.request(request);
                QStringList names;
                for (const auto &name : response.at("names"))
                    names.push_back(qstring(name.get<std::string>()));
                result["records"] = records_with_timestamps(names, records);
            }
            return result;
        } catch (const AuthenticationError &) {
            try {
                ConfigManager config; config.clear_session();
                AgentClient agent(gui_executable());
                if (agent.available()) (void)agent.request({{"op", "lock"}});
                QVariantMap result; add_settings(result, config); result["hasSession"] = false; return result;
            } catch (...) { return QVariantMap{{"error", true}}; }
        } catch (...) {
            return QVariantMap{{"error", true}};
        }
    }));
}

void AppController::authenticate(QString email, QString password, bool registration) {
    dispatch([email = string(email), password = string(password), registration]() mutable {
        ConfigManager config;
        const auto settings = config.load();
        ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.check_compatibility();
        AuthManager auth(api, config);
        try {
            (void)auth.authenticate(email, password, registration);
            AgentClient agent(gui_executable());
            if (agent.available()) (void)agent.request({{"op", "lock"}});
        } catch (...) {
            CryptoService::wipe(password);
            throw;
        }
        CryptoService::wipe(password);
        return QVariantMap{};
    }, [this](const QVariantMap &) { addingAccount_ = false; start(); });
}

void AppController::initializeVault(QString password, QString confirmation, bool privateMetadata) {
    if (password != confirmation) {
        setError("Master passwords do not match.");
        return;
    }
    dispatch([password = string(password), privateMetadata]() mutable {
        ConfigManager config;
        const auto session = config.load_session();
        if (!session)
            throw AuthenticationError("Not signed in");
        const auto settings = config.load();
        ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token);
        CryptoService crypto;
        VaultService vault(api, crypto, session->user_id);
        Bytes key;
        try {
            key = vault.initialize(std::move(password), privateMetadata);
        } catch (const ServerError &error) {
            if (error.status() == 409)
                return QVariantMap{{"vaultExists", true}, {"unlocked", false}};
            throw;
        }
        try {
            AgentClient agent(gui_executable());
            agent.ensure_running();
            (void)agent.request({{"op", "unlock"}, {"key", base64_encode(key)}});
            CryptoService::wipe(key);
            return QVariantMap{{"unlocked", true}};
        } catch (...) {
            CryptoService::wipe(key);
            return QVariantMap{{"unlocked", false}};
        }
    }, [this](const QVariantMap &result) {
        unlocked_ = result.value("unlocked").toBool();
        screen_ = unlocked_ ? "app" : "unlock";
        page_ = "secrets";
        if (result.value("vaultExists").toBool())
            setNotice("Vault already exists. Unlock it with its master password.");
        else
            setNotice(unlocked_ ? "Encrypted vault created and unlocked" : "Encrypted vault created");
        if (unlocked_) refreshSecrets();
    });
}

void AppController::unlock(QString password) {
    dispatch([password = string(password)]() mutable {
        ConfigManager config;
        const auto session = config.load_session();
        if (!session)
            throw AuthenticationError("Not signed in");
        const auto settings = config.load();
        ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token);
        CryptoService crypto;
        VaultService vault(api, crypto, session->user_id);
        Bytes key;
        try {
            key = vault.unlock_with_password(std::move(password));
        } catch (const ServerError &error) {
            if (error.status() == 404)
                return QVariantMap{{"vaultMissing", true}};
            throw;
        }
        AgentClient agent(gui_executable());
        agent.ensure_running();
        (void)agent.request({{"op", "unlock"}, {"key", base64_encode(key)}});
        CryptoService::wipe(key);
        return QVariantMap{};
    }, [this](const QVariantMap &result) {
        if (result.value("vaultMissing").toBool()) {
            unlocked_ = false; screen_ = "setup"; setNotice("Create a master password for this new vault.");
            return;
        }
        unlocked_ = true; screen_ = "app"; page_ = "secrets"; refreshSecrets();
    });
}

QVariantMap AppController::agentOperation(const std::string &operation, const std::string &name,
                                          const std::string &value) {
    ConfigManager config;
    const auto session = config.load_session();
    if (!session)
        throw AuthenticationError("Not signed in");
    AgentClient agent(gui_executable());
    if (!agent.available())
        throw NoxError("Vault is locked.");
    auto request = agent_request(operation, *session, config);
    if (!name.empty())
        request["name"] = name;
    if (!value.empty() || operation == "add" || operation == "update")
        request["value"] = value;
    const auto response = agent.request(request);
    QVariantMap result;
    if (operation == "list") {
        QStringList names;
        for (const auto &item : response.at("names"))
            names.push_back(qstring(item.get<std::string>()));
        const auto settings = config.load();
        ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token);
        CryptoService crypto;
        VaultService vault(api, crypto, session->user_id);
        result["records"] = records_with_timestamps(names, vault.list());
    } else if (operation == "get") {
        result["value"] = qstring(response.at("value").get<std::string>());
    }
    return result;
}

void AppController::lock() {
    clearSensitiveState();
    model_.clear();
    selected_.clear();
    dispatch([] {
        AgentClient agent(gui_executable());
        if (agent.available())
            (void)agent.request({{"op", "lock"}});
        return QVariantMap{};
    }, [this](const QVariantMap &) { unlocked_ = false; screen_ = "unlock"; });
}

void AppController::logout() {
    clearSensitiveState();
    model_.clear();
    selected_.clear();
    dispatch([] {
        AgentClient agent(gui_executable());
        if (agent.available())
            (void)agent.request({{"op", "lock"}});
        ConfigManager config;
        const auto settings = config.load();
        ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        AuthManager auth(api, config);
        auth.logout();
        return QVariantMap{};
    }, [this](const QVariantMap &) {
        unlocked_ = false; email_.clear(); userId_.clear(); revision_.clear(); screen_ = "login";
        ConfigManager config; QVariantMap state; add_settings(state, config); accounts_ = state.value("accounts").toList();
    });
}

void AppController::addAccount() {
    clearSensitiveState(); model_.clear(); selected_.clear();
    addingAccount_ = true; screen_ = "login"; emit stateChanged();
}

void AppController::cancelAddAccount() {
    if (!addingAccount_) return;
    addingAccount_ = false;
    start();
}

void AppController::switchAccount(QString userId) {
    if (userId == userId_) return;
    clearSensitiveState(); model_.clear(); selected_.clear();
    dispatch([userId = string(userId)] {
        AgentClient agent(gui_executable());
        if (agent.available()) (void)agent.request({{"op", "lock"}});
        ConfigManager config; config.activate_session(userId);
        return startupSnapshot();
    }, [this](const QVariantMap &result) { addingAccount_ = false; applyStartup(result); });
}

void AppController::refreshSecrets() {
    dispatch([] { return agentOperation("list"); }, [this](const QVariantMap &result) {
        model_.setRecords(result.value("records").toList());
    });
}
void AppController::addSecret(QString name, QString value) {
    dispatch([name = string(name), value = string(value)]() mutable {
        try {
            auto result = agentOperation("add", name, value);
            CryptoService::wipe(value);
            return result;
        }
        catch (...) { CryptoService::wipe(value); throw; }
    }, [this](const QVariantMap &) { setNotice("Secret added"); refreshSecrets(); });
}
void AppController::updateSecret(QString name, QString value) {
    dispatch([name = string(name), value = string(value)]() mutable {
        try {
            auto result = agentOperation("update", name, value);
            CryptoService::wipe(value);
            return result;
        }
        catch (...) { CryptoService::wipe(value); throw; }
    }, [this](const QVariantMap &) { hideSensitive(); setNotice("Secret updated"); refreshSecrets(); });
}
void AppController::deleteSecret(QString name) {
    dispatch([name = string(name)] { return agentOperation("remove", name); }, [this](const QVariantMap &) {
        hideSensitive(); selected_.clear(); setNotice("Secret deleted"); refreshSecrets();
    });
}
void AppController::revealSecret(QString name) {
    hideSensitive();
    dispatch([name = string(name)] { return agentOperation("get", name); }, [this](const QVariantMap &result) {
        revealed_ = result.value("value").toString(); revealTimer_.start();
    });
}
void AppController::copySecret(QString name) {
    dispatch([name = string(name)] { return agentOperation("get", name); }, [this](const QVariantMap &result) {
        const auto value = result.value("value").toString();
        copy_to_clipboard(string(value));
        copied_ = value;
        clipboardTimer_.start(clipboardTimeout_ * 1000);
        setNotice("Secret copied. Clipboard will clear automatically.");
    });
}

void AppController::generatePassword(int length, bool upper, bool lower, bool numbers, bool symbols) {
    try { generated_ = generate_password(length, upper, lower, numbers, symbols); error_.clear(); emit stateChanged(); }
    catch (const NoxError &error) { setError(qstring(error.what())); }
}
void AppController::copyGeneratedPassword() {
    if (generated_.isEmpty()) { setError("Generate a value first."); return; }
    copy_to_clipboard(string(generated_));
    copied_ = generated_;
    clipboardTimer_.start(clipboardTimeout_ * 1000);
    setNotice("Generated value copied. Clipboard will clear automatically.");
}

void AppController::exportBackup(QString path, QString password) {
    const auto localPath = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    dispatch([path = string(localPath), password = string(password)]() mutable {
        ConfigManager config; const auto session = config.load_session();
        if (!session) throw AuthenticationError("Not signed in");
        const auto settings = config.load(); ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token); CryptoService crypto; BackupService backup(api, crypto, session->user_id);
        backup.export_file(std::filesystem::path(path), std::move(password)); return QVariantMap{};
    }, [this](const QVariantMap &) { setNotice("Encrypted backup exported"); });
}
void AppController::importBackup(QString path, QString password) {
    const auto localPath = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    dispatch([path = string(localPath), password = string(password)]() mutable {
        ConfigManager config; const auto session = config.load_session();
        if (!session) throw AuthenticationError("Not signed in");
        const auto settings = config.load(); ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token); CryptoService crypto; BackupService backup(api, crypto, session->user_id);
        backup.import_file(std::filesystem::path(path), std::move(password), true); return QVariantMap{};
    }, [this](const QVariantMap &) { lock(); setNotice("Backup restored. Unlock the restored vault."); });
}

void AppController::changeMasterPassword(QString current, QString next, QString confirmation) {
    if (next != confirmation) { setError("New master passwords do not match."); return; }
    dispatch([current = string(current), next = string(next)]() mutable {
        ConfigManager config; const auto session = config.load_session();
        if (!session) throw AuthenticationError("Not signed in");
        const auto settings = config.load(); ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.set_token(session->access_token); CryptoService crypto; VaultService vault(api, crypto, session->user_id);
        vault.rotate_password(std::move(current), std::move(next));
        AgentClient agent(gui_executable()); if (agent.available()) (void)agent.request({{"op", "lock"}});
        return QVariantMap{};
    }, [this](const QVariantMap &) { clearSensitiveState(); unlocked_ = false; screen_ = "unlock"; setNotice("Master password changed"); });
}

void AppController::runDiagnostics() {
    diagnostics_ = "Running diagnostics…\n\nChecking server, API, session and shared agent…";
    emit stateChanged();
    dispatch([] {
        ConfigManager config; const auto settings = config.load(); ApiClient api(config.effective_server_url(), settings.timeout_seconds);
        api.check_compatibility(); const auto session = config.load_session(); AgentClient agent(gui_executable());
        const bool unlocked = agent.available() && agent.request({{"op", "status"}}).value("unlocked", false);
        return QVariantMap{{"diagnostics", QString("Server\n● Reachable\n\nAPI\n● Compatible v1\n\nAuthentication\n%1\n\nVault\n%2\n\nConfiguration\n● Valid")
            .arg(session ? "● Logged in" : "○ Logged out", unlocked ? "● Unlocked" : "○ Locked")}};
    }, [this](const QVariantMap &result) { diagnostics_ = result.value("diagnostics").toString(); healthy_ = true; });
}

void AppController::saveSettings(QString url, int requestSeconds, int autoLockSeconds,
                                 int clipboardSeconds, bool startLocked, QString language) {
    dispatch([url = string(url), requestSeconds, autoLockSeconds, clipboardSeconds, startLocked,
              language = string(language)] {
        ConfigManager config;
        config.set("server_url", url);
        config.set("timeout_seconds", std::to_string(requestSeconds));
        config.set("unlock_timeout_seconds", std::to_string(autoLockSeconds));
        config.set("clipboard_timeout_seconds", std::to_string(clipboardSeconds));
        config.set("start_locked", startLocked ? "true" : "false");
        config.set("language", language);
        QVariantMap result; add_settings(result, config); return result;
    }, [this](const QVariantMap &result) {
        serverUrl_ = result.value("serverUrl").toString();
        requestTimeout_ = result.value("requestTimeout").toInt();
        autoLockTimeout_ = result.value("autoLockTimeout").toInt();
        clipboardTimeout_ = result.value("clipboardTimeout").toInt();
        startLocked_ = result.value("startLocked").toBool();
        language_ = result.value("language", "en").toString();
        setNotice("Settings saved");
        QTimer::singleShot(0, this, [this] { checkHealth(); });
    });
}

void AppController::setLanguage(QString language) {
    static const QStringList supported{"en", "ru", "pl", "de", "cs"};
    if (!supported.contains(language) || language_ == language)
        return;
    try {
        ConfigManager config; config.set("language", string(language));
        language_ = language;
        emit stateChanged();
    } catch (const NoxError &error) {
        setError(qstring(error.what()));
    }
}

QString AppController::text(QString key, QString language) const {
    static const QHash<QString, QString> english{{"secrets", "Secrets"}, {"generator", "Generator"},
        {"backup", "Backup"}, {"security", "Security"}, {"diagnostics", "Diagnostics"},
        {"settings", "Settings"}, {"logout", "Sign out"}, {"addAccount", "Add account"},
        {"language", "Language"}, {"saveSettings", "Save settings"}, {"startLocked", "Start locked"},
        {"sharedSettings", "Settings are shared with the NOX Vault CLI."}};
    static const QHash<QString, QString> russian{{"secrets", "Секреты"}, {"generator", "Генератор"},
        {"backup", "Резервные копии"}, {"security", "Безопасность"}, {"diagnostics", "Диагностика"},
        {"settings", "Настройки"}, {"logout", "Выйти"}, {"addAccount", "Добавить аккаунт"},
        {"language", "Язык"}, {"saveSettings", "Сохранить настройки"}, {"startLocked", "Запускать заблокированным"},
        {"sharedSettings", "Настройки общие для приложения и NOX Vault CLI."}};
    static const QHash<QString, QString> polish{{"secrets", "Sekrety"}, {"generator", "Generator"},
        {"backup", "Kopia zapasowa"}, {"security", "Bezpieczeństwo"}, {"diagnostics", "Diagnostyka"},
        {"settings", "Ustawienia"}, {"logout", "Wyloguj"}, {"addAccount", "Dodaj konto"},
        {"language", "Język"}, {"saveSettings", "Zapisz ustawienia"}, {"startLocked", "Uruchamiaj zablokowany"},
        {"sharedSettings", "Ustawienia są wspólne z NOX Vault CLI."}};
    static const QHash<QString, QString> german{{"secrets", "Geheimnisse"}, {"generator", "Generator"},
        {"backup", "Sicherung"}, {"security", "Sicherheit"}, {"diagnostics", "Diagnose"},
        {"settings", "Einstellungen"}, {"logout", "Abmelden"}, {"addAccount", "Konto hinzufügen"},
        {"language", "Sprache"}, {"saveSettings", "Einstellungen speichern"}, {"startLocked", "Gesperrt starten"},
        {"sharedSettings", "Einstellungen werden mit NOX Vault CLI geteilt."}};
    static const QHash<QString, QString> czech{{"secrets", "Tajné údaje"}, {"generator", "Generátor"},
        {"backup", "Záloha"}, {"security", "Zabezpečení"}, {"diagnostics", "Diagnostika"},
        {"settings", "Nastavení"}, {"logout", "Odhlásit"}, {"addAccount", "Přidat účet"},
        {"language", "Jazyk"}, {"saveSettings", "Uložit nastavení"}, {"startLocked", "Spustit uzamčené"},
        {"sharedSettings", "Nastavení je sdílené s NOX Vault CLI."}};
    const auto *selected = language == "ru" ? &russian : language == "pl" ? &polish :
                           language == "de" ? &german : language == "cs" ? &czech : &english;
    return selected->value(key, english.value(key, key));
}

void AppController::setCurrentPage(const QString &page) {
    if (page_ == page) return;
    hideSensitive();
    generated_.fill(QChar(0));
    generated_.clear();
    page_ = page;
    emit stateChanged();
}
void AppController::selectSecret(const QString &name) {
    if (selected_ == name) return;
    hideSensitive(); selected_ = name; emit stateChanged();
}
void AppController::clearMessage() { error_.clear(); notice_.clear(); emit stateChanged(); }
void AppController::hideSensitive() {
    revealTimer_.stop(); revealed_.fill(QChar(0)); revealed_.clear(); emit stateChanged();
}
void AppController::clearSensitiveState() {
    hideSensitive(); generated_.fill(QChar(0)); generated_.clear();
    if (!copied_.isEmpty()) (void)clear_clipboard_if_matches(string(copied_));
    copied_.fill(QChar(0)); copied_.clear(); clipboardTimer_.stop();
}
void AppController::setError(QString message) { error_ = std::move(message); notice_.clear(); emit stateChanged(); }
void AppController::setNotice(QString message) { notice_ = std::move(message); error_.clear(); emit stateChanged(); }
} // namespace nox::gui
