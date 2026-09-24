#include "ui/license_gate.h"

#include <QObject>

namespace pldl::ui {

LicenseGate::LicenseGate(services::LicenseService* service)
    : m_service(service)
{
}

LicenseGate::~LicenseGate() = default;

bool LicenseGate::checkAccess(const QString& feature)
{
    return m_service == nullptr || m_service->checkAccess(feature);
}

bool LicenseGate::admitPlaylist()
{
    return checkAccess(QObject::tr("Playlist downloads"));
}

} // namespace pldl::ui
