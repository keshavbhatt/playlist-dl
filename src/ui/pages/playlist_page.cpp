#include "ui/pages/playlist_page.h"

#include "core/downloads/download_options.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "services/media_probe.h"
#include "ui/busy_button.h"
#include "ui/icons.h"
#include "ui/logging.h"
#include "ui/playlist_entry_delegate.h"
#include "ui/playlist_model.h"
#include "ui/pldl_style.h"
#include "ui/range_slider.h"
#include "ui/thumbnail_cache.h"
#include "ui/thumbnail_painter.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QSpacerItem>
#include <QMenu>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kThumbWidth = 96;
constexpr int kThumbHeight = 54;
constexpr int kSpinWidth = 64;
constexpr int kSliderWidth = 120;
using Role = PlaylistEntryDelegate::Role;
} // namespace

PlaylistPage::PlaylistPage(core::Settings& settings, core::ThemeService& theme, services::MediaProbe& probe,
                           ThumbnailCache& thumbnails, QWidget* parent)
    : Page(tr("Playlist"), theme, parent)
    , m_settings(settings)
    , m_probe(probe)
    , m_thumbnails(thumbnails)
{
    m_model = new PlaylistModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterRole(Role::TitleRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortRole(Role::IndexRole);
    m_proxy->setDynamicSortFilter(true);
    m_proxy->sort(0, Qt::AscendingOrder);

    buildHeader();
    m_stack = new QStackedWidget(this);
    content()->addWidget(m_stack, 1);
    buildList();
    buildStatus();
    applyIcons();

    connect(&m_probe, &services::MediaProbe::finished, this, &PlaylistPage::handleProbeFinished);
    connect(&m_probe, &services::MediaProbe::failed, this, &PlaylistPage::handleProbeFailed);
    connect(&m_thumbnails, &ThumbnailCache::ready, this, [this](const QString& url) {
        if (url == m_thumbnailUrl) {
            renderThumbnail();
        }
        m_list->viewport()->update();
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                if (roles.isEmpty() || roles.contains(Qt::CheckStateRole)) {
                    updateSelectionUi();
                }
            });
    setState(State::Idle);
}

PlaylistPage::~PlaylistPage()
{
    // The probe outlives the page: a run still going is not ours to answer.
    if (m_probeId != 0) {
        m_probe.cancel(m_probeId);
    }
}

// ---- build -----------------------------------------------------------------

void PlaylistPage::buildHeader()
{
    setTitleVisible(false);
    m_back = new QToolButton(this);
    m_back->setObjectName(u"backButton"_s);
    m_back->setProperty("pldlFlat", true);
    m_back->setToolTip(tr("Back to Search"));
    m_back->setAccessibleName(tr("Back to Search"));
    m_back->setCursor(Qt::PointingHandCursor);
    connect(m_back, &QToolButton::clicked, this, &PlaylistPage::backRequested);
    headerLayout()->insertWidget(0, m_back);

    m_thumb = new QLabel(this);
    m_thumb->setObjectName(u"playlistThumbnail"_s);
    m_thumb->setFixedSize(kThumbWidth, kThumbHeight);
    headerLayout()->insertWidget(1, m_thumb);

    auto* text = new QWidget(this);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* column = new QVBoxLayout(text);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(2);
    m_title = new QLabel(text);
    m_title->setObjectName(u"playlistTitle"_s);
    QFont titleFont = m_title->font();
    titleFont.setPixelSize(16);
    titleFont.setWeight(QFont::Medium);
    m_title->setFont(titleFont);
    m_title->installEventFilter(this); // elided to the width the layout gives it
    column->addWidget(m_title);
    m_channel = new QLabel(text);
    m_channel->setObjectName(u"playlistChannel"_s);
    m_channel->setProperty("pldlMuted", true);
    column->addWidget(m_channel);
    m_count = new QLabel(text);
    m_count->setObjectName(u"playlistCount"_s);
    m_count->setProperty("pldlMuted", true);
    column->addWidget(m_count);
    headerLayout()->insertWidget(2, text, 1);
    headerLayout()->setStretch(4, 0); // the base's stretch gives way to the text column

    m_download = new QPushButton(tr("Download"), this);
    m_download->setObjectName(u"downloadButton"_s);
    m_download->setProperty("pldlPrimary", true);
    m_download->setCursor(Qt::PointingHandCursor);
    m_download->setDefault(false);
    m_download->setAutoDefault(false);
    connect(m_download, &QPushButton::clicked, this, [this] {
        const QList<int> indexes = selectedIndexes();
        if (!busy::isBusy(m_download) && !indexes.isEmpty()) {
            Q_EMIT downloadRequested(m_info, indexes);
        }
    });
    headerLayout()->addWidget(m_download);

    m_playAll = new QPushButton(tr("Play all"), this);
    m_playAll->setObjectName(u"playAllButton"_s);
    m_playAll->setProperty("pldlFlat", true);
    m_playAll->setCursor(Qt::PointingHandCursor);
    m_playAll->setToolTip(tr("Play the playlist on the Browser page"));
    connect(m_playAll, &QPushButton::clicked, this, [this] { Q_EMIT playRequested(playAllUrl()); });
    headerLayout()->addWidget(m_playAll);

    m_copyLink = new QPushButton(tr("Copy link"), this);
    m_copyLink->setObjectName(u"copyLinkButton"_s);
    m_copyLink->setProperty("pldlFlat", true);
    m_copyLink->setCursor(Qt::PointingHandCursor);
    connect(m_copyLink, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_url.toString());
        Q_EMIT toast(tr("Link copied"));
    });
    headerLayout()->addWidget(m_copyLink);
}

