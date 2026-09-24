#pragma once

#include <QLoggingCategory>

// Logging categories for the core layer. Every layer declares its own in its
// `logging.h`; category names are dotted under "pldl." so they can be enabled
// with QT_LOGGING_RULES="pldl.*.debug=true".
namespace pldl::core {

Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcSettings)
Q_DECLARE_LOGGING_CATEGORY(lcDownloads)
Q_DECLARE_LOGGING_CATEGORY(lcBlocking)

} // namespace pldl::core
