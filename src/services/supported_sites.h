#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class QProcess;

namespace pldl::services {

/// One entry of the download engine's site list.
struct SupportedSite
{
    QString raw;     ///< the engine's own name, "vimeo:album"
    QString site;    ///< the site part, shown once per site: "Vimeo"
    QString variant; ///< the part after the colon, spaced: "album", empty for the site itself
    /// "Vimeo: album", or the site alone.
    [[nodiscard]] QString display() const { return variant.isEmpty() ? site : site + u": " + variant; }
};

/// The supported sites list (FEATURES S13): the download engine's extractor
/// names, read once per engine version and cached as a file, grouped by
/// site for the sheet. Entries the engine marks broken are dropped.
class SupportedSites : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SupportedSites)

public:
    explicit SupportedSites(QObject* parent = nullptr);
    ~SupportedSites() override;

    /// Reads the cache for `version` or asks the engine at `enginePath`
    /// (`--list-extractors`) and caches its answer, dropping older releases'
    /// caches. Emits loaded() each time a list arrives, so a call after an
    /// engine update (the manager's ready() fires again) refreshes an open sheet; the same version again
    /// is a no-op. A system engine ("system") is asked every run and never
    /// cached. `PLDL_DEBUG_SITES=<path>` reads that file instead (grabs).
    void load(const QString& enginePath, const QString& version);
    /// Feeds a list directly (tests).
    void setSites(const QList<SupportedSite>& sites, const QString& version);
    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] const QList<SupportedSite>& sites() const { return m_sites; }
    /// Distinct sites, not counting the variants.
    [[nodiscard]] int siteCount() const;
    [[nodiscard]] QString version() const { return m_version; }

    /// The site (display name) whose name matches `host`, or empty. Labels
    /// are tried longest first ("player.vimeo.com", "vimeo.com", "vimeo"),
    /// never the last label alone, so ".com" matches nothing.
    [[nodiscard]] QString siteForHost(const QString& host) const;

    /// The engine's `--list-extractors` output parsed and sorted (pure).
    [[nodiscard]] static QList<SupportedSite> parse(const QByteArray& output);
    /// "vimeo:album" split into its display parts (pure).
    [[nodiscard]] static SupportedSite entryFor(const QString& raw);
    /// A best guess at the site's home page for the browser: the name when
    /// it carries a domain ("abc.net.au"), "<name>.com" otherwise.
    [[nodiscard]] static QUrl homePage(const QString& site);

    /// Where the list for `version` is cached; the tests point it elsewhere.
    static void setCacheDirectory(const QString& directory);
    [[nodiscard]] static QString cachePath(const QString& version);
    [[nodiscard]] static bool isCacheable(const QString& version);
    /// Removes every cached list but `keepVersion`'s.
    static void pruneCaches(const QString& keepVersion);

Q_SIGNALS:
    void loaded();

private:
    void askEngine(const QString& enginePath, const QString& version);
    static void writeCache(const QByteArray& output, const QString& version);
    void adopt(const QByteArray& output, const QString& version);

    QList<SupportedSite> m_sites;
    QString m_version;
    bool m_loaded = false;
    QProcess* m_process = nullptr;
};

} // namespace pldl::services
