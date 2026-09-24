#include "ui/pages/downloads_page.h"

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/badge_label.h"
#include "ui/download_card_delegate.h"
#include "ui/downloads_controller.h"
#include "ui/icons.h"
#include "ui/keyboard.h"
#include "ui/message_sheet.h"
#include "ui/pldl_style.h"
#include "ui/thumbnail_cache.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kEmptyMark = 64;
} // namespace

DownloadsPage::DownloadsPage(DownloadsController& controller, core::Settings& settings, core::ThemeService& theme,
                             QWidget* parent)
    : Page(tr("Downloads"), theme, parent)
    , m_controller(controller)
    , m_queue(controller.queue())
    , m_settings(settings)
{
    m_proxy = new DownloadsFilterProxy(this);
    m_proxy->setSourceModel(&m_queue);

    setupHeader();
    setupFilters();
    setupList();
    applyIcons();

    connect(&m_controller.engine(), &services::EngineManager::statusChanged, this, &DownloadsPage::setEngineStatus);
    setEngineStatus(m_controller.engine().status());

    // The counts read the queue itself; the proxy (connected to the queue
    // first) has already caught up when these run. The proxy does not emit
    // rowsRemoved for every removal in a run of them, so its signals are not
    // what the counts wait for.
    connect(&m_queue, &QAbstractItemModel::rowsInserted, this, &DownloadsPage::updateCounts);
    connect(&m_queue, &QAbstractItemModel::rowsRemoved, this, &DownloadsPage::updateCounts);
    connect(&m_queue, &QAbstractItemModel::modelReset, this, &DownloadsPage::updateCounts);
    connect(&m_queue, &QAbstractItemModel::dataChanged, this, [this] { updateCounts(); });
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, [this] { updateCounts(); });
    connect(&m_queue, &core::DownloadQueue::activeCountChanged, this, [this](int) { updateCounts(); });
    connect(&m_controller.thumbnails(), &ThumbnailCache::ready, m_list->viewport(),
            qOverload<>(&QWidget::update));
    updateCounts();
}

void DownloadsPage::setupHeader()
{
    m_engineChip = new BadgeLabel(theme(), this);
    m_engineChip->setObjectName(u"engineChip"_s);
    m_engineChip->setCursor(Qt::PointingHandCursor);
    m_engineChip->setAccessibleName(tr("Download engine status; opens the engine setup"));
    m_engineChip->installEventFilter(this);
    headerLayout()->insertWidget(1, m_engineChip); // right after the title

    m_pauseAll = new QPushButton(tr("Pause all"), this);
    m_pauseAll->setObjectName(u"pauseAllButton"_s);
    m_pauseAll->setProperty("pldlFlat", true);
    m_pauseAll->setCursor(Qt::PointingHandCursor);
    connect(m_pauseAll, &QPushButton::clicked, &m_queue, &core::DownloadQueue::pauseAll);
    headerLayout()->addWidget(m_pauseAll);

    m_resumeAll = new QPushButton(tr("Resume all"), this);
    m_resumeAll->setObjectName(u"resumeAllButton"_s);
    m_resumeAll->setProperty("pldlFlat", true);
    m_resumeAll->setCursor(Qt::PointingHandCursor);
    connect(m_resumeAll, &QPushButton::clicked, &m_queue, &core::DownloadQueue::resumeAll);
    headerLayout()->addWidget(m_resumeAll);

    m_clear = new QToolButton(this);
    m_clear->setObjectName(u"clearButton"_s);
    m_clear->setProperty("pldlFlat", true);
    m_clear->setText(tr("Clear"));
    m_clear->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clear->setPopupMode(QToolButton::InstantPopup);
    m_clear->setCursor(Qt::PointingHandCursor);
    m_clear->setToolTip(tr("Remove finished or failed downloads from the list"));
    m_clear->setAccessibleName(tr("Clear the list"));
    m_clearMenu = new QMenu(m_clear);
    m_clearFinished = m_clearMenu->addAction(tr("Clear finished"), this, &DownloadsPage::clearFinished);
    m_clearFailed = m_clearMenu->addAction(tr("Clear failed"), this, &DownloadsPage::clearFailed);
    m_clearAll = m_clearMenu->addAction(tr("Clear all finished and failed"), this, &DownloadsPage::clearAllFinished);
    m_clear->setMenu(m_clearMenu);
    headerLayout()->addWidget(m_clear);

    m_openFolder = new QPushButton(tr("Open folder"), this);
    m_openFolder->setObjectName(u"openFolderButton"_s);
    m_openFolder->setProperty("pldlFlat", true);
    m_openFolder->setCursor(Qt::PointingHandCursor);
    m_openFolder->setToolTip(tr("Open the download folder"));
    connect(m_openFolder, &QPushButton::clicked, &m_controller, &DownloadsController::openDownloadFolder);
    headerLayout()->addWidget(m_openFolder);
}

