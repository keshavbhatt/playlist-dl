// The What's new sheet (FEATURES S9): opens on the running version, the
// Version picker appears with a second release and swaps the notes.

#include "ui/whats_new_dialog.h"

#include <QComboBox>
#include <QLabel>
#include <QTest>
#include <QTextBrowser>

using namespace Qt::StringLiterals;
using pldl::ui::WhatsNewDialog;

namespace {
const QString kTwo = u"# Changelog\n\n## [3.1.0] - 2026-11-01\n\nNewer.\n\n### Added\n- later thing\n\n"
                     "## [3.0.0] - 2026-09-24\n\nFirst.\n\n### Added\n- first thing\n"_s;
const QString kOne = u"## [3.0.0] - 2026-09-24\n\nOnly.\n"_s;
} // namespace

class TestWhatsNewDialog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void opensOnTheRunningVersionWithThePicker()
    {
        WhatsNewDialog dialog(u"3.0.0"_s, kTwo);
        QCOMPARE(dialog.releaseCount(), 2);
        QCOMPARE(dialog.shownVersion(), u"3.0.0"_s);
        auto* picker = dialog.findChild<QComboBox*>(u"versionPicker"_s);
        QVERIFY(picker != nullptr);
        QVERIFY(dialog.findChild<QWidget*>(u"versionPickerRow"_s)->isVisibleTo(&dialog));
        QVERIFY(picker->currentText().contains(u"this version"_s));
        QCOMPARE(dialog.findChild<QLabel*>(u"whatsNewTitle"_s)->text(), u"What's new in 3.0.0"_s);
        QVERIFY(dialog.findChild<QTextBrowser*>(u"whatsNewNotes"_s)->toPlainText().contains(u"first thing"_s));
        dialog.showVersion(u"3.1.0"_s);
        QCOMPARE(dialog.shownVersion(), u"3.1.0"_s);
        QVERIFY(dialog.findChild<QTextBrowser*>(u"whatsNewNotes"_s)->toPlainText().contains(u"later thing"_s));
        QVERIFY(dialog.findChild<QLabel*>(u"whatsNewSubtitle"_s)->text().contains(u"3.0.0"_s));
        dialog.showVersion(u"9.9.9"_s); // unknown: ignored
        QCOMPARE(dialog.shownVersion(), u"3.1.0"_s);
    }

    void oneReleaseHidesThePicker()
    {
        WhatsNewDialog dialog(u"3.0.0"_s, kOne);
        QCOMPARE(dialog.releaseCount(), 1);
        QVERIFY(!dialog.findChild<QWidget*>(u"versionPickerRow"_s)->isVisibleTo(&dialog));
        QCOMPARE(dialog.shownVersion(), u"3.0.0"_s);
    }

    void aDevBuildAheadOfTheChangelogShowsTheNewest()
    {
        WhatsNewDialog dialog(u"3.2.0-dev"_s, kTwo);
        QCOMPARE(dialog.shownVersion(), u"3.1.0"_s);
        QVERIFY(dialog.findChild<QLabel*>(u"whatsNewSubtitle"_s)->text().contains(u"3.2.0-dev"_s));
    }
};

QTEST_MAIN(TestWhatsNewDialog)
#include "tst_whats_new_dialog.moc"
