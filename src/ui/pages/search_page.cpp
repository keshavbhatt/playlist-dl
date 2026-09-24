#include "ui/pages/search_page.h"

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "services/search_suggestions.h"
#include "ui/badge_label.h"
#include "ui/busy_button.h"
#include "ui/icons.h"
#include "ui/keyboard.h"
#include "ui/logging.h"
#include "ui/pldl_style.h"
#include "ui/search_card_delegate.h"
#include "ui/thumbnail_cache.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kBrandMark = 96;
constexpr int kPopupRows = 8;
constexpr int kPopupRowHeight = 30;
constexpr QLatin1StringView kPlaylistPage{"https://www.youtube.com/playlist?list="};

QToolButton* makeChip(const QString& text, QWidget* parent)
{
    auto* chip = new QToolButton(parent);
    chip->setProperty("pldlChip", true);
    chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setText(text);
    chip->setFocusPolicy(Qt::TabFocus);
    return chip;
}
} // namespace

SearchPage::SearchPage(core::Settings& settings, core::ThemeService& theme, ThumbnailCache& thumbnails,
                       QWidget* parent)
    : Page(tr("Search"), theme, parent)
    , m_settings(settings)
    , m_thumbnails(thumbnails)
    , m_search(new services::PlaylistSearch(settings, this))
    , m_suggestions(new services::SearchSuggestions(this))
{
    buildHeader();
    buildRecent();
    buildStage();
    buildPopup();

    connect(m_search, &services::PlaylistSearch::finished, this, &SearchPage::handleFinished);
    connect(m_search, &services::PlaylistSearch::failed, this, &SearchPage::handleFailed);
    connect(m_search, &services::PlaylistSearch::engineNeeded, this, [this] {
        setState(State::SettingUp); // the window sets the engine up; the page says so in place
        Q_EMIT engineNeeded();
    });
    connect(&m_search->engine(), &services::SearchService::playlistCounted, this, &SearchPage::setPlaylistCount);
    connect(m_suggestions, &services::SearchSuggestions::suggestions, this, &SearchPage::showSuggestions);
    connect(&m_thumbnails, &ThumbnailCache::ready, this,
            [this](const QString&) { m_list->viewport()->update(); });
    connect(&m_settings, &core::Settings::searchChanged, this, &SearchPage::rebuildRecent);
    connect(&m_settings, &core::Settings::searchChanged, this, [this] {
        if (!m_settings.searchSuggestions()) { // switched off while a popup or a request was up
            hideSuggestions();
            m_suggestions->cancel();
        }
    });

    rebuildRecent();
    setState(State::Empty);
    applyViewMode();
}

SearchPage::~SearchPage() = default;

// ---- build -----------------------------------------------------------------

void SearchPage::buildHeader()
{
    m_field = new QLineEdit(this);
    m_field->setObjectName(u"queryField"_s);
    m_field->setPlaceholderText(tr("Search YouTube playlists, or paste a playlist link from any site"));
    m_field->setAccessibleName(tr("Search query"));
    m_field->setClearButtonEnabled(true);
    m_field->installEventFilter(this);
    connect(m_field, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (!m_settings.searchSuggestions() || text.trimmed().isEmpty() || looksLikeLink(text)) {
            hideSuggestions();
            m_suggestions->cancel();
            return;
        }
        m_suggestions->request(text);
    });
    // The field takes the header's room: the stretch the base put after the
    // title gives way to it.
    headerLayout()->insertWidget(1, m_field, 1);
    headerLayout()->setStretch(2, 0);

    m_button = new QPushButton(tr("Search"), this);
    m_button->setObjectName(u"searchButton"_s);
    m_button->setProperty("pldlPrimary", true);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setDefault(false);
    m_button->setAutoDefault(false);
    connect(m_button, &QPushButton::clicked, this, [this] {
        if (busy::isBusy(m_button)) {
            cancelSearch();
            return;
        }
        search(m_field->text());
    });
    headerLayout()->addWidget(m_button);

    // Grid or list (FEATURES B10): two exclusive flat buttons, the choice kept
    // in Settings so it survives a restart and other views can follow it.
    auto makeViewButton = [this](const QString& name, const QString& tip) {
        auto* button = new QToolButton(this);
        button->setObjectName(name);
        button->setProperty("pldlFlat", true);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        button->setFocusPolicy(Qt::TabFocus);
        headerLayout()->addWidget(button);
        return button;
    };
    m_gridButton = makeViewButton(u"gridViewButton"_s, tr("Show results as cards"));
    m_gridButton->setChecked(true);
    m_listButton = makeViewButton(u"listViewButton"_s, tr("Show results as a list"));
    connect(m_gridButton, &QToolButton::clicked, this, [this] { m_settings.setSearchGridView(true); });
    connect(m_listButton, &QToolButton::clicked, this, [this] { m_settings.setSearchGridView(false); });
    connect(&m_settings, &core::Settings::searchChanged, this, &SearchPage::applyViewMode);
}

