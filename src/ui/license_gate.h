// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include "services/licensing/license_service.h"

#include <QString>

namespace pldl::ui {

/// The one place the UI asks whether a Pro feature may run: whole playlist
/// downloads and anything above the free tier's daily allowance. A refusal
/// has already shown the plans sheet by the time false comes back. Tests
/// subclass it to stand in for a Free tier.
class LicenseGate
{
public:
    explicit LicenseGate(services::LicenseService* service = nullptr);
    virtual ~LicenseGate();
    LicenseGate(const LicenseGate&) = delete;
    LicenseGate& operator=(const LicenseGate&) = delete;

    /// True when `feature` may run; the name is what the plans sheet shows.
    [[nodiscard]] virtual bool checkAccess(const QString& feature);
    [[nodiscard]] bool admitPlaylist();

private:
    services::LicenseService* m_service = nullptr;
};

} // namespace pldl::ui
