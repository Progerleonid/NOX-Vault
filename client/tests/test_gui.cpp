#include "nox/gui/password_generator.hpp"
#include "nox/gui/app_controller.hpp"
#include "nox/gui/secrets_model.hpp"
#include "nox/errors.hpp"
#include <QSignalSpy>
#include <QtTest>

using nox::gui::SecretsModel;

class GuiLogicTest final : public QObject {
    Q_OBJECT
  private slots:
    void filtersSecretsCaseInsensitively() {
        SecretsModel model;
        model.setRecords({QVariantMap{{"name", "GITHUB_TOKEN"}, {"updatedAt", "2026-08-14T10:00:00Z"}},
                          QVariantMap{{"name", "DATABASE_URL"}, {"updatedAt", "2026-08-13T10:00:00Z"}},
                          QVariantMap{{"name", "openai_api_key"}, {"updatedAt", "2026-08-12T10:00:00Z"}}});
        QCOMPARE(model.rowCount(), 3);
        model.setFilter("api");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), SecretsModel::NameRole).toString(), QString("openai_api_key"));
        QCOMPARE(model.data(model.index(0), SecretsModel::UpdatedAtRole).toString(), QString("2026-08-12T10:00:00Z"));
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void generatorHonorsEnabledClasses() {
        const auto value = nox::gui::generate_password(64, true, true, true, true);
        QCOMPARE(value.size(), 64);
        QVERIFY(value.contains(QRegularExpression("[A-Z]")));
        QVERIFY(value.contains(QRegularExpression("[a-z]")));
        QVERIFY(value.contains(QRegularExpression("[0-9]")));
        QVERIFY(value.contains(QRegularExpression("[^A-Za-z0-9]")));
        const auto digits = nox::gui::generate_password(32, false, false, true, false);
        QVERIFY(digits.contains(QRegularExpression("^[0-9]+$")));
    }

    void generatorRejectsUnsafeInputs() {
        QVERIFY_EXCEPTION_THROWN(nox::gui::generate_password(7, true, true, true, true), nox::NoxError);
        QVERIFY_EXCEPTION_THROWN(nox::gui::generate_password(32, false, false, false, false), nox::NoxError);
    }

    void translationsUseTheRequestedLanguage() {
        nox::gui::AppController controller;
        QCOMPARE(controller.text("settings", "en"), QString("Settings"));
        QCOMPARE(controller.text("settings", "ru"), QString::fromUtf8("Настройки"));
        QCOMPARE(controller.text("language", "de"), QString("Sprache"));
        QCOMPARE(controller.text("saveSettings", "pl"), QString::fromUtf8("Zapisz ustawienia"));
        QCOMPARE(controller.text("addAccount", "cs"), QString::fromUtf8("Přidat účet"));
    }
};

QTEST_APPLESS_MAIN(GuiLogicTest)
#include "test_gui.moc"