void SearchPage::buildRecent()
{
    m_recentRow = new QWidget(this);
    m_recentRow->setObjectName(u"recentRow"_s);
    m_recentLayout = new QHBoxLayout(m_recentRow);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(8);
    content()->addWidget(m_recentRow);
    m_recentRow->hide();
}

void SearchPage::buildStage()
{
    m_stage = new QStackedWidget(this);
    content()->addWidget(m_stage, 1);

    // Results: the grid and the Load more pill under it.
    m_resultsPane = new QWidget(this);
    auto* resultsLayout = new QVBoxLayout(m_resultsPane);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    resultsLayout->setSpacing(12);
    m_model = new QStandardItemModel(this);
    m_delegate = new SearchCardDelegate(m_thumbnails, theme(), this);
    m_list = new QListView(m_resultsPane);
    m_list->setObjectName(u"resultsView"_s);
    m_list->setAccessibleName(tr("Search results"));
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_delegate);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(0);
    m_list->setUniformItemSizes(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->viewport()->setAutoFillBackground(false);
    m_list->viewport()->installEventFilter(this);
    m_list->installEventFilter(this);
    connect(m_list, &QListView::clicked, this, [this](const QModelIndex& index) { chooseRow(index.row()); });
    connect(m_delegate, &SearchCardDelegate::stateChanged, this, [this] { m_list->viewport()->update(); });
    resultsLayout->addWidget(m_list, 1);
    auto* moreRow = new QHBoxLayout;
    moreRow->addStretch(1);
    m_loadMore = new QPushButton(tr("Load more"), m_resultsPane);
    m_loadMore->setObjectName(u"loadMoreButton"_s);
    m_loadMore->setCursor(Qt::PointingHandCursor);
    connect(m_loadMore, &QPushButton::clicked, this, [this] {
        if (!busy::isBusy(m_loadMore) && m_hasMore) {
            startSearch(m_query, m_page + 1);
        }
    });
    moreRow->addWidget(m_loadMore);
    moreRow->addStretch(1);
    resultsLayout->addLayout(moreRow);
    m_stage->addWidget(m_resultsPane);

    // Empty: the brand mark, the invitation, three example chips.
    m_emptyPane = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyPane);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(14);
    emptyLayout->addStretch(2);
    m_brandMark = new QLabel(m_emptyPane);
    m_brandMark->setAlignment(Qt::AlignCenter);
    m_brandMark->setAccessibleName(tr("Playlist Downloader"));
    emptyLayout->addWidget(m_brandMark);
    auto* invitation = new QLabel(tr("Search for a playlist or paste a link"), m_emptyPane);
    invitation->setObjectName(u"invitation"_s);
    invitation->setProperty("pldlTitle", true);
    invitation->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(invitation);
    auto* examples = new QWidget(m_emptyPane);
    examples->setObjectName(u"exampleChips"_s);
    auto* exampleLayout = new QHBoxLayout(examples);
    exampleLayout->setContentsMargins(0, 0, 0, 0);
    exampleLayout->setSpacing(8);
    exampleLayout->addStretch(1);
    for (const QString& example : {tr("lofi hip hop"), tr("python tutorial"), tr("workout music")}) {
        QToolButton* chip = makeChip(example, examples);
        chip->setAccessibleName(tr("Search for %1").arg(example));
        connect(chip, &QToolButton::clicked, this, [this, example] { search(example); });
        m_exampleChips << chip;
        exampleLayout->addWidget(chip);
    }
    exampleLayout->addStretch(1);
    keyboard::installArrowNavigation(examples);
    emptyLayout->addWidget(examples);
    emptyLayout->addStretch(3);
    m_stage->addWidget(m_emptyPane);

    // Loading, error and no results share one centred status pane; the
    // Retry pill shows for an error.
    m_statusPane = new QWidget(this);
    auto* statusLayout = new QVBoxLayout(m_statusPane);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(14);
    statusLayout->addStretch(2);
    m_status = new QLabel(m_statusPane);
    m_status->setObjectName(u"statusLabel"_s);
    m_status->setProperty("pldlMuted", true);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);
    statusLayout->addWidget(m_status);
    auto* retryRow = new QHBoxLayout;
    retryRow->addStretch(1);
    m_retry = new QPushButton(tr("Retry"), m_statusPane);
    m_retry->setObjectName(u"retryButton"_s);
    m_retry->setCursor(Qt::PointingHandCursor);
    connect(m_retry, &QPushButton::clicked, this, [this] { startSearch(m_query, 0); });
    retryRow->addWidget(m_retry);
    retryRow->addStretch(1);
    statusLayout->addLayout(retryRow);
    statusLayout->addStretch(3);
    m_stage->addWidget(m_statusPane);
}

