#include "services/supported_sites.h"

#include "core/downloads/engine_spec.h"
#include "services/logging.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::services {

namespace {

QString& cacheDirectoryOverride()
{
    static QString directory;
    return directory;
}

/// Brand casing the engine's lowercase names lose; everything else gets a capital first letter.
QString brandCase(const QString& lower)
{
    static const QHash<QString, QString> kBrands = {
        {u"youtube"_s, u"YouTube"_s},         {u"soundcloud"_s, u"SoundCloud"_s}, {u"tiktok"_s, u"TikTok"_s},
        {u"bbc"_s, u"BBC"_s},                 {u"bbc.co.uk"_s, u"BBC"_s},         {u"vk"_s, u"VK"_s},
        {u"dailymotion"_s, u"Dailymotion"_s}, {u"bilibili"_s, u"Bilibili"_s},     {u"twitch"_s, u"Twitch"_s},
        {u"ard"_s, u"ARD"_s},                 {u"zdf"_s, u"ZDF"_s},               {u"cbc"_s, u"CBC"_s},
        {u"npr"_s, u"NPR"_s},                 {u"pbs"_s, u"PBS"_s},               {u"ted"_s, u"TED"_s},
        {u"imdb"_s, u"IMDb"_s},               {u"linkedin"_s, u"LinkedIn"_s},     {u"ok.ru"_s, u"OK.ru"_s},
    };
    if (kBrands.contains(lower)) {
        return kBrands.value(lower);
    }
    if (lower.isEmpty() || lower.contains(u'.')) {
        return lower; // a domain stays a domain
    }
    return lower.at(0).toUpper() + lower.mid(1);
}

} // namespace

SupportedSites::SupportedSites(QObject* parent)
    : QObject(parent)
{
}

SupportedSites::~SupportedSites()
{
    if (m_process != nullptr) {
        m_process->kill();
    }
}

void SupportedSites::setCacheDirectory(const QString& directory)
{
    cacheDirectoryOverride() = directory;
}

QString SupportedSites::cachePath(const QString& version)
{
    const QString directory = cacheDirectoryOverride().isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                  : cacheDirectoryOverride();
    QString safe = version.trimmed();
    safe.replace(QRegularExpression(uR"([^A-Za-z0-9._-])"_s), u"_"_s);
    return directory + u"/supported-sites-"_s + (safe.isEmpty() ? u"unknown"_s : safe) + u".txt"_s;
}

SupportedSite SupportedSites::entryFor(const QString& raw)
{
    SupportedSite entry;
    entry.raw = raw.trimmed();
    const qsizetype colon = entry.raw.indexOf(u':');
    const QString base = colon < 0 ? entry.raw : entry.raw.left(colon);
    const bool allLower = std::none_of(base.cbegin(), base.cend(), [](QChar c) { return c.isUpper(); });
    entry.site = allLower ? brandCase(base) : base;
    if (colon >= 0) {
        QString variant = entry.raw.mid(colon + 1);
        variant.replace(u':', u' ');
        variant.replace(u'_', u' ');
        entry.variant = variant.simplified();
    }
    return entry;
}

QList<SupportedSite> SupportedSites::parse(const QByteArray& output)
{
    QList<SupportedSite> sites;
    QSet<QString> seen;
    for (const QByteArray& rawLine : output.split('\n')) {
        QString line = QString::fromUtf8(rawLine).trimmed();
        // The engine lists one extractor per line; ones it marks broken are
        // left out. The generic scraper stays (owner, 2026-09-24): it is what
        // makes a site that is not listed work.
        if (line.isEmpty() || line.contains(u"(CURRENTLY BROKEN)"_s, Qt::CaseInsensitive)) {
            continue;
        }
        if (const qsizetype space = line.indexOf(u' '); space > 0) {
            line.truncate(space); // any other note after the name
        }
        const QString key = line.toLower();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        sites.append(entryFor(line));
    }
    std::sort(sites.begin(), sites.end(), [](const SupportedSite& a, const SupportedSite& b) {
        // Case-insensitive on the engine's name, so a site's variants follow it.
        const int order = a.raw.compare(b.raw, Qt::CaseInsensitive);
        return order != 0 ? order < 0 : a.raw < b.raw;
    });
    return sites;
}

QUrl SupportedSites::homePage(const QString& site)
{
    QString name = site.trimmed().toLower();
    if (name.isEmpty()) {
        return {};
    }
    if (!name.contains(u'.')) {
        name += u".com"_s;
    }
    return {u"https://"_s + name};
}

