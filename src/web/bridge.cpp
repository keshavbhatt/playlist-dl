#include "web/bridge.h"

#include "web/logging.h"

namespace pldl::web {

Bridge::Bridge(QObject* parent)
    : QObject(parent)
{}

void Bridge::scriptFailed(const QString& name, const QString& message)
{
    qCWarning(lcWebJs).noquote() << "script" << name << "failed:" << message;
    Q_EMIT scriptFailure(name, message);
}

void Bridge::log(const QString& message)
{
    qCDebug(lcWebJs).noquote() << "script:" << message;
}

void Bridge::mediaState(const QJsonObject& state)
{
    Q_EMIT mediaStateChanged(state);
}

void Bridge::retry()
{
    qCInfo(lcWebJs) << "error page: retry requested";
    Q_EMIT retryRequested();
}

void Bridge::downloadRequested(const QString& url)
{
    qCInfo(lcWebJs) << "download requested from page:" << url;
    Q_EMIT downloadUrlRequested(url);
}

void Bridge::pageMedia(const QJsonObject& state)
{
    Q_EMIT pageMediaChanged(state);
}

} // namespace pldl::web
