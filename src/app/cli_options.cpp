#include "app/cli_options.h"

#include <QCommandLineParser>

using namespace Qt::StringLiterals;

namespace pldl::app {

bool CliOptions::hasCommands() const
{
    return download.has_value() || showSettings || quit || !urls.isEmpty();
}

CliParseResult parseCliOptions(const QStringList& arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        u"Search, browse, play and download playlists from any site"_s);
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();
    const QCommandLineOption profile({u"p"_s, u"profile"_s},
                                     u"Use a separate profile (own session and settings)."_s, u"name"_s);
    const QCommandLineOption logFile(
        u"log-file"_s, u"Write the log to <path> instead of the default location."_s, u"path"_s);
    const QCommandLineOption noLogFile(u"no-log-file"_s, u"Do not write a log file."_s);
    const QCommandLineOption download({u"d"_s, u"download"_s},
                                      u"Download a playlist, video or track link; an empty link asks for one."_s, u"url"_s);
    const QCommandLineOption settings({u"s"_s, u"settings"_s}, u"Open the settings dialog."_s);
    const QCommandLineOption quit({u"q"_s, u"quit"_s}, u"Quit the running instance."_s);
    parser.addOptions({profile, logFile, noLogFile, download, settings, quit});
    parser.addPositionalArgument(u"url"_s, u"A playlist, video or channel link to open."_s, u"[url]"_s);

    CliParseResult result;
    if (!parser.parse(arguments)) {
        result.errorText = parser.errorText();
        return result;
    }
    if (parser.isSet(help)) {
        result.helpRequested = true;
        result.helpText = parser.helpText();
        return result;
    }
    if (parser.isSet(version)) {
        result.versionRequested = true;
        return result;
    }

    CliOptions& o = result.options;
    o.profile = parser.value(profile).trimmed();
    if (parser.isSet(logFile)) {
        o.logFile = parser.value(logFile);
    }
    o.noLogFile = parser.isSet(noLogFile);
    if (parser.isSet(download)) {
        o.download = parser.value(download);
    }
    o.showSettings = parser.isSet(settings);
    o.quit = parser.isSet(quit);
    o.urls = parser.positionalArguments();
    return result;
}

QList<QJsonObject> commandsFor(const CliOptions& options)
{
    QList<QJsonObject> commands;
    const auto key = QString::fromLatin1(cmd::kKey);
    const auto urlKey = QString::fromLatin1(cmd::kUrlKey);
    if (options.quit) {
        commands.append({{key, QString::fromLatin1(cmd::kQuit)}});
        return commands;
    }
    for (const QString& url : options.urls) {
        commands.append({{key, QString::fromLatin1(cmd::kOpen)}, {urlKey, url}});
    }
    if (options.download) {
        commands.append({{key, QString::fromLatin1(cmd::kDownload)}, {urlKey, *options.download}});
    }
    if (options.showSettings) {
        commands.append({{key, QString::fromLatin1(cmd::kSettings)}});
    }
    commands.append({{key, QString::fromLatin1(cmd::kRaise)}});
    return commands;
}

} // namespace pldl::app
