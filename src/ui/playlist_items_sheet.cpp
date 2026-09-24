#include "ui/playlist_items_sheet.h"

#include "core/downloads/playlist_file.h"
#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kFileRole = Qt::UserRole + 1;
} // namespace

PlaylistItemsSheet::PlaylistItemsSheet(const core::DownloadJob& job, core::ThemeService& theme, QWidget* parent)
    : QDialog(parent)
    , m_job(job)
    , m_theme(theme)
    , m_entries(core::playlist_file::entriesOf(job))
    , m_original(m_entries)
{
    setupUi();
}

void PlaylistItemsSheet::setupUi()
{
    setWindowTitle(m_job.title.isEmpty() ? tr("Playlist") : m_job.title);
    setModal(true);
    setMinimumWidth(kWidth);
    const Tokens t = Tokens::forScheme(m_theme.isDark());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(12);

    auto* title = new QLabel(m_job.title.isEmpty() ? tr("Playlist") : m_job.title, this);
    title->setProperty("pldlHeading", true);
    title->setWordWrap(true);
    root->addWidget(title);
    m_summary = new QLabel(this);
    m_summary->setObjectName(u"summary"_s);
    m_summary->setProperty("pldlMuted", true);
    m_summary->setWordWrap(true);
    root->addWidget(m_summary);

    m_list = new QListWidget(this);
    m_list->setObjectName(u"itemList"_s);
    m_list->setAccessibleName(tr("Playlist items"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setMinimumHeight(260);
    m_list->setUniformItemSizes(true);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &PlaylistItemsSheet::refreshButtons);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { playSelected(); });
    root->addWidget(m_list, 1);

    // Arranging and playing one item.
    auto* tools = new QHBoxLayout;
    tools->setSpacing(8);
    const auto flat = [this, &t](const QString& text, const QString& glyph) {
        auto* button = new QPushButton(text, this);
        button->setProperty("pldlFlat", true);
        button->setIcon(icons::themed(glyph, t.text, t.muted));
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    m_play = flat(tr("Play"), u"play"_s);
    m_play->setToolTip(tr("Play the selected item"));
    m_play->setObjectName(u"playButton"_s);
    connect(m_play, &QPushButton::clicked, this, &PlaylistItemsSheet::playSelected);
    m_reveal = flat(tr("Folder"), u"folder"_s);
    m_reveal->setToolTip(tr("Show the selected file in its folder"));
    m_reveal->setObjectName(u"revealButton"_s);
    connect(m_reveal, &QPushButton::clicked, this, &PlaylistItemsSheet::revealSelected);
    m_up = flat(tr("Up"), u"chevron-up"_s);
    m_up->setToolTip(tr("Move the selected item up"));
    m_up->setObjectName(u"moveUpButton"_s);
    connect(m_up, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    m_down = flat(tr("Down"), u"chevron-down"_s);
    m_down->setToolTip(tr("Move the selected item down"));
    m_down->setObjectName(u"moveDownButton"_s);
    connect(m_down, &QPushButton::clicked, this, [this] { moveSelected(1); });
    m_sortPlaylist = flat(tr("Original"), u"playlist"_s);
    m_sortPlaylist->setToolTip(tr("Back to the playlist's own order"));
    m_sortPlaylist->setObjectName(u"sortPlaylistButton"_s);
    connect(m_sortPlaylist, &QPushButton::clicked, this, &PlaylistItemsSheet::sortByPlaylistOrder);
    m_sortName = flat(tr("By name"), u"sliders"_s);
    m_sortName->setToolTip(tr("Sort the items by title"));
    m_sortName->setObjectName(u"sortNameButton"_s);
    connect(m_sortName, &QPushButton::clicked, this, &PlaylistItemsSheet::sortByName);
    for (QPushButton* button : {m_play, m_reveal, m_up, m_down, m_sortPlaylist, m_sortName}) {
        tools->addWidget(button);
    }
    tools->addStretch(1);
    root->addLayout(tools);

    auto* footer = new QHBoxLayout;
    footer->setSpacing(8);
    m_save = new QPushButton(tr("Save playlist file"), this);
    m_save->setObjectName(u"saveButton"_s);
    m_save->setToolTip(tr("Writes an .m3u8 next to the videos in the order shown"));
    m_save->setCursor(Qt::PointingHandCursor);
    connect(m_save, &QPushButton::clicked, this, [this] {
        if (writePlaylistFile()) {
            Q_EMIT toast(tr("Playlist file saved"));
            rebuildList();
        }
    });
    footer->addWidget(m_save);
    footer->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(close);
    m_playAll = new QPushButton(tr("Play all"), this);
    m_playAll->setObjectName(u"playAllButton"_s);
    m_playAll->setProperty("pldlPrimary", true);
    m_playAll->setIcon(icons::themed(u"play"_s, t.accentText));
    m_playAll->setCursor(Qt::PointingHandCursor);
    m_playAll->setDefault(true);
    m_playAll->setToolTip(tr("Saves the playlist file in the order shown and opens it in your media player"));
    connect(m_playAll, &QPushButton::clicked, this, &PlaylistItemsSheet::playAll);
    footer->addWidget(m_playAll);
    root->addLayout(footer);

    rebuildList();
}

void PlaylistItemsSheet::rebuildList()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const int selected = m_list->currentRow();
    m_list->clear();
    int index = 0;
    for (const core::PlaylistEntry& entry : m_entries) {
        ++index;
        const bool downloaded = !entry.file.isEmpty() && QFileInfo::exists(entry.file);
        const bool missing = !entry.file.isEmpty() && !downloaded;
        const QString name = entry.title.isEmpty() ? QFileInfo(entry.file).completeBaseName() : entry.title;
        const QString state = downloaded ? QString() : (missing ? tr("file missing") : tr("not downloaded"));
        auto* item = new QListWidgetItem(
            state.isEmpty() ? u"%1.  %2"_s.arg(index).arg(name) : u"%1.  %2  (%3)"_s.arg(index).arg(name, state),
            m_list);
        item->setData(kFileRole, downloaded ? entry.file : QString());
        item->setToolTip(entry.file.isEmpty() ? tr("Not downloaded yet") : entry.file);
        item->setForeground(downloaded ? t.text : t.muted);
        item->setIcon(icons::themed(downloaded ? u"check"_s : (missing ? u"warning"_s : u"downloads"_s),
                                    downloaded ? t.success : t.muted));
    }
    if (selected >= 0 && selected < m_list->count()) {
        m_list->setCurrentRow(selected);
    }
    const int have = downloadedCount();
    const int total = static_cast<int>(m_entries.size());
    QString summary = have == total ? tr("%1 of %2 downloaded, all there.").arg(have).arg(total)
                                    : tr("%1 of %2 downloaded.").arg(have).arg(total);
    if (QFileInfo::exists(playlistFilePath())) {
        summary += u' ' + tr("A playlist file is next to the videos; Play all updates it in the order shown.");
    } else {
        summary += u' ' + tr("Play all writes a playlist file next to the videos in the order shown.");
    }
    m_summary->setText(summary);
    refreshButtons();
}

void PlaylistItemsSheet::refreshButtons()
{
    const int row = m_list->currentRow();
    const bool selected = row >= 0;
    const bool playable = selected && !m_list->item(row)->data(kFileRole).toString().isEmpty();
    m_play->setEnabled(playable);
    m_reveal->setEnabled(playable);
    m_up->setEnabled(selected && row > 0);
    m_down->setEnabled(selected && row < m_list->count() - 1);
    const bool any = downloadedCount() > 0;
    m_save->setEnabled(any);
    m_playAll->setEnabled(any);
}

QStringList PlaylistItemsSheet::orderedFiles() const
{
    QStringList files;
    for (const core::PlaylistEntry& entry : m_entries) {
        if (!entry.file.isEmpty() && QFileInfo::exists(entry.file)) {
            files << entry.file;
        }
    }
    return files;
}

int PlaylistItemsSheet::downloadedCount() const
{
    return static_cast<int>(orderedFiles().size());
}

QString PlaylistItemsSheet::playlistFilePath() const
{
    return core::playlist_file::pathFor(m_job);
}

bool PlaylistItemsSheet::writePlaylistFile()
{
    QList<core::PlaylistEntry> present;
    for (const core::PlaylistEntry& entry : m_entries) {
        if (!entry.file.isEmpty() && QFileInfo::exists(entry.file)) {
            present << entry;
        }
    }
    if (present.isEmpty()) {
        return false;
    }
    const QString path = playlistFilePath();
    return core::playlist_file::write(
        path, core::playlist_file::m3uContent(present, QFileInfo(path).absolutePath(), m_job.title));
}

void PlaylistItemsSheet::moveSelected(int delta)
{
    const int row = m_list->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_entries.size()) {
        return;
    }
    m_entries.move(row, target);
    rebuildList();
    m_list->setCurrentRow(target);
}

void PlaylistItemsSheet::sortByName()
{
    std::stable_sort(m_entries.begin(), m_entries.end(), [](const core::PlaylistEntry& a, const core::PlaylistEntry& b) {
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    rebuildList();
}

void PlaylistItemsSheet::sortByPlaylistOrder()
{
    m_entries = m_original;
    rebuildList();
}

void PlaylistItemsSheet::playSelected()
{
    const int row = m_list->currentRow();
    if (row < 0) {
        return;
    }
    const QString file = m_list->item(row)->data(kFileRole).toString();
    if (!file.isEmpty()) {
        platform::openFile(file);
    }
}

void PlaylistItemsSheet::revealSelected()
{
    const int row = m_list->currentRow();
    if (row < 0) {
        return;
    }
    const QString file = m_list->item(row)->data(kFileRole).toString();
    if (!file.isEmpty()) {
        platform::revealInFileManager(file);
    }
}

void PlaylistItemsSheet::playAll()
{
    if (!writePlaylistFile()) {
        Q_EMIT toast(tr("Nothing downloaded yet"));
        return;
    }
    rebuildList();
    platform::openFile(playlistFilePath());
    accept();
}

} // namespace pldl::ui
