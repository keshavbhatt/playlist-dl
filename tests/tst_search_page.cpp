// The Search page (DESIGN.md section 3, FEATURES B1 to B9) offscreen: the
// empty state, links typed into the field going out as requests instead of
// searches, canned results rendered as cards with the source chip, the
// recent-query chips, and a search that needs the engine waiting for it.
// Offline: the service and the suggestions point at a port nobody answers.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/search_suggestions.h"
#include "ui/badge_label.h"
#include "ui/busy_button.h"
#include "ui/pages/search_page.h"
#include "ui/search_card_delegate.h"
#include "ui/thumbnail_cache.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using namespace pldl::services;
using namespace pldl::ui;

namespace {
const QUrl kDeadEndpoint(u"http://127.0.0.1:1/nobody"_s);

QList<SearchResult> canned(int count)
{
    QList<SearchResult> out;
    for (int i = 0; i < count; ++i) {
        SearchResult result;
        result.kind = SearchKind::Playlists;
        result.id = u"PL%1"_s.arg(i);
        result.title = u"Playlist %1"_s.arg(i);
        result.channel = u"Channel %1"_s.arg(i);
        result.itemCount = i * 10;
        result.url = u"https://www.youtube.com/playlist?list=PL%1"_s.arg(i);
        out << result;
    }
    return out;
}
} // namespace

