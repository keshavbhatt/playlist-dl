#pragma once

#include <QString>
#include <QStringList>

namespace pldl::core {

/// Playlist-Dl 2.x kept its account ID in two places: QSettings
/// ("org.keshavnrj.ubuntu", "Playlist DL") key `accountId`, i.e.
/// `<home>/.config/org.keshavnrj.ubuntu/Playlist DL.conf`, and the first line
/// of `<home>/Downloads/.Playlist DL.id`. Under snap confinement `<home>` was
/// the snap's own home. These helpers read that ID back so an existing licence
/// keeps working (ADR-002, FEATURES L2; pure, tested). The 2.x download folder
/// (`download_path` in the same file) is read the same way once.

/// Homes to search, most likely first: the process home, the real home
/// (SNAP_REAL_HOME), and the snap data directories of the playlist-dl snap.
[[nodiscard]] QStringList legacyAccountSearchHomes();

/// The 2.x account ID found under any of `homes`, or empty.
[[nodiscard]] QString legacyAccountId(const QStringList& homes);

/// The 2.x download folder found under any of `homes`, or empty. Only a
/// folder the user changed away from the 2.x default is worth carrying.
[[nodiscard]] QString legacyDownloadFolder(const QStringList& homes);

/// A plausible account ID: 8+ characters of [A-Za-z0-9].
[[nodiscard]] bool looksLikeAccountId(const QString& id);

} // namespace pldl::core
