#include "web/script_bundle.h"

#include "web/logging.h"

#include <QFile>
#include <QJsonDocument>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace pldl::web {

namespace {
const QString kBootstrapName = u"bootstrap"_s;
}

ScriptBundle::ScriptBundle(QWebEngineProfile& profile)
    : m_profile(profile)
{}

QString ScriptBundle::scriptName(const QString& name)
{
    return u"pldl:"_s + name;
}

QString ScriptBundle::readResource(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(lcWeb) << "missing script resource" << resourcePath;
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool ScriptBundle::isDisabled(const QString& name)
{
    // Diagnostic escape hatch: PLDL_DISABLE_WEB_SCRIPTS turns page tweaks off to
    // isolate an interfering feature. "1"/"all" drops every tweak; otherwise it is
    // a comma-separated list of script names to drop (e.g. "adblock").
    const QString disable = qEnvironmentVariable("PLDL_DISABLE_WEB_SCRIPTS").trimmed();
    if (disable == u"1"_s || disable.compare(u"all"_s, Qt::CaseInsensitive) == 0) {
        return true;
    }
    for (const QString& entry : disable.split(u',', Qt::SkipEmptyParts)) {
        if (entry.trimmed() == name) {
            return true;
        }
    }
    return false;
}

QStringList ScriptBundle::bootstrapResources()
{
    // Order matters: hooks.js provides the JSON/fetch/XHR modifier registry
    // every other script registers into (LESSONS V3); adblock.js registers
    // the response and cosmetic layers (FEATURES B2).
    static const QStringList features{u":/scripts/hooks.js"_s, u":/scripts/adblock.js"_s};
    QStringList resources{u":/qtwebchannel/qwebchannel.js"_s, u":/scripts/bootstrap.js"_s};
    for (const QString& res : features) {
        const QString name = res.mid(res.lastIndexOf(u'/') + 1).chopped(3); // basename, no ".js"
        if (!isDisabled(name)) {
            resources << res;
        }
    }
    return resources;
}

void ScriptBundle::installBootstrap(const QJsonObject& config)
{
    // Config travels as JSON, never string-concatenated user text.
    QString source = u"window.__redConfig = "_s +
                     QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact)) + u";\n"_s;
    for (const QString& resource : bootstrapResources()) {
        source += readResource(resource);
        source += u"\n"_s;
    }
    installSource(kBootstrapName, source, QWebEngineScript::DocumentCreation);
}

void ScriptBundle::installResource(const QString& name, const QString& resourcePath,
                                   QWebEngineScript::InjectionPoint point)
{
    installSource(name, readResource(resourcePath), point);
}

void ScriptBundle::installSource(const QString& name, const QString& source,
                                 QWebEngineScript::InjectionPoint point)
{
    remove(name);
    if (source.isEmpty()) {
        return;
    }
    QWebEngineScript script;
    script.setName(scriptName(name));
    script.setSourceCode(source);
    script.setInjectionPoint(point);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    m_profile.scripts()->insert(script);
    qCDebug(lcWeb) << "installed script" << script.name() << "at" << point;
}

void ScriptBundle::remove(const QString& name)
{
    const QList<QWebEngineScript> existing = m_profile.scripts()->find(scriptName(name));
    for (const QWebEngineScript& s : existing) {
        m_profile.scripts()->remove(s);
    }
}

bool ScriptBundle::isInstalled(const QString& name) const
{
    return !m_profile.scripts()->find(scriptName(name)).isEmpty();
}

} // namespace pldl::web
