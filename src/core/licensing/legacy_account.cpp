#include "core/licensing/legacy_account.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace pldl::core {

namespace {

const QString kConfRelative = u"/.config/org.keshavnrj.ubuntu/Playlist DL.conf"_s;
const QString kIdFileRelative = u"/Downloads/.Playlist DL.id"_s;
const QString kLegacyDefaultFolderName = u"Playlist DL"_s;

} // namespace

bool looksLikeAccountId(const QString& id)
{
    static const QRegularExpression kId(u"^[A-Za-z0-9]{8,64}$"_s);
    return kId.match(id).hasMatch();
}

QStringList legacyAccountSearchHomes()
{
    QStringList homes{QDir::homePath()};
    for (const char* var : {"SNAP_REAL_HOME", "SNAP_USER_COMMON", "SNAP_USER_DATA"}) {
        if (const QString v = qEnvironmentVariable(var); !v.isEmpty()) {
            homes << v;
        }
    }
    // A native or Flatpak 3.0 next to the old snap: its data survives.
    const QString real = qEnvironmentVariable("SNAP_REAL_HOME", QDir::homePath());
    homes << real + u"/snap/playlist-dl/current"_s << real + u"/snap/playlist-dl/common"_s;
    homes.removeDuplicates();
    return homes;
}

QString legacyAccountId(const QStringList& homes)
{
    for (const QString& home : homes) {
        // QSettings: read the INI file directly rather than binding the whole
        // process to another organisation name.
        const QString conf = home + kConfRelative;
        if (QFile::exists(conf)) {
            const QSettings settings(conf, QSettings::IniFormat);
            const QString id = settings.value(u"accountId"_s).toString().trimmed();
            if (looksLikeAccountId(id)) {
                return id;
            }
        }
        QFile file(home + kIdFileRelative);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString id = QString::fromUtf8(file.readLine()).trimmed();
            if (looksLikeAccountId(id)) {
                return id;
            }
        }
    }
    return {};
}

QString legacyDownloadFolder(const QStringList& homes)
{
    for (const QString& home : homes) {
        const QString conf = home + kConfRelative;
        if (!QFile::exists(conf)) {
            continue;
        }
        const QSettings settings(conf, QSettings::IniFormat);
        const QString folder = QDir::cleanPath(settings.value(u"download_path"_s).toString().trimmed());
        if (folder.isEmpty() || QDir(folder).dirName() == kLegacyDefaultFolderName) {
            continue; // the 2.x default: the 3.0 default replaces it
        }
        if (QDir(folder).exists()) {
            return folder;
        }
    }
    return {};
}

} // namespace pldl::core