class TestSearchPage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aCountedPlaylistUpdatesItsCard()
    {
        QList<SearchResult> results = canned(2);
        results[0].itemCount = -1;
        m_page->showResults(results, false);
        QCOMPARE(m_page->resultsView()->model()->index(0, 0).data(SearchCardDelegate::CountRole).toLongLong(), -1);
        m_page->setPlaylistCount(results[0].url, 42);
        QCOMPARE(m_page->results().at(0).itemCount, 42);
        QCOMPARE(m_page->resultsView()->model()->index(0, 0).data(SearchCardDelegate::CountRole).toLongLong(), 42);
        m_page->setPlaylistCount(u"https://example.test/unknown"_s, 7); // no such card: nothing changes
        QCOMPARE(m_page->results().at(1).itemCount, results[1].itemCount);
    }

    void viewToggleSwitchesBetweenCardsAndRows()
    {
        QVERIFY(m_page->gridViewButton()->isChecked());
        QCOMPARE(m_page->resultsView()->viewMode(), QListView::IconMode);
        QTest::mouseClick(m_page->listViewButton(), Qt::LeftButton);
        QVERIFY(m_page->listViewButton()->isChecked());
        QVERIFY(!m_settings->searchGridView());
        QCOMPARE(m_page->resultsView()->viewMode(), QListView::ListMode);
        QCOMPARE(m_page->resultsView()->flow(), QListView::TopToBottom);
        // The choice is a setting: changing it elsewhere moves the toggle.
        m_settings->setSearchGridView(true);
        QVERIFY(m_page->gridViewButton()->isChecked());
        QCOMPARE(m_page->resultsView()->viewMode(), QListView::IconMode);
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"search.ini"_s));
        m_theme = std::make_unique<ThemeService>(*m_settings);
        m_thumbnails = std::make_unique<ThumbnailCache>();
        m_page = std::make_unique<SearchPage>(*m_settings, *m_theme, *m_thumbnails);
        // Nothing leaves the machine: the suggestions are a dead end and there is no engine.
        m_page->suggestions().setEndpoint(kDeadEndpoint);
        m_page->resize(1000, 640);
        m_page->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_page.get()));
    }

    void cleanup()
    {
        m_page.reset();
        m_thumbnails.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void startsEmptyWithTheInvitation()
    {
        QCOMPARE(m_page->state(), SearchPage::State::Empty);
        QVERIFY(m_page->findChild<QLabel*>(u"invitation"_s)->isVisible());
        QWidget* examples = m_page->findChild<QWidget*>(u"exampleChips"_s);
        QVERIFY(examples != nullptr);
        QCOMPARE(examples->findChildren<QToolButton*>().size(), 3);
        QVERIFY(!m_page->recentRow()->isVisible());
        QVERIFY(!m_page->suggestionsPopup()->isVisible());
        QVERIFY(!m_page->queryField()->placeholderText().isEmpty());
        QVERIFY(!m_page->searchButton()->text().isEmpty());
        QVERIFY(!busy::isBusy(m_page->searchButton()));
    }

    void linksAreClassified()
    {
        QUrl url;
        QCOMPARE(SearchPage::linkOf(u"lofi hip hop"_s, &url), SearchPage::Link::None);
        QCOMPARE(SearchPage::linkOf(u"https://www.youtube.com/playlist?list=PLabc123def456"_s, &url),
                 SearchPage::Link::Playlist);
        QCOMPARE(url.toString(), u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QCOMPARE(SearchPage::linkOf(u"youtube.com/watch?v=jNQXAC9IVRw&list=PLabc123def456"_s, &url),
                 SearchPage::Link::Playlist);
        QCOMPARE(url.toString(), u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QCOMPARE(SearchPage::linkOf(u"list=PLabc123def456"_s, &url), SearchPage::Link::Playlist);
        QCOMPARE(url.toString(), u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QCOMPARE(SearchPage::linkOf(u"PLabc123def456xyz"_s, &url), SearchPage::Link::Playlist);
        QCOMPARE(SearchPage::linkOf(u"https://youtu.be/jNQXAC9IVRw"_s, &url), SearchPage::Link::Video);
        QCOMPARE(url.toString(), u"https://www.youtube.com/watch?v=jNQXAC9IVRw"_s);
        // A mix is not a listable playlist: the video alone.
        QCOMPARE(
            SearchPage::linkOf(u"https://www.youtube.com/watch?v=jNQXAC9IVRw&list=RDjNQXAC9IVRw"_s, &url),
            SearchPage::Link::Video);
        QCOMPARE(SearchPage::linkOf(u"https://example.com/watch?v=jNQXAC9IVRw"_s, &url),
                 SearchPage::Link::Unsupported);
        QCOMPARE(SearchPage::linkOf(u"https://www.youtube.com/"_s, &url), SearchPage::Link::Unsupported);
        QCOMPARE(SearchPage::linkOf(u"playlist"_s, &url), SearchPage::Link::None);
    }

    void typingALinkShowsNoSuggestions()
    {
        QLineEdit* field = m_page->queryField();
        field->setFocus();
        QTest::keyClicks(field, u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QVERIFY(!m_page->suggestions().isPending());
        QVERIFY(!m_page->suggestionsPopup()->isVisible());
        field->clear();
        QTest::keyClicks(field, u"lofi"_s);
        QVERIFY(m_page->suggestions().isPending()); // a query does ask (the dead end stays silent)
        QTest::keyClicks(field, u" www.example"_s);
        QVERIFY(!m_page->suggestions().isPending());
    }

    void suggestionsFollowTheSetting()
    {
        QLineEdit* field = m_page->queryField();
        field->setFocus();
        QTest::keyClicks(field, u"lofi"_s);
        QVERIFY(m_page->suggestions().isPending());
        m_settings->setSearchSuggestions(false); // switched off mid-request: dropped
        QVERIFY(!m_page->suggestions().isPending());
        QTest::keyClicks(field, u" beats"_s);
        QVERIFY(!m_page->suggestions().isPending());
        m_page->typeQuery(u"jazz"_s);
        QVERIFY(!m_page->suggestions().isPending());
        m_settings->setSearchSuggestions(true);
        QTest::keyClicks(field, u"y"_s);
        QVERIFY(m_page->suggestions().isPending());
    }

    void aPastedPlaylistLinkGoesOutAsARequest()
    {
        QSignalSpy playlist(m_page.get(), &SearchPage::playlistRequested);
        QSignalSpy video(m_page.get(), &SearchPage::videoRequested);
        QLineEdit* field = m_page->queryField();
        field->setFocus();
        field->setText(u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QTest::keyClick(field, Qt::Key_Return);
        QCOMPARE(playlist.size(), 1);
        QCOMPARE(playlist.first().at(0).toUrl().toString(),
                 u"https://www.youtube.com/playlist?list=PLabc123def456"_s);
        QCOMPARE(m_page->state(), SearchPage::State::Empty); // no search ran
        QVERIFY(!busy::isBusy(m_page->searchButton()));
        QVERIFY(m_settings->recentQueries().isEmpty()); // links are not history

        m_page->search(u"https://youtu.be/jNQXAC9IVRw"_s);
        QCOMPARE(video.size(), 1);
        QCOMPARE(video.first().at(0).toUrl().toString(), u"https://www.youtube.com/watch?v=jNQXAC9IVRw"_s);
        QCOMPARE(playlist.size(), 1);

        m_page->search(u"https://example.com/nothing"_s);
        QCOMPARE(m_page->state(), SearchPage::State::Error);
        QVERIFY(!m_page->retryButton()->isVisible());
    }

    void cannedResultsShowCardsAndTheChip()
    {
        QSignalSpy chosen(m_page.get(), &SearchPage::playlistChosen);
        QSignalSpy requested(m_page.get(), &SearchPage::playlistRequested);
        m_page->showResults(canned(5), true);
        QCOMPARE(m_page->state(), SearchPage::State::Results);
        QCOMPARE(m_page->results().size(), 5);
        QCOMPARE(m_page->resultsView()->model()->rowCount(), 5);
        QVERIFY(m_page->resultsView()->isVisible());
        QVERIFY(m_page->loadMoreButton()->isVisible());
        const QModelIndex first = m_page->resultsView()->model()->index(0, 0);
        QCOMPARE(first.data(SearchCardDelegate::TitleRole).toString(), u"Playlist 0"_s);
        QCOMPARE(first.data(SearchCardDelegate::CountRole).toLongLong(), 0);
        QCOMPARE(SearchCardDelegate::countText(-1), QString());
        QCOMPARE(SearchCardDelegate::countText(1), u"1 video"_s);
        QCOMPARE(SearchCardDelegate::countText(12), u"12 videos"_s);

        // A click on a card chooses its playlist.
        const QModelIndex third = m_page->resultsView()->model()->index(2, 0);
        const QRect rect = m_page->resultsView()->visualRect(third);
        QVERIFY(rect.isValid());
        QTest::mouseClick(m_page->resultsView()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
        QCOMPARE(chosen.size(), 1);
        QCOMPARE(chosen.first().at(0).value<SearchResult>().id, u"PL2"_s);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().at(0).toUrl().toString(), u"https://www.youtube.com/playlist?list=PL2"_s);

        // The service source: no chip, no Load more.
        m_page->showResults(canned(2), false);
        QCOMPARE(m_page->resultsView()->model()->rowCount(), 2);
        QVERIFY(!m_page->loadMoreButton()->isVisible());

        // Nothing: the no-results state names the query.
        m_page->queryField()->setText(u"zzz"_s);
        m_page->showResults({}, false);
        QCOMPARE(m_page->state(), SearchPage::State::NoResults);
        QVERIFY(m_page->statusLabel()->isVisible());
        QVERIFY(!m_page->retryButton()->isVisible());
    }

    void aSearchWithoutTheEngineWaitsForIt()
    {
        QSignalSpy needed(m_page.get(), &SearchPage::engineNeeded);
        m_page->search(u"lofi hip hop"_s);
        QCOMPARE(m_page->state(), SearchPage::State::Loading);
        QVERIFY(busy::isBusy(m_page->searchButton()));
        QVERIFY(m_page->statusLabel()->isVisible());
        QCOMPARE(m_settings->recentQueries(), QStringList{u"lofi hip hop"_s});
        QVERIFY(m_page->recentRow()->isVisible());
        // The service is a dead end: the engine is asked for and the search waits.
        QTRY_COMPARE_WITH_TIMEOUT(needed.size(), 1, 5000);
        QVERIFY(m_page->playlistSearch().hasPending());
        QCOMPARE(m_page->state(), SearchPage::State::Loading);
        // Still no engine: the search fails and offers Retry.
        m_page->retryPending();
        QCOMPARE(m_page->state(), SearchPage::State::Error);
        QVERIFY(m_page->retryButton()->isVisible());
        QVERIFY(!busy::isBusy(m_page->searchButton()));

        // Escape while searching cancels; the page goes back to empty.
        m_page->search(u"second"_s);
        QCOMPARE(m_page->state(), SearchPage::State::Loading);
        m_page->queryField()->setFocus();
        QTest::keyClick(m_page->queryField(), Qt::Key_Escape);
        QCOMPARE(m_page->state(), SearchPage::State::Empty);
        QVERIFY(!busy::isBusy(m_page->searchButton()));
        QVERIFY(!m_page->playlistSearch().isSearching());
    }

    void recentChipsFollowTheSetting()
    {
        m_settings->addRecentQuery(u"one"_s);
        m_settings->addRecentQuery(u"two"_s);
        QVERIFY(m_page->recentRow()->isVisible());
        QList<QToolButton*> chips;
        for (QToolButton* button : m_page->recentRow()->findChildren<QToolButton*>()) {
            if (button->property("pldlChip").toBool()) {
                chips << button;
            }
        }
        QCOMPARE(chips.size(), 2);
        QCOMPARE(chips.first()->text(), u"two"_s);
        // A chip searches again for its query.
        chips.first()->click();
        QCOMPARE(m_page->queryField()->text(), u"two"_s);
        QCOMPARE(m_page->state(), SearchPage::State::Loading);
        m_page->cancelSearch();
        // Clear forgets them; the row goes.
        auto* clear = m_page->recentRow()->findChild<QToolButton*>(u"clearRecentButton"_s);
        QVERIFY(clear != nullptr);
        clear->click();
        QVERIFY(m_settings->recentQueries().isEmpty());
        QVERIFY(!m_page->recentRow()->isVisible());
        // History off hides the row even with queries stored.
        m_settings->addRecentQuery(u"three"_s);
        QVERIFY(m_page->recentRow()->isVisible());
        m_settings->setKeepSearchHistory(false);
        QVERIFY(!m_page->recentRow()->isVisible());
        m_page->search(u"four"_s);
        QVERIFY(!m_settings->recentQueries().contains(u"four"_s));
        m_page->cancelSearch();
    }

    void demoResultsRender()
    {
        QList<SearchResult> demo = SearchPage::demoResults();
        QVERIFY(demo.size() >= 4);
        for (SearchResult& result : demo) {
            QVERIFY(result.isValid());
            QCOMPARE(result.kind, SearchKind::Playlists);
            QVERIFY(!result.thumbnailUrl.isEmpty());
            result.thumbnailUrl.clear(); // offline: nothing is fetched while painting
        }
        m_page->showResults(demo, true);
        QCOMPARE(m_page->resultsView()->model()->rowCount(), demo.size());
        m_settings->setTheme(Theme::Dark);
        QTest::qWait(50);
        m_settings->setTheme(Theme::Light);
        QTest::qWait(50);
        QCOMPARE(m_page->state(), SearchPage::State::Results);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<ThemeService> m_theme;
    std::unique_ptr<ThumbnailCache> m_thumbnails;
    std::unique_ptr<SearchPage> m_page;
};

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-search-page"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestSearchPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_search_page.moc"