void SearchPage::buildPopup()
{
    // A child of the page, not a popup window: the field keeps the focus and
    // the keys while the list shows under it.
    m_popup = new QListWidget(this);
    m_popup->setObjectName(u"suggestionsPopup"_s);
    m_popup->setAccessibleName(tr("Search suggestions"));
    m_popup->setProperty("pldlCard", true);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setSelectionMode(QAbstractItemView::SingleSelection);
    m_popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popup->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popup->setMouseTracking(true);
    m_popup->setCursor(Qt::PointingHandCursor);
    m_popup->hide();
    connect(m_popup, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_popup->setCurrentItem(item);
        pickSuggestion();
    });
    connect(m_popup, &QListWidget::itemEntered, this,
            [this](QListWidgetItem* item) { m_popup->setCurrentItem(item); });
}

void SearchPage::applyIcons()
{
    const Tokens t = Tokens::forScheme(theme().isDark());
    m_brandMark->setPixmap(icons::brand().pixmap(kBrandMark, kBrandMark));
    for (QAbstractButton* chip : std::as_const(m_exampleChips)) {
        chip->setIcon(icons::themed(u"sparkles"_s, t.accent));
    }
    for (QToolButton* chip : m_recentRow->findChildren<QToolButton*>()) {
        if (chip->property("pldlChip").toBool()) {
            chip->setIcon(icons::themed(u"clock"_s, t.muted));
        }
    }
    const bool grid = m_gridButton->isChecked();
    m_gridButton->setIcon(icons::themed(u"grid"_s, grid ? t.accent : t.muted));
    m_listButton->setIcon(icons::themed(u"list"_s, grid ? t.muted : t.accent));
    m_list->viewport()->update();
}

void SearchPage::onThemeChanged()
{
    applyIcons();
}

// ---- states ----------------------------------------------------------------

void SearchPage::setState(State state)
{
    m_state = state;
    switch (state) {
    case State::Empty:
        m_stage->setCurrentWidget(m_emptyPane);
        break;
    case State::Results:
        m_stage->setCurrentWidget(m_resultsPane);
        // The pane may have been hidden since the last resize: lay the grid
        // out against its real width once it is on screen.
        QTimer::singleShot(0, this, [this] { layoutGrid(); });
        break;
    case State::Loading:
        m_status->setText(tr("Searching"));
        m_retry->hide();
        m_stage->setCurrentWidget(m_statusPane);
        break;
    case State::Error:
        m_retry->show();
        m_stage->setCurrentWidget(m_statusPane);
        break;
    case State::NoResults:
        m_status->setText(tr("Nothing found for %1").arg(m_query));
        m_retry->hide();
        m_stage->setCurrentWidget(m_statusPane);
        break;
    case State::SettingUp:
        m_status->setText(tr("Setting up the download engine, a one-time step"));
        m_retry->hide();
        m_stage->setCurrentWidget(m_statusPane);
        break;
    }
    m_loadMore->setVisible(state == State::Results && m_hasMore);
}

// ---- searching -------------------------------------------------------------

bool SearchPage::looksLikeLink(const QString& text)
{
    const QString lower = text.trimmed().toLower();
    return lower.contains(u"http"_s) || lower.contains(u"www."_s);
}