void DownloadsPage::setupFilters()
{
    auto* row = new QWidget(this);
    row->setObjectName(u"filterRow"_s);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* group = new QButtonGroup(this);
    int id = 0;
    for (const QString& name : {tr("All"), tr("Active"), tr("Finished"), tr("Failed")}) {
        auto* chip = new QToolButton(row);
        chip->setProperty("pldlChip", true);
        chip->setText(name);
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        group->addButton(chip, id++);
        layout->addWidget(chip);
        m_filters << chip;
    }
    m_filters.first()->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this, [this](int which) { setFilter(static_cast<Filter>(which)); });
    keyboard::installArrowNavigation(row);
    layout->addStretch(1);
    m_count = new QLabel(row);
    m_count->setObjectName(u"countLabel"_s);
    m_count->setProperty("pldlMuted", true);
    layout->addWidget(m_count);
    content()->addWidget(row);
}

void DownloadsPage::setupList()
{
    m_stack = new QStackedWidget(this);

    m_list = new QListView(m_stack);
    m_list->setObjectName(u"downloadsList"_s);
    m_list->setAccessibleName(tr("Downloads"));
    m_list->setModel(m_proxy);
    m_delegate = new DownloadCardDelegate(m_controller.thumbnails(), theme(), this);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setMouseTracking(true);
    m_list->setUniformItemSizes(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setSpacing(0);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setStyleSheet(u"QListView { background: transparent; }"_s);
    m_list->installEventFilter(this);
    connect(m_delegate, &DownloadCardDelegate::actionTriggered, &m_controller, &DownloadsController::handleCardAction);
    connect(m_delegate, &DownloadCardDelegate::repaintNeeded, m_list->viewport(), qOverload<>(&QWidget::update));
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.data(core::DownloadQueue::FilePathRole).toString().isEmpty()) {
            m_controller.handleCardAction(index.data(core::DownloadQueue::IdRole).toULongLong(),
                                          DownloadCardDelegate::Action::Open);
        }
    });
    m_stack->addWidget(m_list);

    auto* empty = new QWidget(m_stack);
    empty->setObjectName(u"emptyState"_s);
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(32, 0, 32, 0);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch(2);
    m_emptyMark = new QLabel(empty);
    m_emptyMark->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyMark);
    m_emptyTitle = new QLabel(tr("Downloads you start will show up here"), empty);
    m_emptyTitle->setObjectName(u"emptyTitle"_s);
    m_emptyTitle->setProperty("pldlTitle", true);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyTitle->setWordWrap(true);
    emptyLayout->addWidget(m_emptyTitle);
    m_emptyBody = new QLabel(tr("Pick videos on a playlist page and press Download"), empty);
    m_emptyBody->setObjectName(u"emptyBody"_s);
    m_emptyBody->setProperty("pldlMuted", true);
    m_emptyBody->setAlignment(Qt::AlignCenter);
    m_emptyBody->setWordWrap(true);
    emptyLayout->addWidget(m_emptyBody);
    emptyLayout->addStretch(3);
    m_stack->addWidget(empty);

    content()->addWidget(m_stack, 1);
}

