#pragma once

#include "nox/gui/secrets_model.hpp"
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <functional>

namespace nox::gui {
class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString screen READ screen NOTIFY stateChanged)
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool serverHealthy READ serverHealthy NOTIFY stateChanged)
    Q_PROPERTY(bool vaultUnlocked READ vaultUnlocked NOTIFY stateChanged)
    Q_PROPERTY(QString email READ email NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY stateChanged)
    Q_PROPERTY(QString selectedSecret READ selectedSecret WRITE selectSecret NOTIFY stateChanged)
    Q_PROPERTY(QString revealedSecret READ revealedSecret NOTIFY stateChanged)
    Q_PROPERTY(QString generatedPassword READ generatedPassword NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY stateChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY stateChanged)
    Q_PROPERTY(int requestTimeout READ requestTimeout NOTIFY stateChanged)
    Q_PROPERTY(int autoLockTimeout READ autoLockTimeout NOTIFY stateChanged)
    Q_PROPERTY(int clipboardTimeout READ clipboardTimeout NOTIFY stateChanged)
    Q_PROPERTY(bool startLocked READ startLocked NOTIFY stateChanged)
    Q_PROPERTY(QString language READ language NOTIFY stateChanged)
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY stateChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(SecretsModel *secretsModel READ secretsModel CONSTANT)

  public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;
    [[nodiscard]] QString screen() const;
    [[nodiscard]] QString currentPage() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool serverHealthy() const;
    [[nodiscard]] bool vaultUnlocked() const;
    [[nodiscard]] QString email() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString notice() const;
    [[nodiscard]] QString selectedSecret() const;
    [[nodiscard]] QString revealedSecret() const;
    [[nodiscard]] QString generatedPassword() const;
    [[nodiscard]] QString diagnostics() const;
    [[nodiscard]] QString serverUrl() const;
    [[nodiscard]] int requestTimeout() const;
    [[nodiscard]] int autoLockTimeout() const;
    [[nodiscard]] int clipboardTimeout() const;
    [[nodiscard]] bool startLocked() const;
    [[nodiscard]] QString language() const;
    [[nodiscard]] QVariantList accounts() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] SecretsModel *secretsModel();

    Q_INVOKABLE void start();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void authenticate(QString email, QString password, bool registration);
    Q_INVOKABLE void initializeVault(QString password, QString confirmation, bool privateMetadata);
    Q_INVOKABLE void unlock(QString password);
    Q_INVOKABLE void lock();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void addAccount();
    Q_INVOKABLE void cancelAddAccount();
    Q_INVOKABLE void switchAccount(QString userId);
    Q_INVOKABLE void refreshSecrets();
    Q_INVOKABLE void addSecret(QString name, QString value);
    Q_INVOKABLE void updateSecret(QString name, QString value);
    Q_INVOKABLE void deleteSecret(QString name);
    Q_INVOKABLE void revealSecret(QString name);
    Q_INVOKABLE void copySecret(QString name);
    Q_INVOKABLE void generatePassword(int length, bool upper, bool lower, bool numbers, bool symbols);
    Q_INVOKABLE void copyGeneratedPassword();
    Q_INVOKABLE void exportBackup(QString path, QString password);
    Q_INVOKABLE void importBackup(QString path, QString password);
    Q_INVOKABLE void changeMasterPassword(QString current, QString next, QString confirmation);
    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void saveSettings(QString url, int requestSeconds, int autoLockSeconds,
                                  int clipboardSeconds, bool startLocked, QString language);
    Q_INVOKABLE void setLanguage(QString language);
    Q_INVOKABLE QString text(QString key, QString language) const;
    Q_INVOKABLE void clearMessage();
    Q_INVOKABLE void hideSensitive();
    void setCurrentPage(const QString &page);
    void selectSecret(const QString &name);

  signals:
    void stateChanged();

  private:
    using Completion = std::function<void(const QVariantMap &)>;
    void dispatch(std::function<QVariantMap()> operation, Completion completion = {});
    void applyStartup(const QVariantMap &result);
    void checkHealth();
    void syncSharedState();
    void clearSensitiveState();
    void setError(QString message);
    void setNotice(QString message);
    static QVariantMap startupSnapshot();
    static QVariantMap agentOperation(const std::string &operation, const std::string &name = {},
                                      const std::string &value = {});

    SecretsModel model_;
    QTimer revealTimer_;
    QTimer clipboardTimer_;
    QTimer healthTimer_;
    QTimer syncTimer_;
    QString screen_{"loading"};
    QString page_{"secrets"};
    QString email_;
    QString error_;
    QString notice_;
    QString selected_;
    QString revealed_;
    QString generated_;
    QString copied_;
    QString diagnostics_;
    QString serverUrl_;
    QString language_{"en"};
    QString userId_;
    QString revision_;
    QVariantList accounts_;
    int requestTimeout_{15};
    int autoLockTimeout_{900};
    int clipboardTimeout_{30};
    bool startLocked_{false};
    bool busy_{false};
    bool healthy_{false};
    bool unlocked_{false};
    bool shuttingDown_{false};
    bool syncRunning_{false};
    bool healthRunning_{false};
    bool addingAccount_{false};
    quint64 stateGeneration_{0};
};
} // namespace nox::gui