SearchPage::Link SearchPage::linkOf(const QString& text, QUrl* url)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(u' ')) {
        return Link::None;
    }
    // A bare "list=PL..." or the id alone, as pasted from an address bar.
    static const QRegularExpression kBareId(u"^(?:list=)?((?:PL|UU|LL|FL|OLAK5uy_)[A-Za-z0-9_-]{10,})$"_s);
    if (const auto m = kBareId.match(trimmed); m.hasMatch()) {
        if (url != nullptr) {
            *url = QUrl(QString(kPlaylistPage) + m.captured(1));
        }
        return Link::Playlist;
    }
    if (!looksLikeLink(trimmed) && !trimmed.contains(u"youtu"_s)) {
        return Link::None;
    }
    const QUrl parsed = QUrl::fromUserInput(trimmed);
    const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(parsed);
    // A watch link inside a playlist is the playlist: the page lists it, the
    // video is one of its rows (2.x read any "list=" the same way).
    if (info.kind == core::YouTubeUrlKind::Playlist ||
        (info.kind == core::YouTubeUrlKind::Video && !info.playlistId.isEmpty())) {
        if (url != nullptr) {
            *url = QUrl(QString(kPlaylistPage) + info.playlistId);
        }
        return Link::Playlist;
    }
    if (info.kind == core::YouTubeUrlKind::Video) {
        if (url != nullptr) {
            *url = core::canonicalVideoUrl(parsed).value_or(parsed);
        }
        return Link::Video;
    }
    if (core::isDownloadable(parsed)) {
        if (url != nullptr) {
            *url = parsed;
        }
        return Link::Other; // any site: the engine decides
    }
    return Link::Unsupported;
}

void SearchPage::typeQuery(const QString& text)
{
    m_field->setFocus();
    m_field->setText(text);
    if (m_settings.searchSuggestions() && !text.trimmed().isEmpty() && !looksLikeLink(text)) {
        m_suggestions->request(text);
    }
}

void SearchPage::search(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (m_field->text() != trimmed) {
        m_field->setText(trimmed);
    }
    hideSuggestions();
    m_suggestions->cancel();
    if (trimmed.isEmpty()) {
        m_field->setFocus();
        return;
    }
    QUrl url;
    switch (linkOf(trimmed, &url)) {
    case Link::Playlist:
        qCInfo(lcUi) << "search: playlist link" << url.toString();
        Q_EMIT playlistRequested(url);
        return;
    case Link::Video:
        qCInfo(lcUi) << "search: video link" << url.toString();
        Q_EMIT videoRequested(url);
        return;
    case Link::Other:
        qCInfo(lcUi) << "search: link on another site" << url.toString();
        Q_EMIT linkRequested(url);
        return;
    case Link::Unsupported:
        cancelSearch();
        m_query = trimmed;
        m_status->setText(tr("That is not a link to a page with media."));
        setState(State::Error);
        m_retry->hide();
        return;
    case Link::None:
        break;
    }
    if (m_settings.keepSearchHistory()) {
        m_settings.addRecentQuery(trimmed);
    }
    startSearch(trimmed, 0);
}

void SearchPage::startSearch(const QString& query, int page)
{
    if (query.isEmpty()) {
        return;
    }
    if (page == 0) {
        m_query = query;
        m_results.clear();
        m_model->clear();
        m_hasMore = false;
        setState(State::Loading);
        busy::set(m_button, true, tr("Searching"), true);
    } else {
        busy::set(m_loadMore, true, tr("Loading"));
    }
    m_page = page;
    m_searchId = m_search->search(query, page);
}

void SearchPage::cancelSearch()
{
    m_search->cancel();
    m_searchId = 0;
    busy::set(m_button, false);
    busy::set(m_loadMore, false);
    if (m_state == State::Loading || m_state == State::SettingUp) {
        setState(m_results.isEmpty() ? State::Empty : State::Results);
    }
}

void SearchPage::handleFinished(quint64 id, const QList<services::SearchResult>& results, bool hasMore)
{
    if (id != m_searchId) {
        return;
    }
    m_searchId = 0;
    busy::set(m_button, false);
    busy::set(m_loadMore, false);
    if (m_page > 0 && results.isEmpty()) {
        // The last page was the last page after all.
        m_hasMore = false;
        setState(State::Results);
        return;
    }
    showResults(results, hasMore);
}