void DownloadsPage::applyIcons()
{
    const Tokens t = Tokens::forScheme(theme().isDark());
    m_pauseAll->setIcon(icons::themed(u"pause"_s, t.text, t.muted));
    m_resumeAll->setIcon(icons::themed(u"play"_s, t.text, t.muted));
    m_clear->setIcon(icons::themed(u"trash"_s, t.text, t.muted));
    m_openFolder->setIcon(icons::themed(u"folder"_s, t.text, t.muted));
    m_emptyMark->setPixmap(icons::appIcon().pixmap(kEmptyMark, kEmptyMark));
}

void DownloadsPage::onThemeChanged()
{
    applyIcons();
    m_list->viewport()->update();
}

// ---- engine chip -----------------------------------------------------------

void DownloadsPage::setEngineStatus(const services::EngineManager::Status& status)
{
    m_engine = status;
    using State = services::EngineManager::State;
    QString text;
    QString glyph = u"engine"_s;
    BadgeLabel::Tone tone = BadgeLabel::Tone::Accent;
    switch (status.state) {
    case State::Unknown:
        text = tr("Download engine");
        break;
    case State::NotInstalled:
        text = tr("Engine missing");
        glyph = u"warning"_s;
        tone = BadgeLabel::Tone::Warning;
        break;
    case State::Installing:
    case State::Updating:
        text = tr("Setting up");
        if (status.progress >= 0) {
            text += u" %1%"_s.arg(static_cast<int>(status.progress * 100));
        }
        glyph = u"loader"_s;
        break;
    case State::Ready:
        if (status.updateAvailable) {
            text = tr("Update available");
            glyph = u"download"_s;
            tone = BadgeLabel::Tone::Warning;
        } else {
            text = status.ytdlpVersion.isEmpty() ? tr("Download engine ready")
                                                 : tr("Download engine %1").arg(status.ytdlpVersion);
            glyph = u"check"_s;
            tone = BadgeLabel::Tone::Ok;
        }
        break;
    case State::Error:
        text = tr("Engine error");
        glyph = u"warning"_s;
        tone = BadgeLabel::Tone::Danger;
        break;
    }
    m_engineChip->setText(text);
    m_engineChip->setGlyph(glyph);
    m_engineChip->setTone(tone);
    m_engineChip->setToolTip(
        status.error.isEmpty()
            ? tr("Download engine %1, media converter %2. Click for the engine setup.")
                  .arg(status.ytdlpVersion.isEmpty() ? tr("missing") : status.ytdlpVersion,
                       status.ffmpegPath.isEmpty() ? tr("missing") : tr("ready"))
            : status.error);
}

// ---- filters and counts ----------------------------------------------------

void DownloadsPage::setFilter(Filter filter)
{
    m_proxy->setFilter(filter);
    m_filters.at(static_cast<int>(filter))->setChecked(true);
    updateCounts();
}

