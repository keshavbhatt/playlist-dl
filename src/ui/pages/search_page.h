#pragma once

#include "core/downloads/download_options.h"
#include "services/playlist_search.h"
#include "services/search_service.h"
#include "ui/pages/page.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

class QAbstractButton;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;

namespace pldl::core {
class Settings;
}
namespace pldl::services {
class SearchSuggestions;
}

namespace pldl::ui {

class BadgeLabel;
class SearchCardDelegate;
class ThumbnailCache;

/// The Search page, the home (DESIGN.md section 3, FEATURES B1 to B9): the
/// query field with suggestions and the recent-query chips, playlist cards in
/// a grid, the source chip that says when the engine answered instead of the
/// service (ADR-003). A pasted playlist or video link goes straight out as a
/// request; a card opens its playlist.
class SearchPage : public Page
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchPage)

public:
    enum class State
    {
        Empty,
        Loading,
        Results,
        Error,
        NoResults,
    };
    Q_ENUM(State)

    /// What a typed text is, besides a query (pure, see linkOf).
    enum class Link
    {
        None,        ///< a query
        Playlist,    ///< a playlist link or a bare list id
        Video,       ///< a video link without a playlist
        Unsupported, ///< a link that is neither
    };
    Q_ENUM(Link)

    SearchPage(core::Settings& settings, core::ThemeService& theme, ThumbnailCache& thumbnails,
               QWidget* parent = nullptr);
    ~SearchPage() override;

    /// Types `text` into the field and acts on it: a link is emitted, a
    /// query searched.
    void search(const QString& text);
    /// Puts `text` in the field as if typed, asking for suggestions but not
    /// searching (the debug hook, the tests).
    void typeQuery(const QString& text);
    void cancelSearch();
    /// The window's answer to engineNeeded once the engine is provisioned.
    void setEnginePaths(const core::EnginePaths& paths);
    void retryPending();
    /// Forces the engine for every search of this run (the debug hook).
    void setEngineOnly(bool engineOnly);
    /// Shows `results` as if a search had answered (the tests, the demo hook).
    void showResults(const QList<services::SearchResult>& results, bool hasMore,
                     services::PlaylistSearch::Source source);
    /// A canned result set for PLDL_DEBUG_OPEN=search-demo.
    [[nodiscard]] static QList<services::SearchResult> demoResults();

    /// Classifies a typed text (pure): the URL to emit lands in `url`.
    [[nodiscard]] static Link linkOf(const QString& text, QUrl* url = nullptr);

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] const QList<services::SearchResult>& results() const { return m_results; }
    [[nodiscard]] services::PlaylistSearch& playlistSearch() { return *m_search; }
    [[nodiscard]] services::SearchSuggestions& suggestions() { return *m_suggestions; }
    [[nodiscard]] QLineEdit* queryField() const { return m_field; }
    [[nodiscard]] QPushButton* searchButton() const { return m_button; }
    [[nodiscard]] BadgeLabel* sourceChip() const { return m_chip; }
    [[nodiscard]] QListView* resultsView() const { return m_list; }
    [[nodiscard]] QListWidget* suggestionsPopup() const { return m_popup; }
    [[nodiscard]] QWidget* recentRow() const { return m_recentRow; }
    [[nodiscard]] QPushButton* loadMoreButton() const { return m_loadMore; }
    [[nodiscard]] QPushButton* retryButton() const { return m_retry; }
    [[nodiscard]] QLabel* statusLabel() const { return m_status; }

Q_SIGNALS:
    /// A playlist link was pasted or a card chosen: the Playlist page opens it.
    void playlistRequested(const QUrl& url);
    /// A video link was pasted: the Browser page opens it.
    void videoRequested(const QUrl& url);
    /// The chosen card's result, with everything the Playlist page can show at once.
    void playlistChosen(const pldl::services::SearchResult& result);
    /// The engine is needed and not provisioned: the window sets it up and
    /// calls retryPending().
    void engineNeeded();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void onThemeChanged() override;

private:
    void buildHeader();
    void buildRecent();
    void buildPopup();
    void buildStage();
    void applyIcons();
    void setState(State state);
    void startSearch(const QString& query, int page);
    void handleFinished(quint64 id, const QList<services::SearchResult>& results, bool hasMore,
                        services::PlaylistSearch::Source source);
    void handleFailed(quint64 id, const QString& message);
    void setSource(services::PlaylistSearch::Source source);
    void appendResults(const QList<services::SearchResult>& results);
    void chooseRow(int row);
    void rebuildRecent();
    void showSuggestions(const QStringList& suggestions);
    void hideSuggestions();
    void moveSuggestion(int delta);
    void pickSuggestion();
    void layoutGrid();
    void placePopup();
    [[nodiscard]] static bool looksLikeLink(const QString& text);

    core::Settings& m_settings;
    ThumbnailCache& m_thumbnails;
    services::PlaylistSearch* m_search;
    services::SearchSuggestions* m_suggestions;
    QLineEdit* m_field = nullptr;
    QPushButton* m_button = nullptr;
    BadgeLabel* m_chip = nullptr;
    QWidget* m_recentRow = nullptr;
    QHBoxLayout* m_recentLayout = nullptr;
    QListWidget* m_popup = nullptr;
    QStackedWidget* m_stage = nullptr;
    QWidget* m_resultsPane = nullptr;
    QListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    SearchCardDelegate* m_delegate = nullptr;
    QPushButton* m_loadMore = nullptr;
    QWidget* m_emptyPane = nullptr;
    QLabel* m_brandMark = nullptr;
    QList<QAbstractButton*> m_exampleChips;
    QWidget* m_statusPane = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_retry = nullptr;
    State m_state = State::Empty;
    QList<services::SearchResult> m_results;
    quint64 m_searchId = 0;
    int m_page = 0;
    bool m_hasMore = false;
    QString m_query;
};

} // namespace pldl::ui
