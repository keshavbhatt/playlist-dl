#pragma once

#include <QLoggingCategory>

namespace pldl::services {

Q_DECLARE_LOGGING_CATEGORY(lcServices)
/// The Search page's services: the search service, the engine fallback and
/// the suggestions (ADR-003).
Q_DECLARE_LOGGING_CATEGORY(lcSearch)

} // namespace pldl::services
