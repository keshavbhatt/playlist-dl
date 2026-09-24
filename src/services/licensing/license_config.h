#pragma once

#include "AccountLicenseConfig.h"

#include <QCoreApplication>

namespace pldl::services {

/// The app's AccountLicenseConfig: the shared ktechpit "USS" backend, app
/// code PLDL, checkout slug playlist-dl, a 10-day evaluation.
inline AccountLicenseConfig appLicenseConfig()
{
    AccountLicenseConfig cfg;
    cfg.appCode = QStringLiteral("PLDL");
    cfg.appName = QStringLiteral("Playlist Downloader");
    cfg.settingsOrgName = QCoreApplication::organizationName();
    cfg.settingsAppName = QCoreApplication::applicationName();
    cfg.checkStatusEndpoint = QStringLiteral("https://ktechpit.com/USS/public/check/status.php");
    cfg.checkoutUrlTemplate =
        QStringLiteral("https://ktechpit.com/USS/public/checkout.php?slug=playlist-dl&accountId=%1");
    cfg.selfServicePortalUrl = QStringLiteral("https://ktechpit.com/USS/public/user/");
    cfg.evaluationDurationSecs = 10LL * 24LL * 60LL * 60LL;
    return cfg;
}

} // namespace pldl::services
