// The Settings dialog (DESIGN.md section 3, FEATURES G1) offscreen: six
// pages build, the navigation names them, controls write straight to
// Settings and follow changes made elsewhere, Reset restores the defaults,
// the Engine card describes every manager state, and the geometry round
// trips through the settings key.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/engine_manager.h"
#include "ui/settings_dialog.h"
#include "web/request_interceptor.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;
using pldl::ui::SettingsDialog;
using Status = pldl::services::EngineManager::Status;
using State = pldl::services::EngineManager::State;

class TestSettingsDialog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startPageChoiceDrivesTheSetting()
    {
        auto dialog = makeDialog(true);
        auto* choice = dialog->findChild<QComboBox*>(u"startPageCombo"_s);
        auto* address = dialog->findChild<QLineEdit*>(u"startPageEdit"_s);
        QVERIFY(choice != nullptr && address != nullptr);
        dialog->showPage(pldl::ui::SettingsDialog::Browser); // only the shown page counts as visible
        QCOMPARE(choice->currentData().toInt(), 0); // Empty tab is the default
        QVERIFY(!address->isVisibleTo(dialog.get()));
        choice->setCurrentIndex(choice->findData(1));
        QCOMPARE(m_settings->browserStartPage(), QString(pldl::core::Settings::kYouTubeStartPage));
        choice->setCurrentIndex(choice->findData(2));
        QVERIFY(address->isVisibleTo(dialog.get()));
        address->setText(u"https://example.com/start"_s);
        Q_EMIT address->editingFinished(); // the field stores when editing ends, not per keystroke
        QCOMPARE(m_settings->browserStartPage(), u"https://example.com/start"_s);
        choice->setCurrentIndex(choice->findData(0));
        QCOMPARE(m_settings->browserStartPage(), QString(pldl::core::Settings::kEmptyStartPage));
        QVERIFY(!address->isVisibleTo(dialog.get()));
        // A change from elsewhere moves the choice.
        m_settings->setBrowserStartPage(QString(pldl::core::Settings::kYouTubeStartPage));
        QCOMPARE(choice->currentData().toInt(), 1);
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<pldl::core::Settings>(m_dir->filePath(u"settings.ini"_s));
        m_theme = std::make_unique<pldl::core::ThemeService>(*m_settings);
        m_engine = std::make_unique<pldl::services::EngineManager>(*m_settings);
        m_interceptor = std::make_unique<pldl::web::RequestInterceptor>();
    }

    void cleanup()
    {
        m_interceptor.reset();
        m_engine.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void everyPageBuildsAndTheNavNamesThem()
    {
        auto dialog = makeDialog(true);
        for (int page = 0; page < SettingsDialog::kPageCount; ++page) {
            dialog->showPage(page);
            QTest::qWait(20);
            QCOMPARE(dialog->currentPage(), page);
        }
        auto* nav = dialog->findChild<QListWidget*>(u"settingsNav"_s);
        QVERIFY(nav != nullptr);
        QCOMPARE(nav->count(), 5); // Appearance lives on General (review 2026-09-25)
        const QStringList titles{u"General"_s, u"Downloads"_s, u"Browser"_s, u"Search"_s, u"Advanced"_s};
        for (int i = 0; i < titles.size(); ++i) {
            QCOMPARE(nav->item(i)->text(), titles.at(i));
            QVERIFY(!nav->item(i)->icon().isNull());
        }
        dialog->showPage(u"advanced"_s);
        QCOMPARE(dialog->currentPage(), static_cast<int>(SettingsDialog::Advanced));
        dialog->showPage(u"nonsense"_s);
        QCOMPARE(dialog->currentPage(), static_cast<int>(SettingsDialog::General));
        QCOMPARE(SettingsDialog::pageIndex(u"Search"_s), static_cast<int>(SettingsDialog::Search));
        // Every control has a name for assistive technology.
        for (QWidget* control : dialog->findChildren<QWidget*>()) {
            if (qobject_cast<QComboBox*>(control) != nullptr || qobject_cast<QSpinBox*>(control) != nullptr ||
                qobject_cast<QCheckBox*>(control) != nullptr) {
                QVERIFY2(!control->accessibleName().isEmpty(), qPrintable(control->objectName()));
            }
        }
        // No user-facing text names the engines or accounts.
        for (QLabel* label : dialog->findChildren<QLabel*>()) {
            const QString text = label->text().toLower();
            QVERIFY2(!text.contains(u"ktechpit"_s) && !text.contains(u"account"_s) &&
                         !text.contains(u"plan"_s),
                     qPrintable(label->text()));
        }
        dialog->close();
    }

    void controlsWriteToSettings()
    {
        auto dialog = makeDialog(true);
        auto* theme = dialog->findChild<QComboBox*>(u"themeCombo"_s);
        auto* concurrent = dialog->findChild<QSpinBox*>(u"concurrentSpin"_s);
        auto* perPage = dialog->findChild<QSpinBox*>(u"resultsPerPageSpin"_s);
        QVERIFY(theme != nullptr && concurrent != nullptr && perPage != nullptr);
        QCOMPARE(theme->currentData().toInt(), static_cast<int>(pldl::core::Theme::System));
        theme->setCurrentIndex(theme->findData(static_cast<int>(pldl::core::Theme::Dark)));
        QCOMPARE(m_settings->theme(), pldl::core::Theme::Dark);
        concurrent->setValue(4);
        QCOMPARE(m_settings->concurrentDownloads(), 4);
        perPage->setValue(30);
        QCOMPARE(m_settings->searchResultsPerPage(), 30);
        auto* suggest = dialog->findChild<QCheckBox*>(u"suggestCheck"_s);
        QVERIFY(suggest != nullptr && suggest->isChecked()); // on by default
        suggest->setChecked(false);
        QVERIFY(!m_settings->searchSuggestions());

        // A custom browser identity: the field appears and stores its text.
        dialog->showPage(SettingsDialog::Browser);
        auto* identity = dialog->findChild<QComboBox*>(u"identityCombo"_s);
        QVERIFY(identity != nullptr);
        QVERIFY(identity->count() >= 3);
        QCOMPARE(identity->currentData().toString(), u"default"_s);
        identity->setCurrentIndex(identity->count() - 1);
        QCOMPARE(m_settings->browserUserAgentPreset(), u"custom"_s);
        auto* identityText = dialog->findChild<QLineEdit*>(u"identityField"_s);
        QVERIFY(identityText != nullptr);
        QVERIFY(identityText->isVisible());
        identityText->setText(u"Mine/1.0"_s);
        Q_EMIT identityText->editingFinished();
        QCOMPARE(m_settings->browserUserAgent(), u"Mine/1.0"_s);
        dialog->close();
    }

    void controlsFollowSettingsChangedElsewhere()
    {
        auto dialog = makeDialog(true);
        auto* theme = dialog->findChild<QComboBox*>(u"themeCombo"_s);
        auto* concurrent = dialog->findChild<QSpinBox*>(u"concurrentSpin"_s);
        auto* perPage = dialog->findChild<QSpinBox*>(u"resultsPerPageSpin"_s);
        QSignalSpy themeChanges(m_settings.get(), &pldl::core::Settings::themeChanged);
        m_settings->setTheme(pldl::core::Theme::Light);
        m_settings->setConcurrentDownloads(5);
        m_settings->setSearchResultsPerPage(40);
        m_settings->setBlockAds(false);
        QCOMPARE(theme->currentData().toInt(), static_cast<int>(pldl::core::Theme::Light));
        QCOMPARE(concurrent->value(), 5);
        QCOMPARE(perPage->value(), 40);
        QCOMPARE(themeChanges.count(), 1); // loading never echoes back into the settings
        auto* blocked = dialog->findChild<QLabel*>(u"blockedCount"_s);
        QVERIFY(blocked != nullptr);
        QVERIFY(!blocked->text().contains(u"blocked this session"_s));
        m_settings->setBlockAds(true);
        QVERIFY(blocked->text().contains(u"0 requests blocked this session"_s));
        dialog->close();
    }

    void resetRestoresDefaultsAfterConfirmation()
    {
        auto dialog = makeDialog(true);
        m_settings->setTheme(pldl::core::Theme::Dark);
        m_settings->setConcurrentDownloads(5);
        dialog->showPage(SettingsDialog::Advanced);
        auto* reset = dialog->findChild<QPushButton*>(u"resetSettings"_s);
        QVERIFY(reset != nullptr);
        QTimer::singleShot(50, dialog.get(), [dialog = dialog.get()] {
            for (QDialog* sheet : dialog->findChildren<QDialog*>()) {
                if (!sheet->isVisible()) {
                    continue;
                }
                for (QPushButton* button : sheet->findChildren<QPushButton*>()) {
                    if (button->text() == u"Reset"_s) {
                        button->click();
                    }
                }
            }
        });
        reset->click();
        QTRY_COMPARE(m_settings->theme(), pldl::core::Theme::System);
        QCOMPARE(m_settings->concurrentDownloads(), 2);
        QCOMPARE(dialog->findChild<QSpinBox*>(u"concurrentSpin"_s)->value(), 2);
        QCOMPARE(dialog->findChild<QComboBox*>(u"themeCombo"_s)->currentData().toInt(),
                 static_cast<int>(pldl::core::Theme::System));
        dialog->close();
    }

    void closeToTrayIsOfferedOnlyWithATray()
    {
        auto withTray = makeDialog(true);
        auto without = makeDialog(false);
        QComboBox* closeWith = nullptr;
        QComboBox* closeWithout = nullptr;
        for (QComboBox* combo : withTray->findChildren<QComboBox*>()) {
            if (combo->accessibleName() == u"When closing"_s) {
                closeWith = combo;
            }
        }
        for (QComboBox* combo : without->findChildren<QComboBox*>()) {
            if (combo->accessibleName() == u"When closing"_s) {
                closeWithout = combo;
            }
        }
        QVERIFY(closeWith != nullptr && closeWithout != nullptr);
        QVERIFY(closeWith->isEnabled());
        QVERIFY(!closeWithout->isEnabled());
        withTray->close();
        without->close();
    }

    void engineCardTextPerState()
    {
        Status s;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Not installed"_s);
        s.state = State::Installing;
        s.stepLabel = u"Downloading…"_s;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Setting up"_s);
        QCOMPARE(SettingsDialog::engineStatusDetail(s), u"Downloading…"_s);
        s.state = State::NotInstalled;
        s.ytdlpPath = u"/x/yt-dlp"_s;
        s.jsRuntime = u"deno"_s;
        s.ffmpegMissing = true;
        s.error = u"Install it with: sudo apt install ffmpeg"_s;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Media converter missing"_s);
        QCOMPARE(SettingsDialog::engineStatusDetail(s), s.error);
        s.ffmpegMissing = false;
        s.error.clear();
        s.state = State::Ready;
        s.ytdlpVersion = u"2026.09.01"_s;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Download engine 2026.09.01"_s);
        s.updateAvailable = true;
        s.latestVersion = u"2026.09.20"_s;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Update available (2026.09.20)"_s);
        s.updateAvailable = false;
        s.systemYtdlp = true;
        QCOMPARE(SettingsDialog::engineStatusText(s), u"Download engine from this system"_s);
        s.systemYtdlp = false;
        s.checkingForUpdates = true;
        QVERIFY(SettingsDialog::engineStatusDetail(s).startsWith(u"Checking"_s));
        s.checkingForUpdates = false;
        s.checkError = u"offline"_s;
        QVERIFY(SettingsDialog::engineStatusDetail(s).contains(u"offline"_s));

        // The card in the dialog shows the manager's state (Unknown before initialize()).
        auto dialog = makeDialog(true);
        auto* status = dialog->findChild<QLabel*>(u"engineStatus"_s);
        QVERIFY(status != nullptr);
        QCOMPARE(status->text(), u"Not installed"_s);
        QPushButton* setup = nullptr;
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == u"Set up"_s) {
                setup = button;
            }
        }
        QVERIFY(setup != nullptr);
        QSignalSpy setupRequests(dialog.get(), &SettingsDialog::engineSetupRequested);
        setup->click();
        QCOMPARE(setupRequests.count(), 1);
        dialog->close();
    }

    void geometryRoundTrips()
    {
        QVERIFY(m_settings->settingsDialogGeometry().isEmpty());
        {
            auto dialog = makeDialog(true);
            dialog->resize(760, 520); // inside the offscreen screen, so the restore is not clamped
            QTest::qWait(30);
            dialog->close();
        }
        QVERIFY(!m_settings->settingsDialogGeometry().isEmpty());
        auto again = makeDialog(true);
        QCOMPARE(again->size(), QSize(760, 520));
        again->close();
    }

private:
    std::unique_ptr<SettingsDialog> makeDialog(bool trayAvailable)
    {
        auto dialog =
            std::make_unique<SettingsDialog>(*m_settings, *m_theme, *m_engine, *m_interceptor, trayAvailable);
        dialog->show();
        if (!QTest::qWaitForWindowExposed(dialog.get())) {
            return nullptr;
        }
        return dialog;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
    std::unique_ptr<pldl::services::EngineManager> m_engine;
    std::unique_ptr<pldl::web::RequestInterceptor> m_interceptor;
};

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-settings-dialog"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestSettingsDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_settings_dialog.moc"