void SearchPage::handleFailed(quint64 id, const QString& message)
{
    if (id != m_searchId) {
        return;
    }
    m_searchId = 0;
    busy::set(m_button, false);
    busy::set(m_loadMore, false);
    qCWarning(lcUi) << "search failed:" << message;
    if (m_page > 0 && !m_results.isEmpty()) {
        // The results stay; the pill says why more did not come.
        m_loadMore->setToolTip(message);
        setState(State::Results);
        return;
    }
    m_status->setText(message);
    setState(State::Error);
}

void SearchPage::showResults(const QList<services::SearchResult>& results, bool hasMore)
{
    if (m_query.isEmpty()) {
        m_query = m_field->text().trimmed();
    }
    if (m_page == 0 || m_state != State::Results) {
        m_results.clear();
        m_model->clear();
    }
    m_hasMore = hasMore;
    m_loadMore->setToolTip(QString());
    appendResults(results);
    if (m_results.isEmpty()) {
        setState(State::NoResults);
        return;
    }
    setState(State::Results);
    layoutGrid();
}

void SearchPage::appendResults(const QList<services::SearchResult>& results)
{
    for (const services::SearchResult& result : results) {
        if (!result.isValid()) {
            continue;
        }
        // The service's pages overlap by an item now and then: one card each.
        const bool seen = std::any_of(m_results.cbegin(), m_results.cend(),
                                      [&result](const services::SearchResult& r) { return r.url == result.url; });
        if (seen) {
            continue;
        }
        m_results.append(result);
        auto* item = new QStandardItem;
        item->setData(result.title, SearchCardDelegate::TitleRole);
        item->setData(result.channel, SearchCardDelegate::ChannelRole);
        item->setData(result.thumbnailUrl, SearchCardDelegate::ThumbnailRole);
        item->setData(result.url, SearchCardDelegate::UrlRole);
        item->setData(result.itemCount, SearchCardDelegate::CountRole);
        item->setData(result.title, Qt::AccessibleTextRole);
        item->setToolTip(result.title);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_model->appendRow(item);
        if (result.itemCount < 0) {
            m_search->engine().countPlaylist(result.url);
        }
    }
}

void SearchPage::setPlaylistCount(const QString& url, qint64 count)
{
    for (int row = 0; row < m_results.size(); ++row) {
        if (m_results.at(row).url != url) {
            continue;
        }
        m_results[row].itemCount = count;
        if (QStandardItem* item = m_model->item(row)) {
            item->setData(count, SearchCardDelegate::CountRole);
        }
        break;
    }
}

void SearchPage::chooseRow(int row)
{
    if (row < 0 || row >= m_results.size()) {
        return;
    }
    const services::SearchResult result = m_results.at(row);
    qCInfo(lcUi) << "search: playlist chosen" << result.url;
    Q_EMIT playlistChosen(result);
    Q_EMIT playlistRequested(QUrl(result.url));
}

QList<services::SearchResult> SearchPage::demoResults()
{
    const struct
    {
        QLatin1StringView id;
        QLatin1StringView title;
        QLatin1StringView channel;
        qint64 count;
        QLatin1StringView video;
    } rows[] = {
        {"PLdemo01"_L1, "lofi hip hop radio: beats to relax and study to"_L1, "Lofi Girl"_L1, 120,
         "jfKfPfyJRdk"_L1},
        {"PLdemo02"_L1, "Chillhop Essentials: Winter 2025"_L1, "Chillhop Music"_L1, 24, "5yx6BWlEVcY"_L1},
        {"PLdemo03"_L1,
         "Late night jazz and lofi for long coding sessions with a very long title that wraps"_L1,
         "Study Beats"_L1, 8, "DWcJFNfaw9c"_L1},
        {"PLdemo04"_L1, "Rainy day lofi"_L1, "Ambient Room"_L1, -1, "lTRiuFIWV54"_L1},
        {"PLdemo05"_L1, "Focus playlist: deep work"_L1, "The Quiet Desk"_L1, 1, "4xDzrJKXOOY"_L1},
        {"PLdemo06"_L1, "Sleep sounds and slow beats"_L1, "Night Owl"_L1, 42, "rUxyKA_-grg"_L1},
    };
    QList<services::SearchResult> out;
    for (const auto& row : rows) {
        services::SearchResult result;
        result.kind = services::SearchKind::Playlists;
        result.id = QString(row.id);
        result.title = QString(row.title);
        result.channel = QString(row.channel);
        result.itemCount = row.count;
        result.thumbnailUrl = core::thumbnailUrl(QString(row.video)).toString();
        result.url = QString(kPlaylistPage) + result.id;
        out << result;
    }
    return out;
}

