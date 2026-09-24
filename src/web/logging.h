#pragma once

#include <QLoggingCategory>

namespace pldl::web {

Q_DECLARE_LOGGING_CATEGORY(lcWeb)
/// Console output of the page itself (console.log/warn/error from YouTube
/// and from our injected scripts).
Q_DECLARE_LOGGING_CATEGORY(lcWebJs)

} // namespace pldl::web