void PlaylistPage::buildToolbar()
{
    auto* row = new QWidget(m_listPane);
    row->setObjectName(u"toolbarRow"_s);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    m_toolRow = layout;

    m_selectAll = new QCheckBox(tr("Select all"), row);
    m_selectAll->setObjectName(u"selectAllBox"_s);
    m_selectAll->setTristate(true);
    connect(m_selectAll, &QCheckBox::clicked, this, [this] {
        // A click means "all" unless everything visible is already checked.
        const QList<int> rows = visibleRows();
        const bool allChecked = std::all_of(rows.cbegin(), rows.cend(), [this](int r) {
            return m_model->isUnavailable(r) || m_model->isChecked(r) || (m_skipDownloaded && m_model->isDownloaded(r));
        });
        setAllChecked(!allChecked);
    });
    layout->addWidget(m_selectAll);
    layout->addSpacing(8);

    auto* fromLabel = new QLabel(tr("From"), row);
    fromLabel->setProperty("pldlMuted", true);
    layout->addWidget(fromLabel);
    m_from = new QSpinBox(row);
    m_from->setObjectName(u"fromSpin"_s);
    m_from->setAccessibleName(tr("From item"));
    m_from->setFixedWidth(kSpinWidth);
    layout->addWidget(m_from);
    m_range = new RangeSlider(theme(), row);
    m_range->setAccessibleName(tr("Item range"));
    m_range->setObjectName(u"rangeSlider"_s);
    m_range->setFixedWidth(kSliderWidth);
    layout->addWidget(m_range);
    auto* toLabel = new QLabel(tr("to"), row);
    toLabel->setProperty("pldlMuted", true);
    layout->addWidget(toLabel);
    m_to = new QSpinBox(row);
    m_to->setObjectName(u"toSpin"_s);
    m_to->setAccessibleName(tr("To item"));
    m_to->setFixedWidth(kSpinWidth);
    layout->addWidget(m_to);
    for (QSpinBox* spin : {m_from, m_to}) {
        spin->setRange(1, 1);
        connect(spin, &QSpinBox::valueChanged, this, [this] {
            if (!m_syncing) {
                applyRange();
            }
        });
    }
    connect(m_range, &RangeSlider::userChanged, this, [this](int lower, int upper) {
        m_syncing = true;
        m_from->setValue(lower);
        m_to->setValue(upper);
        m_syncing = false;
        applyRange();
    });
    layout->addSpacing(8);

    m_filter = new QLineEdit(row);
    m_filter->setObjectName(u"filterField"_s);
    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->setAccessibleName(tr("Filter by title"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setMinimumWidth(110);
    connect(m_filter, &QLineEdit::textChanged, this, &PlaylistPage::setFilter);
    layout->addWidget(m_filter, 1);

    m_sort = new QComboBox(row);
    m_sort->setObjectName(u"sortCombo"_s);
    m_sort->setAccessibleName(tr("Sort"));
    m_sort->addItem(tr("Playlist order"), static_cast<int>(SortOrder::PlaylistOrder));
    m_sort->addItem(tr("Title"), static_cast<int>(SortOrder::Title));
    m_sort->addItem(tr("Duration"), static_cast<int>(SortOrder::Duration));
    connect(m_sort, &QComboBox::currentIndexChanged, this,
            [this](int index) { setSortOrder(static_cast<SortOrder>(m_sort->itemData(index).toInt())); });
    layout->addWidget(m_sort);

    m_skip = new QCheckBox(tr("Skip already downloaded"), row);
    m_skip->setObjectName(u"skipBox"_s);
    m_skip->setToolTip(tr("Leave out items that already have a file in the playlist's folder"));
    m_skip->setChecked(m_skipDownloaded);
    connect(m_skip, &QCheckBox::toggled, this, &PlaylistPage::setSkipDownloaded);
    layout->addWidget(m_skip);

    dynamic_cast<QVBoxLayout*>(m_listPane->layout())->addWidget(row);
    // Filter, sort and skip drop to a second row when the page is narrow
    // (review 2026-09-25: one row was tight at the window's minimum width).
    m_toolRow2 = new QWidget(m_listPane);
    m_toolRow2->setObjectName(u"toolbarRow2"_s);
    auto* layout2 = new QHBoxLayout(m_toolRow2);
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->setSpacing(8);
    m_toolRow2->hide();
    dynamic_cast<QVBoxLayout*>(m_listPane->layout())->addWidget(m_toolRow2);
}

void PlaylistPage::buildList()
{
    m_listPane = new QWidget(m_stack);
    auto* layout = new QVBoxLayout(m_listPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    buildToolbar();

    m_list = new QListView(m_listPane);
    m_list->setObjectName(u"entriesList"_s);
    m_list->setAccessibleName(tr("Playlist entries"));
    m_list->setModel(m_proxy);
    m_delegate = new PlaylistEntryDelegate(m_thumbnails, theme(), this);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setMouseTracking(true);
    m_list->setUniformItemSizes(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setResizeMode(QListView::Adjust);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setStyleSheet(u"QListView { background: transparent; }"_s);
    m_list->installEventFilter(this);
    connect(m_delegate, &PlaylistEntryDelegate::actionTriggered, this,
            [this](const QModelIndex& index, PlaylistEntryDelegate::Action action) {
                handleRowAction(index, static_cast<int>(action));
            });
    // The hover buttons as a menu too, plus Copy link (review 2026-09-24).
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_list->indexAt(pos).isValid() ? m_list->indexAt(pos) : m_list->currentIndex();
        if (!index.isValid()) {
            return;
        }
        const core::MediaEntry entry = m_model->entryAt(m_proxy->mapToSource(index).row());
        if (entry.url.isEmpty()) {
            return;
        }
        QMenu menu(this);
        const QColor tint = palette().color(QPalette::Text);
        menu.addAction(icons::themed(u"play"_s, tint), PlaylistEntryDelegate::actionLabel(PlaylistEntryDelegate::Action::Play),
                       this, [this, index] { handleRowAction(index, static_cast<int>(PlaylistEntryDelegate::Action::Play)); });
        menu.addAction(icons::themed(u"download"_s, tint),
                       PlaylistEntryDelegate::actionLabel(PlaylistEntryDelegate::Action::Download), this,
                       [this, index] { handleRowAction(index, static_cast<int>(PlaylistEntryDelegate::Action::Download)); });
        menu.addAction(icons::themed(u"copy"_s, tint), tr("Copy link"), this, [this, entry] {
            QGuiApplication::clipboard()->setText(entry.url);
            Q_EMIT toast(tr("Link copied"));
        });
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });
    layout->addWidget(m_list, 1);

    m_footer = new QLabel(m_listPane);
    m_footer->setObjectName(u"footerLabel"_s);
    m_footer->setProperty("pldlMuted", true);
    layout->addWidget(m_footer);
    m_stack->addWidget(m_listPane);
}

void PlaylistPage::buildStatus()
{
    m_statusPane = new QWidget(m_stack);
    auto* layout = new QVBoxLayout(m_statusPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addStretch(2);
    m_status = new QLabel(m_statusPane);
    m_status->setObjectName(u"statusLabel"_s);
    m_status->setProperty("pldlMuted", true);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* retryRow = new QHBoxLayout;
    retryRow->addStretch(1);
    m_retry = new QPushButton(tr("Retry"), m_statusPane);
    m_retry->setObjectName(u"retryButton"_s);
    m_retry->setCursor(Qt::PointingHandCursor);
    connect(m_retry, &QPushButton::clicked, this, &PlaylistPage::reload);
    retryRow->addWidget(m_retry);
    retryRow->addStretch(1);
    layout->addLayout(retryRow);
    layout->addStretch(3);
    m_stack->addWidget(m_statusPane);
}

void PlaylistPage::applyIcons()
{
    const Tokens t = Tokens::forScheme(theme().isDark());
    m_back->setIcon(icons::themed(u"back"_s, t.text, t.muted));
    m_download->setIcon(icons::themed(u"download"_s, t.accentText));
    m_playAll->setIcon(icons::themed(u"play"_s, t.text, t.muted));
    m_copyLink->setIcon(icons::themed(u"copy"_s, t.text, t.muted));
    renderThumbnail();
}

void PlaylistPage::resizeEvent(QResizeEvent* event)
{
    Page::resizeEvent(event);
    relayoutToolbar();
}

void PlaylistPage::relayoutToolbar()
{
    constexpr int kNarrowBelow = 1000;
    constexpr int kWideAbove = 1060; // hysteresis: no flapping around the edge
    const bool narrow = m_toolbarNarrow ? width() < kWideAbove : width() < kNarrowBelow;
    if (narrow == m_toolbarNarrow || m_toolRow2 == nullptr) {
        return;
    }
    m_toolbarNarrow = narrow;
    auto* layout2 = dynamic_cast<QHBoxLayout*>(m_toolRow2->layout());
    QHBoxLayout* from = narrow ? m_toolRow : layout2;
    QHBoxLayout* to = narrow ? layout2 : m_toolRow;
    for (QWidget* widget : {static_cast<QWidget*>(m_filter), static_cast<QWidget*>(m_sort), static_cast<QWidget*>(m_skip)}) {
        from->removeWidget(widget);
        to->addWidget(widget, widget == m_filter ? 1 : 0);
        widget->setParent(narrow ? m_toolRow2 : m_toolRow->parentWidget());
        widget->show();
    }
    // Row one keeps its controls together once the filter's stretch has left it.
    if (narrow) {
        if (m_toolStretch == nullptr) {
            m_toolStretch = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        }
        m_toolRow->addItem(m_toolStretch);
    } else if (m_toolStretch != nullptr) {
        m_toolRow->removeItem(m_toolStretch);
    }
    m_toolRow2->setVisible(narrow);
}

void PlaylistPage::onThemeChanged()
{
    applyIcons();
    m_list->viewport()->update();
}

// ---- opening ---------------------------------------------------------------

void PlaylistPage::open(const QUrl& playlistUrl, const std::optional<services::SearchResult>& known)
{
    if (m_probeId != 0) {
        m_probe.cancel(m_probeId);
        m_probeId = 0;
    }
    m_url = playlistUrl;
    m_info = {};
    m_model->clear();
    if (known) {
        fillHeader(known->title, known->channel, known->thumbnailUrl, static_cast<int>(known->itemCount), 0);
    } else {
        fillHeader(tr("Playlist"), QString(), QString(), -1, 0);
    }
    setState(State::Loading);
    Q_EMIT hasPlaylistChanged(true);
    reload();
}

void PlaylistPage::reload()
{
    if (m_url.isEmpty()) {
        return;
    }
    if (m_probeId != 0) {
        m_probe.cancel(m_probeId);
        m_probeId = 0;
    }
    setState(State::Loading);
    if (!m_probe.hasEngine()) {
        qCInfo(lcUi) << "playlist: waiting for the engine" << m_url;
        Q_EMIT engineNeeded();
        return;
    }
    m_probeId = m_probe.probe(m_url, true);
    qCInfo(lcUi) << "playlist: reading" << m_url << "probe" << m_probeId;
}

void PlaylistPage::openInfo(const core::MediaInfo& info)
{
    if (m_probeId != 0) {
        m_probe.cancel(m_probeId);
        m_probeId = 0;
    }
    m_info = info;
    if (m_url.isEmpty()) {
        m_url = !info.url.isEmpty() ? QUrl(info.url) : QUrl(u"https://www.youtube.com/playlist?list="_s + info.id);
    }
    fillHeader(info.title.isEmpty() ? tr("Playlist") : info.title, info.uploader, info.thumbnail,
               static_cast<int>(info.entries.size()), 0);
    populate();
    Q_EMIT hasPlaylistChanged(true);
}

void PlaylistPage::openInfo(const QUrl& playlistUrl, const core::MediaInfo& info)
{
    m_url = playlistUrl;
    m_model->clear();
    openInfo(info);
}

void PlaylistPage::showError(const QString& reason)
{
    m_probeId = 0;
    m_status->setText(reason);
    setState(State::Error);
}

void PlaylistPage::handleProbeFinished(quint64 id, const core::MediaInfo& info)
{
    if (id != m_probeId) {
        return;
    }
    m_probeId = 0;
    qCInfo(lcUi) << "playlist: read" << info.title << info.entries.size() << "entries";
    openInfo(info);
}

void PlaylistPage::handleProbeFailed(quint64 id, const QString& error)
{
    if (id != m_probeId) {
        return;
    }
    qCWarning(lcUi) << "playlist: could not read" << m_url << error;
    showError(tr("The playlist could not be read: %1").arg(error));
}

void PlaylistPage::setState(State state)
{
    m_state = state;
    const bool loading = state == State::Loading;
    busy::set(m_download, loading, tr("Reading"));
    if (!loading) {
        m_download->setEnabled(state == State::Ready && selectedCount() > 0);
    }
    m_playAll->setEnabled(!m_url.isEmpty() && state != State::Empty);
    m_copyLink->setEnabled(!m_url.isEmpty());
    switch (state) {
    case State::Idle:
        m_status->setText(tr("Open a playlist from Search or paste a link"));
        break;
    case State::Loading:
        m_status->setText(tr("Reading the playlist"));
        break;
    case State::Empty:
        m_status->setText(tr("This playlist is empty"));
        break;
    case State::Ready:
    case State::Error:
        break;
    }
    m_retry->setVisible(state == State::Error);
    m_stack->setCurrentWidget(state == State::Ready ? m_listPane : m_statusPane);
}

// ---- header ----------------------------------------------------------------

void PlaylistPage::fillHeader(const QString& title, const QString& channel, const QString& thumbnail, int count,
                              double totalDuration)
{
    m_titleText = title;
    elideTitle();
    m_channel->setText(channel);
    m_channel->setVisible(!channel.isEmpty());
    QString countText;
    if (count >= 0) {
        countText = itemWord(count);
        if (totalDuration > 0) {
            countText += u", "_s + core::formatDuration(totalDuration);
        }
    }
    m_count->setText(countText);
    m_thumbnailUrl = thumbnail;
    renderThumbnail();
}

void PlaylistPage::renderThumbnail()
{
    const bool dark = theme().isDark();
    const Tokens t = Tokens::forScheme(dark);
    const qreal dpr = devicePixelRatioF();
    QPixmap out(QSize(kThumbWidth, kThumbHeight) * dpr);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPixmap picture = m_thumbnailUrl.isEmpty() ? QPixmap() : m_thumbnails.get(m_thumbnailUrl);
    const QColor tile = dark ? t.muted.darker(175) : t.muted.lighter(130);
    thumbs::paintThumbnail(&painter, QRect(0, 0, kThumbWidth, kThumbHeight), picture, {tile, u"playlist"_s, 22}, dpr,
                           8);
    painter.end();
    m_thumb->setPixmap(out);
}

void PlaylistPage::elideTitle()
{
    const int width = std::max(80, m_title->width());
    m_title->setText(QFontMetrics(m_title->font()).elidedText(m_titleText, Qt::ElideRight, width));
    Q_EMIT titleChanged(m_titleText);
    m_title->setToolTip(m_titleText);
}

// ---- entries ---------------------------------------------------------------

QString PlaylistPage::expectedFolder() const
{
    const QString base = m_settings.downloadDirectory();
    if (!m_settings.organiseDownloads() || m_info.title.isEmpty()) {
        return base;
    }
    return QDir(base).filePath(core::sanitiseFolderName(m_info.title));
}

QStringList PlaylistPage::downloadedFiles() const
{
    return QDir(expectedFolder()).entryList(QDir::Files | QDir::NoDotAndDotDot);
}

void PlaylistPage::populate()
{
    m_syncing = true;
    m_model->setEntries(m_info.entries, downloadedFiles(), m_skipDownloaded);
    const int count = static_cast<int>(m_info.entries.size());
    m_from->setRange(1, std::max(1, count));
    m_to->setRange(1, std::max(1, count));
    m_range->setRange(1, std::max(1, count));
    m_from->setValue(1);
    m_to->setValue(std::max(1, count));
    m_range->setValues(1, std::max(1, count));
    m_filter->clear();
    m_syncing = false;
    fillHeader(m_info.title.isEmpty() ? tr("Playlist") : m_info.title, m_info.uploader, m_info.thumbnail, count,
               m_model->totalDuration());
    setState(count == 0 ? State::Empty : State::Ready);
    updateSelectionUi();
}

core::MediaInfo PlaylistPage::demoInfo()
{
    core::MediaInfo info;
    info.type = core::MediaInfo::Type::Playlist;
    info.id = u"PLdemo"_s;
    info.url = u"https://www.youtube.com/playlist?list=PLdemo"_s;
    info.title = u"Learn Qt in 12 videos"_s;
    info.uploader = u"Tutorials"_s;
    const QStringList titles{u"Getting started with Qt Widgets"_s,
                             u"Signals and slots, the whole story in one sitting with every pitfall covered"_s,
                             u"Layouts that survive a resize"_s,
                             u"[Private video]"_s,
                             u"Model, view, delegate"_s,
                             u"Style sheets without tears"_s,
                             u"Threads and workers"_s,
                             QString(), // the engine's shape for a removed video: no title, no duration
                             u"Settings and persistence"_s,
                             u"Testing widgets offscreen"_s,
                             u"Packaging for Linux"_s,
                             u"Shipping the app"_s};
    int i = 0;
    for (const QString& title : titles) {
        ++i;
        core::MediaEntry entry;
        entry.id = u"demo%1"_s.arg(i, 7, 10, QLatin1Char('0'));
        entry.url = u"https://www.youtube.com/watch?v="_s + entry.id;
        entry.title = title;
        const bool unavailable = PlaylistEntryDelegate::isUnavailableTitle(title);
        entry.uploader = unavailable ? QString() : u"Tutorials"_s;
        entry.duration = unavailable ? 0 : 300 + i * 95;
        info.entries << entry;
    }
    info.entryCount = static_cast<int>(info.entries.size());
    return info;
}

// ---- selection -------------------------------------------------------------

QList<int> PlaylistPage::visibleRows() const
{
    QList<int> rows;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        rows << m_proxy->mapToSource(m_proxy->index(row, 0)).row();
    }
    return rows;
}

QList<int> PlaylistPage::selectedIndexes() const
{
    return m_model->selectedIndexes();
}

int PlaylistPage::selectedCount() const
{
    return m_model->selectedCount();
}

void PlaylistPage::setAllChecked(bool checked)
{
    m_model->setRowsChecked(visibleRows(), checked, m_skipDownloaded);
}

void PlaylistPage::setRange(int from, int to)
{
    m_syncing = true;
    m_from->setValue(std::min(from, to));
    m_to->setValue(std::max(from, to));
    m_syncing = false;
    applyRange();
}

void PlaylistPage::applyRange()
{
    int from = m_from->value();
    int to = m_to->value();
    if (from > to) {
        std::swap(from, to);
        m_syncing = true;
        m_from->setValue(from);
        m_to->setValue(to);
        m_syncing = false;
    }
    m_range->setValues(from, to);
    m_syncing = true; // the spins already say what the selection will
    m_model->selectRange(from, to, m_skipDownloaded);
    m_syncing = false;
    updateSelectionUi();
}

void PlaylistPage::syncRangeToSelection()
{
    const QList<int> indexes = selectedIndexes();
    if (indexes.isEmpty() || m_syncing) {
        return;
    }
    m_syncing = true;
    m_from->setValue(indexes.first());
    m_to->setValue(indexes.last());
    m_range->setValues(indexes.first(), indexes.last());
    m_syncing = false;
}

void PlaylistPage::setFilter(const QString& text)
{
    if (m_filter->text() != text) {
        m_filter->setText(text);
        return; // textChanged brings us back here
    }
    m_proxy->setFilterFixedString(text.trimmed());
    updateSelectionUi();
}

void PlaylistPage::setSortOrder(SortOrder order)
{
    m_sortOrder = order;
    switch (order) {
    case SortOrder::PlaylistOrder:
        m_proxy->setSortRole(Role::IndexRole);
        m_proxy->sort(0, Qt::AscendingOrder);
        break;
    case SortOrder::Title:
        m_proxy->setSortRole(Role::TitleRole);
        m_proxy->sort(0, Qt::AscendingOrder);
        break;
    case SortOrder::Duration:
        m_proxy->setSortRole(Role::DurationRole);
        m_proxy->sort(0, Qt::DescendingOrder);
        break;
    }
    if (m_sort->currentData().toInt() != static_cast<int>(order)) {
        m_sort->setCurrentIndex(m_sort->findData(static_cast<int>(order)));
    }
}

void PlaylistPage::setSkipDownloaded(bool skip)
{
    m_skipDownloaded = skip;
    if (m_skip->isChecked() != skip) {
        m_skip->setChecked(skip);
    }
    if (skip) {
        m_model->uncheckDownloaded();
    }
    updateSelectionUi();
}

void PlaylistPage::updateSelectionUi()
{
    const int selected = selectedCount();
    const int total = m_model->rowCount();
    const double duration = m_model->selectedDuration();
    m_footer->setText(selected > 0 && duration > 0
                          ? tr("%1 of %2 selected, %3").arg(selected).arg(total).arg(core::formatDuration(duration))
                          : tr("%1 of %2 selected").arg(selected).arg(total));
    m_download->setText(selected > 0 ? tr("Download %1").arg(itemWord(selected)) : tr("Download"));
    if (!busy::isBusy(m_download)) {
        m_download->setEnabled(m_state == State::Ready && selected > 0);
    }

    // Select all reads the visible rows: every checkable one checked, none, or some.
    int checkable = 0;
    int checked = 0;
    for (const int row : visibleRows()) {
        if (m_model->isUnavailable(row) || (m_skipDownloaded && m_model->isDownloaded(row))) {
            continue;
        }
        ++checkable;
        if (m_model->isChecked(row)) {
            ++checked;
        }
    }
    m_selectAll->setEnabled(checkable > 0);
    m_selectAll->setCheckState(checked == 0 ? Qt::Unchecked
                                            : (checked == checkable ? Qt::Checked : Qt::PartiallyChecked));
    syncRangeToSelection();
}

// ---- rows ------------------------------------------------------------------

void PlaylistPage::handleRowAction(const QModelIndex& index, int action)
{
    const int row = m_proxy->mapToSource(index).row();
    const core::MediaEntry entry = m_model->entryAt(row);
    if (entry.url.isEmpty()) {
        return;
    }
    if (static_cast<PlaylistEntryDelegate::Action>(action) == PlaylistEntryDelegate::Action::Play) {
        Q_EMIT playRequested(QUrl(entry.url));
    } else {
        Q_EMIT videoDownloadRequested(entry);
    }
}

void PlaylistPage::setEngineStatus(const services::EngineManager::Status& status)
{
    using EngineState = services::EngineManager::State;
    if (m_state != State::Loading || (status.state != EngineState::Installing && status.state != EngineState::Updating)) {
        return;
    }
    QString text = tr("Setting up the download engine, a one-time step");
    if (!status.stepLabel.isEmpty()) {
        text += u"\n"_s + status.stepLabel;
        if (status.progress >= 0) {
            text += u" %1%"_s.arg(static_cast<int>(status.progress * 100));
        }
    }
    m_status->setText(text);
}

QString PlaylistPage::itemWord(int count) const
{
    if (core::isYouTubeHost(m_url.host())) {
        return count == 1 ? tr("1 video") : tr("%1 videos").arg(count);
    }
    return count == 1 ? tr("1 item") : tr("%1 items").arg(count);
}

QUrl PlaylistPage::playAllUrl() const
{
    // A watch link with the list plays the playlist; the bare playlist link
    // only shows its page.
    const QString listId = core::classifyYouTubeUrl(m_url).playlistId;
    for (const core::MediaEntry& entry : m_info.entries) {
        if (!entry.id.isEmpty() && !listId.isEmpty() && !PlaylistEntryDelegate::isUnavailableTitle(entry.title)) {
            return QUrl(u"https://www.youtube.com/watch?v=%1&list=%2"_s.arg(entry.id, listId));
        }
    }
    return m_url;
}

bool PlaylistPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_title && event->type() == QEvent::Resize) {
        elideTitle();
        return false;
    }
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        const auto* key = dynamic_cast<QKeyEvent*>(event);
        const QModelIndex current = m_list->currentIndex();
        if (current.isValid() && key->key() == Qt::Key_Space) {
            const int row = m_proxy->mapToSource(current).row();
            m_model->setChecked(row, !m_model->isChecked(row));
            return true;
        }
        if (current.isValid() && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
            handleRowAction(current, static_cast<int>(PlaylistEntryDelegate::Action::Play));
            return true;
        }
    }
    return Page::eventFilter(watched, event);
}

} // namespace pldl::ui
