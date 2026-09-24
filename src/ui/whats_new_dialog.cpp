#include "ui/whats_new_dialog.h"

#include "core/changelog.h"
#include "platform/file_manager.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/pldl_style.h"

#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

QString WhatsNewDialog::bundledChangelog()
{
    QFile file(qEnvironmentVariableIsSet("PLDL_DEBUG_CHANGELOG") ? qEnvironmentVariable("PLDL_DEBUG_CHANGELOG")
                                                                : u":/text/CHANGELOG.md"_s);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString WhatsNewDialog::bundledNotes(const QString& version)
{
    return core::changelogSection(bundledChangelog(), version);
}

WhatsNewDialog::WhatsNewDialog(const QString& runningVersion, const QString& changelogMarkdown, QWidget* parent)
    : QDialog(parent)
    , m_running(runningVersion)
    , m_markdown(changelogMarkdown)
    , m_releases(core::changelogReleases(changelogMarkdown))
{
    setupUi();
}

void WhatsNewDialog::setupUi()
{
    const bool dark = parent() != nullptr && parentWidget()->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    setModal(true);
    setWindowTitle(tr("What's new"));
    setMinimumWidth(560);
    setMaximumWidth(680);
    resize(600, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* badge = new QLabel(this);
    badge->setFixedSize(44, 44);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.accent.name()));
    badge->setPixmap(icons::pixmap(u"bolt"_s, t.accentText, 24, devicePixelRatioF()));
    top->addWidget(badge, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    m_title = new QLabel(this);
    m_title->setObjectName(u"whatsNewTitle"_s);
    m_title->setProperty("pldlTitle", true);
    text->addWidget(m_title);
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(u"whatsNewSubtitle"_s);
    m_subtitle->setProperty("pldlMuted", true);
    text->addWidget(m_subtitle);
    top->addLayout(text, 1);

    // The Version picker: only worth showing once there is a second release to pick.
    m_pickerRow = new QWidget(this);
    m_pickerRow->setObjectName(u"versionPickerRow"_s);
    auto* pickerLayout = new QHBoxLayout(m_pickerRow);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    pickerLayout->setSpacing(8);
    auto* pickerLabel = new QLabel(tr("Version"), m_pickerRow);
    pickerLabel->setProperty("pldlMuted", true);
    pickerLayout->addWidget(pickerLabel);
    m_picker = new QComboBox(m_pickerRow);
    m_picker->setObjectName(u"versionPicker"_s);
    m_picker->setAccessibleName(tr("Version"));
    m_picker->setToolTip(tr("Read the notes of an earlier release"));
    for (const core::ChangelogRelease& release : m_releases) {
        m_picker->addItem(release.version == m_running ? tr("%1 (this version)").arg(release.version)
                                                       : release.version,
                          release.version);
    }
    pickerLabel->setBuddy(m_picker);
    pickerLayout->addWidget(m_picker);
    connect(m_picker, &QComboBox::currentIndexChanged, this,
            [this](int index) { showVersion(m_picker->itemData(index).toString()); });
    m_pickerRow->setVisible(m_releases.size() > 1);
    top->addWidget(m_pickerRow, 0, Qt::AlignTop);
    root->addLayout(top);

    auto* card = new QFrame(this);
    card->setProperty("pldlCard", true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 8, 12, 8);
    m_notes = new QTextBrowser(card);
    m_notes->setObjectName(u"whatsNewNotes"_s);
    m_notes->setProperty("pldlNotes", true);
    m_notes->setReadOnly(true);
    m_notes->setFrameShape(QFrame::NoFrame);
    m_notes->viewport()->setAutoFillBackground(false);
    m_notes->setOpenLinks(false);
    m_notes->setOpenExternalLinks(false);
    m_notes->setFocusPolicy(Qt::NoFocus); // Enter goes to the default button, not a link
    m_notes->setAccessibleName(tr("Release notes"));
    m_notes->setMinimumHeight(220);
    connect(m_notes, &QTextBrowser::anchorClicked, this,
            [](const QUrl& url) { platform::openUrl(url.toString()); });
    cardLayout->addWidget(m_notes, 1);
    root->addWidget(card, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* guide = new QPushButton(tr("Guide"), this);
    connect(guide, &QPushButton::clicked, this, [] { platform::openUrl(links::kGuide); });
    buttons->addWidget(guide);
    auto* full = new QPushButton(tr("Full changelog"), this);
    connect(full, &QPushButton::clicked, this, [] { platform::openUrl(links::kChangelog); });
    buttons->addWidget(full);
    auto* ok = new QPushButton(tr("Got it"), this);
    ok->setProperty("pldlPrimary", true);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    // Open on the running version, or the newest release when the running
    // version has no section (a dev build ahead of the changelog).
    const int running = m_picker->findData(m_running);
    if (running >= 0 || m_picker->count() > 0) {
        m_picker->setCurrentIndex(qMax(0, running));
    }
    showVersion(m_picker->count() > 0 ? m_picker->currentData().toString() : m_running);
    ok->setFocus(); // Enter dismisses; the picker is one Shift+Tab away
}

void WhatsNewDialog::showVersion(const QString& version)
{
    const auto it = std::find_if(m_releases.cbegin(), m_releases.cend(),
                                 [&version](const core::ChangelogRelease& r) { return r.version == version; });
    if (it == m_releases.cend() && version != m_running) {
        return;
    }
    if (const int index = m_picker->findData(version); index >= 0 && index != m_picker->currentIndex()) {
        m_picker->setCurrentIndex(index); // re-enters through currentIndexChanged
        return;
    }
    m_title->setText(tr("What's new in %1").arg(version));
    if (version == m_running) {
        m_subtitle->setText(tr("Highlights of this release."));
    } else if (it != m_releases.cend() && !it->date.isEmpty()) {
        m_subtitle->setText(tr("Released %1. You are on %2.").arg(it->date, m_running));
    } else {
        m_subtitle->setText(tr("You are on %1.").arg(m_running));
    }
    fillNotes(core::changelogSection(m_markdown, version));
}

QString WhatsNewDialog::shownVersion() const
{
    return m_picker->count() > 0 ? m_picker->currentData().toString() : m_running;
}

int WhatsNewDialog::releaseCount() const
{
    return static_cast<int>(m_releases.size());
}

void WhatsNewDialog::fillNotes(const QString& markdown)
{
    m_notes->setMarkdown(markdown);
    // Markdown headings ignore the document style sheet; keep "### Added"
    // a shade above body size instead of banner-sized.
    for (QTextBlock block = m_notes->document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat format;
            format.setFontPointSize(m_notes->font().pointSizeF() + 1);
            format.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(format);
        }
    }
    m_notes->verticalScrollBar()->setValue(0);
}

} // namespace pldl::ui
