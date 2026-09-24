// DESIGN.md section 5: every text-on-background token pair meets WCAG AA in
// both schemes (4.5:1 for text, 3:1 for glyphs and large text).
#include "ui/pldl_style.h"

#include <QTest>

using pldl::ui::Tokens;

class TestStyleContrast : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void contrastFormula()
    {
        QCOMPARE(qRound(Tokens::contrast(Qt::white, Qt::black) * 10), 210);
        QCOMPARE(qRound(Tokens::contrast(Qt::black, Qt::black) * 10), 10);
    }

    void textPairsMeetAA_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void textPairsMeetAA()
    {
        QFETCH(bool, dark);
        const Tokens t = Tokens::forScheme(dark);
        struct Pair
        {
            const char* name;
            QColor fg;
            QColor bg;
            double minimum;
        };
        const Pair pairs[] = {
            {"text on bg", t.text, t.bg, 4.5},
            {"text on panel", t.text, t.panel, 4.5},
            {"text on elevated", t.text, t.elevated, 4.5},
            {"text on hover", t.text, t.hover, 4.5},
            {"text on input", t.text, t.input, 4.5},
            {"text on accentSoft", t.text, t.accentSoft, 4.5},
            {"muted on bg", t.muted, t.bg, 4.5},
            {"muted on panel", t.muted, t.panel, 4.5},
            {"muted on hover", t.muted, t.hover, 4.5},
            {"muted on rail", t.muted, t.rail, 4.5}, // inactive browser tab titles
            {"muted on input", t.muted, t.input, 4.5},
            {"primary button text", Tokens::textOn(t.accentStrong), t.accentStrong, 4.5},
            {"chip text on accentSoft", t.accent, t.accentSoft, 4.5},
            {"link on panel", t.link, t.panel, 4.5},
            {"link on bg", t.link, t.bg, 4.5},
            {"success badge text", t.success, t.panel, 4.5},
            {"warning badge text", t.warning, t.panel, 4.5},
            {"danger badge text", t.danger, t.panel, 4.5},
            {"badge text on badge", t.badgeText, t.badge, 4.5},
            {"pro badge text", Tokens::textOn(t.warning), t.warning, 4.5},
            {"accent glyph on bg", t.accent, t.bg, 3.0},
            {"accent glyph on panel", t.accent, t.panel, 3.0},
            {"focus ring on bg", t.accent, t.bg, 3.0},
            {"focus ring on panel", t.accent, t.panel, 3.0},
            {"border on panel", t.muted, t.panel, 3.0},
            {"progress on hover track", t.accent, t.hover, 3.0},
        };
        for (const Pair& pair : pairs) {
            const double ratio = Tokens::contrast(pair.fg, pair.bg);
            QVERIFY2(ratio >= pair.minimum,
                     qPrintable(QString::fromLatin1("%1: %2 on %3 is %4, needs %5")
                                    .arg(QString::fromLatin1(pair.name), pair.fg.name(), pair.bg.name())
                                    .arg(ratio, 0, 'f', 2)
                                    .arg(pair.minimum)));
        }
    }

    void textOnPicksTheReadableColour()
    {
        QCOMPARE(Tokens::textOn(QColor(0xF9, 0xAB, 0x00)), QColor(0x14, 0x11, 0x18));
        QCOMPARE(Tokens::textOn(QColor(0x2F, 0x6F, 0xE4)), QColor(Qt::white));
    }
};

QTEST_GUILESS_MAIN(TestStyleContrast)
#include "tst_style_contrast.moc"