void SupportedSites::setSites(const QList<SupportedSite>& sites, const QString& version)
{
    m_sites = sites;
    m_version = version;
    m_loaded = true;
    Q_EMIT loaded();
}

int SupportedSites::siteCount() const
{
    QSet<QString> sites;
    for (const SupportedSite& entry : m_sites) {
        sites.insert(entry.site.toLower());
    }
    return static_cast<int>(sites.size());
}

QString SupportedSites::siteForHost(const QString& host) const
{
    QString h = host.trimmed().toLower();
    if (h.startsWith(u"www."_s)) {
        h.remove(0, 4);
    }
    if (h.isEmpty()) {
        return {};
    }
    QHash<QString, QString> names; // lowercase engine site name -> display
    for (const SupportedSite& entry : m_sites) {
        if (entry.variant.isEmpty()) {
            const qsizetype colon = entry.raw.indexOf(u':');
            names.insert((colon < 0 ? entry.raw : entry.raw.left(colon)).toLower(), entry.site);
        }
    }
    const QStringList labels = h.split(u'.', Qt::SkipEmptyParts);
    for (qsizetype i = 0; i + 1 < labels.size(); ++i) {
        const QString suffix = labels.mid(i).join(u'.');
        if (names.contains(suffix)) {
            return names.value(suffix);
        }
        if (labels.at(i).size() >= 3 && names.contains(labels.at(i))) {
            return names.value(labels.at(i));
        }
    }
    return {};
}

bool SupportedSites::isCacheable(const QString& version)
{
    // A system engine reports "system" whatever its release: its list is asked
    // afresh each run so an update through the package manager shows up.
    return !version.isEmpty() && version.compare(u"system"_s, Qt::CaseInsensitive) != 0;
}

void SupportedSites::load(const QString& enginePath, const QString& version)
{
    if (m_loaded && version == m_version && isCacheable(version)) {
        return; // the same engine again (a re-provision): nothing changed
    }
    if (qEnvironmentVariableIsSet("PLDL_DEBUG_SITES")) {
        QFile file(qEnvironmentVariable("PLDL_DEBUG_SITES"));
        if (file.open(QIODevice::ReadOnly)) {
            adopt(file.readAll(), version);
            return;
        }
    }
    QFile cache(cachePath(version));
    if (isCacheable(version) && cache.open(QIODevice::ReadOnly)) {
        const QByteArray output = cache.readAll();
        if (!output.trimmed().isEmpty()) {
            adopt(output, version);
            return;
        }
    }
    askEngine(enginePath, version);
}

void SupportedSites::pruneCaches(const QString& keepVersion)
{
    // After an engine update the old release's list is dead weight.
    const QFileInfo keep(cachePath(keepVersion));
    QDir directory(keep.absolutePath());
    const QFileInfoList files = directory.entryInfoList({u"supported-sites-*.txt"_s}, QDir::Files);
    for (const QFileInfo& file : files) {
        if (file.fileName() != keep.fileName()) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}

void SupportedSites::askEngine(const QString& enginePath, const QString& version)
{
    if (enginePath.isEmpty() || m_process != nullptr) {
        return;
    }
    m_process = new QProcess(this);
    m_process->setProcessEnvironment(core::engineProcessEnvironment());
    m_process->setProgram(enginePath);
    m_process->setArguments({u"--list-extractors"_s});
    connect(m_process, &QProcess::finished, this, [this, version](int code, QProcess::ExitStatus status) {
        QProcess* process = m_process;
        m_process = nullptr;
        process->deleteLater();
        const QByteArray output = process->readAllStandardOutput();
        if (status != QProcess::NormalExit || code != 0 || output.trimmed().isEmpty()) {
            qCWarning(lcServices) << "supported sites: the engine's list could not be read, exit" << code;
            return;
        }
        writeCache(output, version);
        adopt(output, version);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_process != nullptr && error == QProcess::FailedToStart) {
            qCWarning(lcServices) << "supported sites: the engine did not start";
            m_process->deleteLater();
            m_process = nullptr;
        }
    });
    m_process->start();
}

void SupportedSites::writeCache(const QByteArray& output, const QString& version)
{
    if (!isCacheable(version)) {
        return;
    }
    QDir().mkpath(QFileInfo(cachePath(version)).absolutePath());
    QFile file(cachePath(version));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(output);
    }
    pruneCaches(version);
}

void SupportedSites::adopt(const QByteArray& output, const QString& version)
{
    setSites(parse(output), version);
}

} // namespace pldl::services