// ---- the engine ------------------------------------------------------------

void SearchPage::setEnginePaths(const core::EnginePaths& paths)
{
    m_search->setEnginePaths(paths);
}

void SearchPage::setEngineStatus(const services::EngineManager::Status& status)
{
    if (m_state != State::SettingUp) {
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

void SearchPage::retryPending()
{
    m_search->retryPending();
}

// ---- recent ----------------------------------------------------------------

void SearchPage::rebuildRecent()
{
    while (QLayoutItem* item = m_recentLayout->takeAt(0)) {
        // Clear rebuilds the row from its own click: the old chips leave the
        // row now and die once the event loop is back.
        if (QWidget* old = item->widget()) {
            old->hide();
            old->setParent(nullptr);
            old->deleteLater();
        }
        delete item;
    }
    const QStringList recent = m_settings.keepSearchHistory() ? m_settings.recentQueries() : QStringList();
    if (recent.isEmpty()) {
        m_recentRow->hide();
        return;
    }
    for (const QString& query : recent) {
        QToolButton* chip = makeChip(query, m_recentRow);
        chip->setAccessibleName(tr("Search again for %1").arg(query));
        connect(chip, &QToolButton::clicked, this, [this, query] { search(query); });
        m_recentLayout->addWidget(chip);
    }
    auto* clear = new QToolButton(m_recentRow);
    clear->setObjectName(u"clearRecentButton"_s);
    clear->setProperty("pldlFlat", true);
    clear->setText(tr("Clear"));
    clear->setToolTip(tr("Forget the recent searches"));
    clear->setAccessibleName(clear->toolTip());
    clear->setCursor(Qt::PointingHandCursor);
    clear->setFocusPolicy(Qt::TabFocus);
    connect(clear, &QToolButton::clicked, this, [this] { m_settings.clearRecentQueries(); });
    m_recentLayout->addWidget(clear);
    m_recentLayout->addStretch(1);
    keyboard::installArrowNavigation(m_recentRow);
    applyIcons();
    m_recentRow->show();
}

// ---- suggestions -----------------------------------------------------------

void SearchPage::showSuggestions(const QStringList& suggestions)
{
    if (suggestions.isEmpty() || !m_field->hasFocus() || looksLikeLink(m_field->text()) ||
        m_field->text().trimmed().isEmpty()) {
        hideSuggestions();
        return;
    }
    m_popup->clear();
    for (const QString& text : suggestions.mid(0, kPopupRows)) {
        auto* item = new QListWidgetItem(text, m_popup);
        item->setSizeHint(QSize(0, kPopupRowHeight));
    }
    m_popup->setCurrentRow(-1);
    placePopup();
    m_popup->raise();
    m_popup->show();
}

void SearchPage::hideSuggestions()
{
    m_popup->hide();
    m_popup->clear();
}

void SearchPage::placePopup()
{
    const QPoint below = m_field->mapTo(this, QPoint(0, m_field->height() + 4));
    const int rows = std::max(1, m_popup->count());
    m_popup->setGeometry(below.x(), below.y(), m_field->width(), rows * kPopupRowHeight + 10);
}

void SearchPage::moveSuggestion(int delta)
{
    const int count = m_popup->count();
    if (count == 0) {
        return;
    }
    const int current = m_popup->currentRow();
    int next = current + delta;
    if (next < -1) {
        next = count - 1;
    } else if (next >= count) {
        next = -1;
    }
    m_popup->setCurrentRow(next);
}

void SearchPage::pickSuggestion()
{
    QListWidgetItem* item = m_popup->currentItem();
    if (item == nullptr) {
        return;
    }
    const QString text = item->text();
    hideSuggestions();
    search(text);
}

// ---- events ----------------------------------------------------------------

bool SearchPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_field) {
        if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            const bool popupUp = m_popup->isVisible();
            switch (key->key()) {
            case Qt::Key_Down:
                if (popupUp) {
                    moveSuggestion(1);
                    return true;
                }
                break;
            case Qt::Key_Up:
                if (popupUp) {
                    moveSuggestion(-1);
                    return true;
                }
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (popupUp && m_popup->currentItem() != nullptr) {
                    pickSuggestion();
                } else {
                    search(m_field->text());
                }
                return true;
            case Qt::Key_Escape:
                if (popupUp) {
                    hideSuggestions();
                    m_suggestions->cancel();
                } else if (m_searchId != 0) {
                    cancelSearch();
                } else {
                    return false;
                }
                return true;
            default:
                break;
            }
        } else if (event->type() == QEvent::FocusOut) {
            hideSuggestions();
        } else if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            if (m_popup->isVisible()) {
                placePopup();
            }
        }
        return false;
    }
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space) &&
            m_list->currentIndex().isValid()) {
            chooseRow(m_list->currentIndex().row());
            return true;
        }
        return false;
    }
    if (watched == m_list->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            m_delegate->setHoverPos(static_cast<QMouseEvent*>(event)->pos());
        } else if (event->type() == QEvent::Leave) {
            m_delegate->setHoverPos(QPoint(-1, -1));
        } else if (event->type() == QEvent::Resize) {
            layoutGrid();
        }
    }
    return Page::eventFilter(watched, event);
}

