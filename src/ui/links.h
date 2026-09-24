#pragma once

#include <QString>

/// Public web addresses the app points people at (the public repository:
/// guide, issues, changelog).
namespace pldl::ui::links {

inline const QString kGuide = QStringLiteral("https://github.com/keshavbhatt/p-pldl/blob/main/GUIDE.md");
inline const QString kMoreApps = QStringLiteral("https://ktechpit.com/USS/public/products.php");
inline const QString kContact = QStringLiteral("mailto:connect@ktechpit.com");
inline const QString kIssues = QStringLiteral("https://github.com/keshavbhatt/p-pldl/issues");
inline const QString kChangelog =
    QStringLiteral("https://github.com/keshavbhatt/p-pldl/blob/main/CHANGELOG.md");

} // namespace pldl::ui::links
