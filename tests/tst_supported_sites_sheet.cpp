// The Supported sites sheet: the filter narrows the rows and the count, Enter
// on a single match and a double click open the site.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/supported_sites.h"
#include "ui/supported_sites_sheet.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestSupportedSitesSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void filtersCountsAndOpens()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        pldl::core::Settings settings(dir.filePath(u"sites.ini"_s));
        pldl::core::ThemeService theme(settings);
        pldl::services::SupportedSites sites;
        sites.setSites(pldl::services::SupportedSites::parse("youtube\nyoutube:tab\nvimeo\nvimeo:album\nVimm\nVimm:stream\n"),
                       u"2026.08.19"_s);

        pldl::ui::SupportedSitesSheet sheet(sites, theme);
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        QCOMPARE(sheet.shownCount(), 6);
        QCOMPARE(sheet.findChild<QLabel*>(u"siteCount"_s)->text(), u"3 sites"_s);
        QVERIFY(sheet.findChild<QLabel*>(u"siteVersion"_s)->text().contains(u"2026.08.19"_s));
        QVERIFY(sheet.filterField()->hasFocus());

        sheet.setFilter(u"vim"_s);
        QCOMPARE(sheet.shownCount(), 4);
        QCOMPARE(sheet.findChild<QLabel*>(u"siteCount"_s)->text(), u"2 of 3 sites"_s);
        QCOMPARE(sheet.list()->item(0)->text(), u"Vimeo"_s);
        QCOMPARE(sheet.list()->item(1)->text(), u"Vimeo: album"_s);

        QSignalSpy opened(&sheet, &pldl::ui::SupportedSitesSheet::openSiteRequested);
        sheet.setFilter(u"vimeo: album"_s);
        QCOMPARE(sheet.shownCount(), 1);
        QTest::keyClick(sheet.filterField(), Qt::Key_Return); // one match: Enter opens it
        QCOMPARE(opened.count(), 1);
        QCOMPARE(opened.at(0).at(0).toUrl(), QUrl(u"https://vimeo.com"_s));

        sheet.setFilter(QString());
        QTest::keyClick(sheet.filterField(), Qt::Key_Down); // into the list
        QVERIFY(sheet.list()->hasFocus());
        QCOMPARE(sheet.list()->currentRow(), 0);
        QTest::keyClick(sheet.list(), Qt::Key_Return);
        QCOMPARE(opened.count(), 2);
        QCOMPARE(opened.at(1).at(0).toUrl(), QUrl(u"https://vimeo.com"_s)); // "vimeo" sorts first, case-insensitively

        // An engine update while the sheet is open: the rows and the version follow.
        sites.setSites(pldl::services::SupportedSites::parse("youtube\nnewsite\n"), u"2026.09.20"_s);
        QCOMPARE(sheet.shownCount(), 2);
        QVERIFY(sheet.findChild<QLabel*>(u"siteVersion"_s)->text().contains(u"2026.09.20"_s));
    }
};

QTEST_MAIN(TestSupportedSitesSheet)
#include "tst_supported_sites_sheet.moc"