void SearchPage::resizeEvent(QResizeEvent* event)
{
    Page::resizeEvent(event);
    layoutGrid();
    if (m_popup->isVisible()) {
        placePopup();
    }
}

void SearchPage::showEvent(QShowEvent* event)
{
    Page::showEvent(event);
    layoutGrid();
    if (m_state == State::Empty) {
        m_field->setFocus();
    }
}

void SearchPage::applyViewMode()
{
    const bool grid = m_settings.searchGridView();
    if (m_gridButton->isChecked() != grid) {
        m_gridButton->setChecked(grid);
        m_listButton->setChecked(!grid);
    }
    const auto layout = grid ? SearchCardDelegate::Layout::Grid : SearchCardDelegate::Layout::List;
    if (m_delegate->layout() != layout) {
        m_delegate->setLayout(layout);
        m_list->setViewMode(grid ? QListView::IconMode : QListView::ListMode);
        m_list->setFlow(grid ? QListView::LeftToRight : QListView::TopToBottom);
        m_list->setWrapping(grid);
        // A stale grid size from the other layout would size the rows wrong.
        m_list->setGridSize(QSize());
    }
    applyIcons();
    layoutGrid();
}

void SearchPage::layoutGrid()
{
    // The width the cards share is the same whether the scrollbar shows or
    // not, so the columns never jump when it appears.
    int width = m_list->viewport()->width();
    if (!m_list->verticalScrollBar()->isVisible()) {
        width -= m_list->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_list) + 2;
    }
    // The view keeps a few pixels for itself: without this slack a row that
    // fits by the maths wraps one card short (a scrollbar showing, 1166 px
    // viewport, four 291 px columns).
    width -= 4;
    if (width <= 0) {
        return;
    }
    constexpr int gap = SearchCardDelegate::kGap;
    if (m_delegate->layout() == SearchCardDelegate::Layout::List) {
        // One row per result, as wide as the view.
        const int rowWidth = width - gap;
        if (rowWidth == m_delegate->cardWidth() && m_list->gridSize() == m_delegate->itemSize()) {
            return;
        }
        m_delegate->setCardWidth(rowWidth);
        m_list->setGridSize(m_delegate->itemSize());
        m_list->doItemsLayout();
        return;
    }
    // Every item carries its gutter on the right and below, so a column is
    // a card plus a gap: as many as the minimum card allows, the cards
    // stretched to share the width, capped so a wide window gets more
    // columns instead of huge cards.
    const int columns = std::max(1, width / (SearchCardDelegate::kMinCardWidth + gap));
    const int cardWidth = std::min(SearchCardDelegate::kMaxCardWidth, width / columns - gap);
    qCDebug(lcUi) << "search grid: viewport" << m_list->viewport()->width() << "usable" << width << "columns" << columns
                  << "card" << cardWidth << "visible" << m_list->isVisible() << "pane" << m_resultsPane->isVisible();
    if (cardWidth == m_delegate->cardWidth() && m_list->gridSize() == m_delegate->itemSize()) {
        return;
    }
    m_delegate->setCardWidth(cardWidth);
    m_list->setGridSize(m_delegate->itemSize());
    m_list->doItemsLayout();
}

} // namespace pldl::ui
