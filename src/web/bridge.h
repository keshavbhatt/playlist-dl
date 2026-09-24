#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace pldl::web {

/// The single QWebChannel object exposed to injected scripts as `bridge`
/// (ADR-006/007). JS → C++ are the public slots; C++ → JS are the signals.
/// Keep the surface tiny and documented in scripts/README.md.
class Bridge : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Bridge)

public:
    explicit Bridge(QObject* parent = nullptr);
    ~Bridge() override = default;

public Q_SLOTS:
    /// Called by scripts when their try/catch fires.
    void scriptFailed(const QString& name, const QString& message);
    void log(const QString& message);
    /// media-session.js: what is playing (see core::MediaState::fromJson).
    void mediaState(const QJsonObject& state);
    /// The custom error page's "Try again" was clicked.
    void retry();
    /// A script asked to download the given URL (the page's download button).
    void downloadRequested(const QString& url);
    /// page-media.js: what the page is playing (FEATURES B4). Keys: "kind"
    /// ("video", "audio" or "" for nothing), "height" (int, 0 unknown),
    /// "direct" (an http(s) media URL when the element has one, else "").
    void pageMedia(const QJsonObject& state);

Q_SIGNALS:
    // JS → C++ (re-emitted for C++ consumers)
    void scriptFailure(const QString& name, const QString& message);
    void mediaStateChanged(const QJsonObject& state);
    void retryRequested();
    void downloadUrlRequested(const QString& url);
    void pageMediaChanged(const QJsonObject& state);

    // C++ → JS (consumed by scripts through the channel)
    /// "play", "pause", "playPause", "next", "previous", "seek" (arg = seconds),
    /// "stop", "mute", "volume" (arg = 0..1).
    void mediaCommand(const QString& command, double argument);
    /// The live config object (same shape as window.__red.config).
    void configChanged(const QJsonObject& config);
};

} // namespace pldl::web
