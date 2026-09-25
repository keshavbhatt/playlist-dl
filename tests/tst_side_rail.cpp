// The rail expands to show its labels and collapses back, animated, and
// remembers nothing itself (the window keeps the setting).

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/side_rail.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;
using pldl::ui::SideRail;

class TestSideRail : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void expandsAndCollapses()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        pldl::core::Settings settings(dir.filePath(u"rail.ini"_s));
        pldl::core::ThemeService theme(settings);
        QWidget owner;
        pldl::ui::Actions actions(&owner);
        SideRail rail(actions, theme, &owner);
        owner.show();
        QVERIFY(QTest::qWaitForWindowExposed(&owner));
        QCOMPARE(rail.width(), SideRail::kCollapsedWidth);
        QVERIFY(!rail.isExpanded());
        auto* search = rail.findChild<QToolButton*>(u"railSearch"_s);
        QVERIFY(search != nullptr);
        QCOMPARE(search->width(), 40);

        QSignalSpy changed(&rail, &SideRail::expandedChanged);
        rail.setExpanded(true, false); // start-up: no animation
        QCOMPARE(rail.width(), SideRail::kExpandedWidth);
        QCOMPARE(search->width(), SideRail::kExpandedWidth - 16);
        QVERIFY(actions.railLabels->isChecked());
        QCOMPARE(actions.railLabels->text(), u"Hide labels"_s);
        QCOMPARE(changed.count(), 1);

        rail.setExpanded(false); // animated: ends at the collapsed width within the duration
        QVERIFY(rail.isAnimating());
        QTRY_VERIFY_WITH_TIMEOUT(!rail.isAnimating(), SideRail::kAnimationMs * 5);
        QCOMPARE(rail.width(), SideRail::kCollapsedWidth);
        QCOMPARE(search->width(), 40);
        QCOMPARE(actions.railLabels->text(), u"Show labels"_s);
        QCOMPARE(changed.count(), 2);

        // The toggle button carries the action; a click expands again.
        auto* toggle = rail.findChild<QToolButton*>(u"railToggle"_s);
        QVERIFY(toggle != nullptr);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QVERIFY(actions.railLabels->isChecked());
    }
};

QTEST_MAIN(TestSideRail)
#include "tst_side_rail.moc"
