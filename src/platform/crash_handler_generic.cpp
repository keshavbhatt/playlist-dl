#include "platform/crash_handler.h"

namespace pldl::platform {

void installCrashHandler(const QString& /*crashFilePath*/) {}

QString lastCrashReport()
{
    return {};
}

} // namespace pldl::platform
