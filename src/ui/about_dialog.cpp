#include "ui/about_dialog.h"

#include "core/downloads/engine_spec.h"

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "ui/bug_report_dialog.h"
#include "ui/diagnostics.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/pldl_style.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
const QString kDonateUrl = u"https://www.paypal.com/paypalme/keshavnrj/11"_s;
const QString kWebsiteUrl = u"https://ktechpit.com"_s;
} // namespace

AboutDialog::AboutDialog(const core::Settings& settings, core::ThemeService& theme, const QString& userAgent,
                         const QString& engineSummary, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_userAgent(userAgent)
    , m_engineSummary(engineSummary)
{
    setupUi();
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About Playlist Downloader"));
    setModal(true);
    setMinimumWidth(520);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 18);
    root->setSpacing(14);

    auto* hero = new QHBoxLayout;
    hero->setSpacing(18);
    auto* icon = new QLabel(this);
    icon->setPixmap(icons::brand().pixmap(QSize(88, 88), devicePixelRatioF()));
    icon->setFixedSize(88, 88);
    hero->addWidget(icon, 0, Qt::AlignTop);
    auto* identity = new QVBoxLayout;
    identity->setSpacing(3);
    auto* name = new QLabel(u"Playlist Downloader"_s, this);
    name->setProperty("pldlHeading", true);
    identity->addWidget(name);
    auto* tagline = new QLabel(tr("YouTube, in a real desktop app."), this);
    identity->addWidget(tagline);
    auto* version = new QLabel(tr("Version %1").arg(QApplication::applicationVersion()), this);
    version->setProperty("pldlMuted", true);
    identity->addWidget(version);
    identity->addSpacing(6);
    auto* author = new QLabel(
        tr("Designed and developed by Keshav Bhatt · <a href=\"%1\">ktechpit.com</a>").arg(kWebsiteUrl),
        this);
    author->setTextFormat(Qt::RichText);
    author->setOpenExternalLinks(false);
    author->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(author, &QLabel::linkActivated, this, [](const QString& link) { platform::openUrl(link); });
    identity->addWidget(author);
    auto* disclaimer =
        new QLabel(tr("Site names and trademarks belong to their owners. This app is independent and not "
                      "affiliated with, endorsed by, or sponsored by any of them."),
                   this);
    disclaimer->setProperty("pldlMuted", true);
    disclaimer->setWordWrap(true);
    identity->addWidget(disclaimer);
    hero->addLayout(identity, 1);
    root->addLayout(hero);

    auto* links = new QHBoxLayout;
    links->setSpacing(8);
    const auto linkButton = [this](const QString& label, const QString& url) {
        auto* button = new QPushButton(label, this);
        connect(button, &QPushButton::clicked, this, [url] { platform::openUrl(url); });
        return button;
    };
    links->addWidget(linkButton(tr("Online guide"), links::kGuide));
    auto* report = new QPushButton(tr("Report a bug…"), this);
    connect(report, &QPushButton::clicked, this, &AboutDialog::reportBug);
    links->addWidget(report);
    links->addWidget(linkButton(tr("Contact"), links::kContact));
    links->addWidget(linkButton(tr("Donate"), kDonateUrl));
    links->addWidget(linkButton(tr("More apps"), links::kMoreApps));
    links->addStretch(1);
    root->addLayout(links);

    auto* sep = new QFrame(this);
    sep->setProperty("pldlSeparator", true);
    root->addWidget(sep);

    // Supported sites (FEATURES B7): the download engine's own list.
    auto* sitesRow = new QHBoxLayout;
    auto* sitesTitle = new QLabel(tr("Supported sites"), this);
    sitesTitle->setProperty("pldlSection", true);
    sitesRow->addWidget(sitesTitle, 1);
    m_siteCount = new QLabel(this);
    m_siteCount->setProperty("pldlMuted", true);
    sitesRow->addWidget(m_siteCount);
    root->addLayout(sitesRow);
    m_siteFilter = new QLineEdit(this);
    m_siteFilter->setObjectName(u"siteFilter"_s);
    m_siteFilter->setPlaceholderText(tr("Filter sites"));
    m_siteFilter->setClearButtonEnabled(true);
    connect(m_siteFilter, &QLineEdit::textChanged, this, &AboutDialog::filterSites);
    root->addWidget(m_siteFilter);
    m_sites = new QListWidget(this);
    m_sites->setObjectName(u"siteList"_s);
    m_sites->setMinimumHeight(110);
    m_sites->setMaximumHeight(160);
    root->addWidget(m_sites);
    m_siteCount->setText(tr("Loading the list\u2026"));

    auto* debugRow = new QHBoxLayout;
    auto* debugTitle = new QLabel(tr("Diagnostics"), this);
    debugTitle->setProperty("pldlSection", true);
    debugRow->addWidget(debugTitle, 1);
    auto* copy = new QPushButton(tr("Copy"), this);
    copy->setToolTip(
        tr("Copies versions, paths and recent log lines for a bug report. Your session is never included."));
    connect(copy, &QPushButton::clicked, this, &AboutDialog::copyDiagnostics);
    debugRow->addWidget(copy);
    root->addLayout(debugRow);
    m_debugText = new QPlainTextEdit(this);
    m_debugText->setReadOnly(true);
    m_debugText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_debugText->setPlainText(buildDiagnostics(m_settings, m_userAgent, m_engineSummary, 30));
    m_debugText->setMinimumHeight(160);
    root->addWidget(m_debugText, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setProperty("pldlPrimary", true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

void AboutDialog::reportBug()
{
    auto* dialog = new BugReportDialog(m_settings, m_theme, m_userAgent, m_engineSummary, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void AboutDialog::copyDiagnostics()
{
    QApplication::clipboard()->setText(buildDiagnostics(m_settings, m_userAgent, m_engineSummary));
}

void AboutDialog::setEnginePath(const QString& path)
{
    if (path.isEmpty()) {
        m_siteCount->setText(tr("The download engine is not ready yet"));
        return;
    }
    auto* process = new QProcess(this);
    process->setProcessEnvironment(core::engineProcessEnvironment());
    process->setProgram(path);
    process->setArguments({u"--list-extractors"_s});
    connect(process, &QProcess::finished, this, [this, process](int, QProcess::ExitStatus) {
        QStringList sites;
        for (const QByteArray& line : process->readAllStandardOutput().split('\n')) {
            const QString site = QString::fromUtf8(line).trimmed();
            // The engine lists one extractor per line; the ":tab" and ":user"
            // variants and the generic scrapers are noise to a person.
            if (site.isEmpty() || site.contains(u':') || site.startsWith(u"generic"_s, Qt::CaseInsensitive)) {
                continue;
            }
            sites << site;
        }
        sites.removeDuplicates();
        setSupportedSites(sites);
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        m_siteCount->setText(tr("The list could not be read"));
        process->deleteLater();
    });
    process->start();
}

void AboutDialog::setSupportedSites(const QStringList& sites)
{
    m_allSites = sites;
    filterSites(m_siteFilter->text());
}

int AboutDialog::supportedSiteCount() const
{
    return static_cast<int>(m_allSites.size());
}

void AboutDialog::filterSites(const QString& text)
{
    m_sites->clear();
    int shown = 0;
    for (const QString& site : m_allSites) {
        if (text.isEmpty() || site.contains(text, Qt::CaseInsensitive)) {
            m_sites->addItem(site);
            ++shown;
        }
    }
    const int total = static_cast<int>(m_allSites.size());
    m_siteCount->setText(text.isEmpty() ? (total == 1 ? tr("1 site") : tr("%n sites", nullptr, total))
                                        : tr("%1 of %2 sites").arg(shown).arg(total));
}

} // namespace pldl::ui
