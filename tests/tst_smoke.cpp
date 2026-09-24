// Smoke test: the whole widget stack constructs, shows every page, and tears
// down offscreen without crashing. Runs under QT_QPA_PLATFORM=offscreen.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mainWindowConstructsAndShows()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        pldl::core::Settings settings(dir.filePath(u"smoke.ini"_s));
        pldl::core::ThemeService theme(settings);

        pldl::ui::MainWindow window(settings, theme, u"3.0.0-test"_s);
        window.start();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(300);

        using PageId = pldl::ui::MainWindow::PageId;
        for (PageId page : {PageId::Playlist, PageId::Browser, PageId::Downloads, PageId::Search}) {
            window.showPage(page);
            QTest::qWait(50);
        }
        settings.setTheme(pldl::core::Theme::Dark);
        settings.setTheme(pldl::core::Theme::Light);
        settings.setBlockAds(false);
        settings.setBlockAds(true);
        window.openUrl(u"https://example.com/video"_s);
        QTest::qWait(100);
        QCOMPARE(window.findChild<QStackedWidget*>()->currentIndex(), static_cast<int>(PageId::Browser));

        // The sheets the debug hook opens are their own top-level windows:
        // open them, then close them again.
        for (const QString& what : {u"about"_s, u"shortcuts"_s, u"whatsnew"_s, u"bug"_s}) {
            window.debugOpen(what);
            QTest::qWait(100);
        }
        int sheets = 0;
        for (QWidget* top : QApplication::topLevelWidgets()) {
            if (top != &window && top->isWindow() && top->isVisible() && !top->windowTitle().isEmpty()) {
                ++sheets;
                top->close();
            }
        }
        QVERIFY2(sheets >= 3, qPrintable(u"only %1 sheets opened"_s.arg(sheets)));
        QTest::qWait(100);

        // Every page fits at 1200 px beside the rail.
        for (PageId page : {PageId::Search, PageId::Playlist, PageId::Browser, PageId::Downloads}) {
            window.showPage(page);
            QTest::qWait(30);
            QWidget* current = window.findChild<QStackedWidget*>()->currentWidget();
            const int minimum = current->minimumSizeHint().width();
            QVERIFY2(minimum <= 1144, qPrintable(u"page %1 needs %2 px"_s.arg(static_cast<int>(page)).arg(minimum)));
        }
        window.showAndRaise();
        window.close();
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-smoke"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    QApplication::setQuitOnLastWindowClosed(false);
    TestSmoke test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_smoke.moc"
