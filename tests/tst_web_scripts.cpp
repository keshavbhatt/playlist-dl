// The injected bundle: every resource exists, parses as JavaScript, carries
// the script contract's header, and the profile-level installation works
// (ADR-006).

#include "web/script_bundle.h"

#include <QApplication>
#include <QJSEngine>
#include <QTest>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>

using namespace Qt::StringLiterals;
using pldl::web::ScriptBundle;

class TestWebScripts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyResourceExistsAndParses()
    {
        const QStringList all = ScriptBundle::bootstrapResources();
        QVERIFY(all.size() >= 4);
        QCOMPARE(all.first(), u":/qtwebchannel/qwebchannel.js"_s);
        QCOMPARE(all.at(1), u":/scripts/bootstrap.js"_s);
        QCOMPARE(all.at(2), u":/scripts/hooks.js"_s);
        QVERIFY(all.contains(u":/scripts/adblock.js"_s));
        QJSEngine engine;
        for (const QString& resource : all + QStringList{u":/scripts/page-media.js"_s}) {
            const QString source = ScriptBundle::readResource(resource);
            QVERIFY2(!source.isEmpty(), qPrintable(resource));
            const QJSValue result =
                engine.evaluate(u"(function(){ 'use strict'; "_s + source + u"\n})"_s, resource);
            QVERIFY2(!result.isError(), qPrintable(resource + u": "_s + result.toString()));
        }
    }

    void headerBlocksArePresent()
    {
        for (const QString& resource : ScriptBundle::bootstrapResources() + QStringList{u":/scripts/page-media.js"_s}) {
            if (resource.contains(u"qwebchannel"_s)) {
                continue;
            }
            const QString source = ScriptBundle::readResource(resource);
            QVERIFY2(source.startsWith(u"// name:"_s), qPrintable(resource));
            QVERIFY2(source.contains(u"// on-fail:"_s), qPrintable(resource));
        }
    }

    void installsOnTheProfileCollection()
    {
        QWebEngineProfile profile; // off-the-record: nothing touches disk
        ScriptBundle bundle(profile);
        bundle.installBootstrap({{u"adblock"_s, true}});
        QVERIFY(bundle.isInstalled(u"bootstrap"_s));
        QVERIFY(!profile.scripts()->find(ScriptBundle::scriptName(u"bootstrap"_s)).isEmpty());
        bundle.installResource(u"page-media"_s, u":/scripts/page-media.js"_s);
        QVERIFY(bundle.isInstalled(u"page-media"_s));
        bundle.remove(u"page-media"_s);
        QVERIFY(!bundle.isInstalled(u"page-media"_s));
        // Re-installing replaces rather than stacking.
        bundle.installBootstrap({{u"adblock"_s, false}});
        QCOMPARE(profile.scripts()->find(ScriptBundle::scriptName(u"bootstrap"_s)).size(), 1);
    }

    void disableSwitchReadsTheEnvironment()
    {
        qunsetenv("PLDL_DISABLE_WEB_SCRIPTS");
        QVERIFY(!ScriptBundle::isDisabled(u"adblock"_s));
        qputenv("PLDL_DISABLE_WEB_SCRIPTS", "adblock,page-media");
        QVERIFY(ScriptBundle::isDisabled(u"adblock"_s));
        QVERIFY(ScriptBundle::isDisabled(u"page-media"_s));
        QVERIFY(!ScriptBundle::isDisabled(u"hooks"_s));
        qputenv("PLDL_DISABLE_WEB_SCRIPTS", "all");
        QVERIFY(ScriptBundle::isDisabled(u"hooks"_s));
        qunsetenv("PLDL_DISABLE_WEB_SCRIPTS");
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    TestWebScripts test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_web_scripts.moc"
