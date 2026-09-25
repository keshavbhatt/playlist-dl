#pragma once

#include "services/engine_manager.h"
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
class QToolButton;
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
        SettingUp, ///< the engine is being set up for the first search (inline, no dialog)
    };
    Q_ENUM(State)

    /// What a typed text is, besides a query (pure, see linkOf).
    enum class Link
    {
        None,        ///< a query
        Playlist,    ///< a playlist link or a bare list id
        Video,       ///< a video link without a playlist
        Other,       ///< a web link on any site: the engine says what it is
        Unsupported, ///< not a web link
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
    /// Back to the clean page: no query, no results, the invitation and today's examples
    /// (owner, 2026-09-25). The field's clear button, an empty search and Esc all do it.
    void resetToEmpty();
    /// The window's answer to engineNeeded once the engine is provisioned.
    void setEnginePaths(const core::EnginePaths& paths);
    /// The engine's progress while the page waits for it (State::SettingUp).
    void setEngineStatus(const services::EngineManager::Status& status);
    void retryPending();
    /// Forces the engine for every search of this run (the debug hook).
    /// Shows `results` as if a search had answered (the tests, the demo hook).
    void showResults(const QList<services::SearchResult>& results, bool hasMore);
    /// A canned result set for PLDL_DEBUG_OPEN=search-demo.
    [[nodiscard]] static QList<services::SearchResult> demoResults();

    /// Classifies a typed text (pure): the URL to emit lands in `url`.
    [[nodiscard]] static Link linkOf(const QString& text, QUrl* url = nullptr);

    /// A playlist's size arrived (the engine's flat search carries none; the
    /// count is asked for per result): the card's pill updates.
    void setPlaylistCount(const QString& url, qint64 count);
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] const QList<services::SearchResult>& results() const { return m_results; }
    [[nodiscard]] services::PlaylistSearch& playlistSearch() { return *m_search; }
    [[nodiscard]] services::SearchSuggestions& suggestions() { return *m_suggestions; }
    /// The services behind the empty state's example chips: three seeds a day, one
    /// completion each, so the chips differ. Tests point them at a local server.
    void setIdeasEndpoint(const QUrl& endpoint);
    [[nodiscard]] bool ideasPending() const;
    /// Asks once for today's example searches; the fixed three stay until an answer comes.
    void refreshExamples();
    /// Replaces the example chips (empty list: back to the fixed three).
    void setExamples(const QStringList& examples);
    [[nodiscard]] QLineEdit* queryField() const { return m_field; }
    [[nodiscard]] QPushButton* searchButton() const { return m_button; }
    [[nodiscard]] QListView* resultsView() const { return m_list; }
    [[nodiscard]] QToolButton* gridViewButton() const { return m_gridButton; }
    [[nodiscard]] QToolButton* listViewButton() const { return m_listButton; }
    [[nodiscard]] QListWidget* suggestionsPopup() const { return m_popup; }
    [[nodiscard]] QWidget* recentRow() const { return m_recentRow; }
    [[nodiscard]] QPushButton* loadMoreButton() const { return m_loadMore; }
    [[nodiscard]] QPushButton* retryButton() const { return m_retry; }
    [[nodiscard]] QLabel* statusLabel() const { return m_status; }

Q_SIGNALS:
    /// A playlist link was pasted or a card chosen: the Playlist page opens it.
    void playlistRequested(const QUrl& url);
    /// A video link was pasted: the window opens the Download options sheet.
    void videoRequested(const QUrl& url);
    /// A link on any other site was pasted: the window probes it (a playlist
    /// opens the Playlist page, a single item the options sheet).
    void linkRequested(const QUrl& url);
    /// The empty state's "Supported sites" link.
    void supportedSitesRequested();
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
    void handleFinished(quint64 id, const QList<services::SearchResult>& results, bool hasMore);
    void handleFailed(quint64 id, const QString& message);
    void appendResults(const QList<services::SearchResult>& results);
    void chooseRow(int row);
    void rebuildRecent();
    void showSuggestions(const QStringList& suggestions);
    void hideSuggestions();
    void moveSuggestion(int delta);
    void pickSuggestion();
    void layoutGrid();
    void applyViewMode();
    void placePopup();
    [[nodiscard]] static bool looksLikeLink(const QString& text);

    core::Settings& m_settings;
    ThumbnailCache& m_thumbnails;
    services::PlaylistSearch* m_search;
    services::SearchSuggestions* m_suggestions;
    QList<services::SearchSuggestions*> m_ideas; ///< one per seed
    QStringList m_ideaPicks;                     ///< one completion per seed, in seed order
    QStringList m_ideaSeeds;                     ///< today's seeds, in the same order
    QWidget* m_examplesRow = nullptr;
    bool m_examplesAsked = false;
    QLineEdit* m_field = nullptr;
    QPushButton* m_button = nullptr;
    QToolButton* m_gridButton = nullptr;
    QToolButton* m_listButton = nullptr;
    QWidget* m_recentRow = nullptr;
    QHBoxLayout* m_recentLayout = nullptr;
    QListWidget* m_popup = nullptr;
    QStackedWidget* m_stage = nullptr;
    QWidget* m_resultsPane = nullptr;
    QListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    SearchCardDelegate* m_delegate = nullptr;
    QPushButton* m_loadMore = nullptr;
    QLabel* m_resultsCount = nullptr;
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
