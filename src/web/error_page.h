#pragma once

#include <QColor>
#include <QString>
#include <QUrl>

namespace pldl::web {

/// The colours the load-failure page is drawn with: the UI layer hands over
/// its tokens (DESIGN.md section 1) so the page matches the app in both
/// schemes; the defaults are the dark tokens.
struct ErrorPageStyle
{
    QColor background{0x12, 0x14, 0x17};
    QColor text{0xF1, 0xF3, 0xF5};
    QColor muted{0x9A, 0xA3, 0xAF};
    QColor accent{0x2F, 0x6F, 0xE4};
    QColor accentHover{0x2A, 0x62, 0xCF};
};

/// Self-contained HTML for the app's load-failure page (replaces Chromium's
/// stock error page). "Try again" is a plain link back to `retryUrl`, so it
/// works on every site without a script bridge.
[[nodiscard]] QString errorPageHtml(const ErrorPageStyle& style, const QString& title, const QString& detail,
                                    const QUrl& retryUrl);

} // namespace pldl::web