void DownloadsPage::updateCounts()
{
    const int active = DownloadsFilterProxy::countFor(m_queue, Filter::Active);
    const int finished = DownloadsFilterProxy::countFor(m_queue, Filter::Finished);
    const int failed = DownloadsFilterProxy::countFor(m_queue, Filter::Failed);
    QStringList parts;
    if (active > 0) {
        parts << tr("%n active", nullptr, active);
    }
    if (finished > 0) {
        parts << tr("%n finished", nullptr, finished);
    }
    if (failed > 0) {
        parts << tr("%n failed", nullptr, failed);
    }
    m_count->setText(parts.join(u", "_s));
    m_count->setVisible(!parts.isEmpty());

    int paused = 0;
    for (const core::DownloadJob& job : m_queue.jobs()) {
        if (job.state == core::DownloadState::Paused) {
            ++paused;
        }
    }
    m_pauseAll->setEnabled(m_queue.activeCount() > 0);
    m_resumeAll->setEnabled(paused > 0);
    m_clearFinished->setEnabled(finished > 0);
    m_clearFailed->setEnabled(failed > 0);
    m_clearAll->setEnabled(finished + failed > 0);
    m_clear->setEnabled(finished + failed > 0);

    const bool empty = m_proxy->rowCount() == 0;
    m_stack->setCurrentIndex(empty ? 1 : 0);
    if (empty) {
        const bool nothingAtAll = m_queue.rowCount() == 0;
        m_emptyTitle->setText(nothingAtAll ? tr("Downloads you start will show up here") : tr("Nothing here"));
        m_emptyBody->setText(nothingAtAll ? tr("Pick videos on a playlist page and press Download")
                                          : tr("No downloads match this filter"));
    }
}

QString DownloadsPage::countText() const
{
    return m_count->text();
}

bool DownloadsPage::isEmptyStateVisible() const
{
    return m_stack->currentIndex() == 1;
}

int DownloadsPage::visibleCount() const
{
    return m_proxy->rowCount();
}

// ---- clearing --------------------------------------------------------------

void DownloadsPage::removeInStates(const QList<core::DownloadState>& states)
{
    QList<quint64> ids;
    for (const core::DownloadJob& job : m_queue.jobs()) {
        if (states.contains(job.state)) {
            ids << job.id;
        }
    }
    for (const quint64 id : ids) {
        m_queue.remove(id);
    }
}

void DownloadsPage::clearFinished()
{
    removeInStates({core::DownloadState::Completed});
}

void DownloadsPage::clearFailed()
{
    removeInStates({core::DownloadState::Failed, core::DownloadState::Cancelled});
}

void DownloadsPage::clearAllFinished()
{
    m_queue.clearFinished();
}

// ---- keyboard and the chip -------------------------------------------------

void DownloadsPage::openCurrent()
{
    const QModelIndex current = m_list->currentIndex();
    if (current.isValid() && !current.data(core::DownloadQueue::FilePathRole).toString().isEmpty()) {
        m_controller.handleCardAction(current.data(core::DownloadQueue::IdRole).toULongLong(),
                                      DownloadCardDelegate::Action::Open);
    }
}

void DownloadsPage::removeCurrent()
{
    const QModelIndex current = m_list->currentIndex();
    if (!current.isValid()) {
        return;
    }
    const quint64 id = current.data(core::DownloadQueue::IdRole).toULongLong();
    const auto state = current.data(core::DownloadQueue::StateRole).value<core::DownloadState>();
    if (core::isFinishedState(state)) {
        m_controller.handleCardAction(id, DownloadCardDelegate::Action::Remove);
        return;
    }
    if (m_removeSheet != nullptr) {
        return;
    }
    auto* sheet = new MessageSheet(window(), MessageSheet::Tone::Danger, tr("Remove this download?"),
                                   tr("It has not finished. Removing it stops the transfer; whatever has "
                                      "arrived stays in the download folder."));
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    sheet->addButton(tr("Keep"));
    sheet->addButton(tr("Remove"), MessageSheet::Role::Destructive);
    connect(sheet, &QDialog::finished, this, [this, sheet, id](int) {
        if (sheet->clickedIndex() == 1) {
            m_controller.handleCardAction(id, DownloadCardDelegate::Action::Remove);
        }
    });
    m_removeSheet = sheet;
    sheet->open();
}

bool DownloadsPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_engineChip && event->type() == QEvent::MouseButtonRelease) {
        if (dynamic_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            Q_EMIT engineSetupRequested();
            return true;
        }
    }
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        const auto* key = dynamic_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            openCurrent();
            return true;
        }
        if (key->key() == Qt::Key_Delete) {
            removeCurrent();
            return true;
        }
    }
    return Page::eventFilter(watched, event);
}

} // namespace pldl::ui
