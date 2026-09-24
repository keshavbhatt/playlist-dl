#include "core/navigation_policy.h"

#include "core/youtube_url.h"

#include <QStringList>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace pldl::core {

namespace {

bool isWebScheme(const QString& scheme)
{
    return scheme == u"http"_s || scheme == u"https"_s;
}

bool isInternalScheme(const QString& scheme)
{
    return scheme == u"data"_s || scheme == u"about"_s || scheme == u"blob"_s || scheme == u"qrc"_s;
}

} // namespace

bool shouldOpenExternally(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    if (isInternalScheme(scheme) || isWebScheme(scheme)) {
        return false;
    }
    // Only a short list of schemes may leave the app: a page (or an unwrapped
    // redirect) must not be able to hand file: or an arbitrary protocol
    // handler to the desktop.
    static const QStringList kAllowed{u"mailto"_s, u"tel"_s, u"magnet"_s};
    return kAllowed.contains(scheme);
}

bool isInAppPopupUrl(const QUrl& url)
{
    if (url.isEmpty()) {
        return true; // about:blank pop-ups decide on their first navigation
    }
    const QString scheme = url.scheme().toLower();
    return isWebScheme(scheme) || isInternalScheme(scheme);
}

QUrl unwrapRedirect(const QUrl& url)
{
    if (!isYouTubeHost(url.host()) || url.path() != u"/redirect"_s) {
        return url;
    }
    const QString target = QUrlQuery(url).queryItemValue(u"q"_s, QUrl::FullyDecoded);
    const QUrl unwrapped(target);
    return unwrapped.isValid() && !unwrapped.scheme().isEmpty() ? unwrapped : url;
}

} // namespace pldl::core
