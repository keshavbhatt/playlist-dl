#include "ui/supported_sites_sheet.h"

#include "core/theme/theme_service.h"
#include "ui/a11y.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {

constexpr int kSheetWidth = 560;
constexpr int kRowHeight = 36;
constexpr int kOpenWidth = 56;

/// Paints the entry, and "Open" at the right of the hovered or selected row.
class SiteRowDelegate : public QStyledItemDelegate
{
public:
    SiteRowDelegate(core::ThemeService& theme, QObject* parent)
        : QStyledItemDelegate(parent)
        , m_theme(theme)
    {
    }

    static QRect openRect(const QRect& row) { return QRect(row.right() - kOpenWidth - 4, row.top(), kOpenWidth, row.height()); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const Tokens t = Tokens::forScheme(m_theme.isDark());
        const bool active = option.state.testFlag(QStyle::State_Selected) || option.state.testFlag(QStyle::State_MouseOver);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (active) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(t.hover);
            painter->drawRoundedRect(option.rect.adjusted(2, 1, -2, -1), 8, 8);
        }
        painter->setPen(t.text);
        const QRect textRect = option.rect.adjusted(12, 0, -(active ? kOpenWidth + 12 : 12), 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, textRect.width()));
        if (active) {
            painter->setPen(t.link);
            painter->drawText(openRect(option.rect), Qt::AlignVCenter | Qt::AlignRight, tr("Open"));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, kRowHeight); }

private:
    core::ThemeService& m_theme;
};

} // namespace

SupportedSitesSheet::SupportedSitesSheet(const services::SupportedSites& sites, core::ThemeService& theme,
                                         QWidget* parent)
    : QDialog(parent)
    , m_sites(sites)
    , m_theme(theme)
{
    setWindowTitle(tr("Supported sites"));
    setModal(true); // a sheet: modal to the window and to the accessibility tree
    setMinimumWidth(kSheetWidth);
    setupUi();
    connect(&m_sites, &services::SupportedSites::loaded, this, &SupportedSitesSheet::rebuild); // an engine update
    a11y::nameControlsFromLabels(this);
    resize(kSheetWidth, 560);
    m_filter->setFocus();
}

void SupportedSitesSheet::setupUi()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(12);

    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    auto* icon = new QLabel(this);
    icon->setPixmap(icons::pixmap(u"globe"_s, t.accent, 22, devicePixelRatioF()));
    head->addWidget(icon);
    auto* title = new QLabel(tr("Supported sites"), this);
    title->setProperty("pldlTitle", true);
    head->addWidget(title, 1);
    root->addLayout(head);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(u"siteFilter"_s);
    m_filter->setPlaceholderText(tr("Filter sites"));
    m_filter->setAccessibleName(tr("Filter sites"));
    m_filter->setClearButtonEnabled(true);
    m_filter->addAction(icons::themed(u"search"_s, t.muted), QLineEdit::LeadingPosition);
    m_filter->installEventFilter(this); // Down moves into the list
    connect(m_filter, &QLineEdit::textChanged, this, &SupportedSitesSheet::rebuild);
    root->addWidget(m_filter);

    auto* meta = new QHBoxLayout;
    m_count = new QLabel(this);
    m_count->setObjectName(u"siteCount"_s);
    m_count->setProperty("pldlMuted", true);
    meta->addWidget(m_count, 1);
    m_version = new QLabel(this);
    m_version->setObjectName(u"siteVersion"_s);
    m_version->setProperty("pldlMuted", true);
    meta->addWidget(m_version);
    root->addLayout(meta);

    auto* card = new QFrame(this);
    card->setProperty("pldlCard", true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(4, 4, 4, 4);
    m_list = new QListWidget(card);
    m_list->setObjectName(u"siteList"_s);
    m_list->setAccessibleName(tr("Sites"));
    m_list->setItemDelegate(new SiteRowDelegate(m_theme, m_list));
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->viewport()->installEventFilter(this); // a click on Open
    connect(m_list, &QListWidget::itemDoubleClicked, this, &SupportedSitesSheet::openRow);
    connect(m_list, &QListWidget::itemActivated, this, &SupportedSitesSheet::openRow); // Enter
    cardLayout->addWidget(m_list, 1);
    root->addWidget(card, 1);

    auto* note = new QLabel(
        tr("Names come from the download engine. Open takes the site's home page to the built-in browser. "
           "A site that is missing may still work through the generic downloader."),
        this);
    note->setProperty("pldlMuted", true);
    note->setWordWrap(true);
    root->addWidget(note);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setProperty("pldlPrimary", true);
    close->setDefault(false);
    close->setAutoDefault(false);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    root->addLayout(buttons);

    rebuild();
}

void SupportedSitesSheet::setFilter(const QString& text)
{
    m_filter->setText(text);
}

QString SupportedSitesSheet::filter() const
{
    return m_filter->text().trimmed();
}

int SupportedSitesSheet::shownCount() const
{
    return m_list->count();
}

void SupportedSitesSheet::rebuild()
{
    const QString needle = filter();
    m_list->clear();
    QSet<QString> shownSites;
    for (const services::SupportedSite& entry : m_sites.sites()) {
        const QString display = entry.display();
        if (!needle.isEmpty() && !display.contains(needle, Qt::CaseInsensitive) &&
            !entry.raw.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, entry.site);
        item->setToolTip(tr("Open %1 in the browser").arg(entry.site));
        shownSites.insert(entry.site.toLower());
    }
    const int total = m_sites.siteCount();
    if (!m_sites.isLoaded()) {
        m_count->setText(tr("The download engine is not ready yet"));
    } else if (needle.isEmpty()) {
        m_count->setText(tr("%Ln sites", nullptr, total));
    } else {
        m_count->setText(tr("%L1 of %L2 sites").arg(shownSites.size()).arg(total));
    }
    m_version->setText(m_sites.version().isEmpty() ? QString()
                                                   : tr("From the download engine, version %1").arg(m_sites.version()));
}

void SupportedSitesSheet::openRow(QListWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }
    const QUrl url = services::SupportedSites::homePage(item->data(Qt::UserRole).toString());
    if (url.isValid()) {
        Q_EMIT openSiteRequested(url);
    }
}

bool SupportedSitesSheet::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down && m_list->count() > 0) {
            m_list->setFocus();
            m_list->setCurrentRow(0);
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && m_list->count() == 1) {
            openRow(m_list->item(0)); // one match: Enter opens it
            return true;
        }
    }
    if (watched == m_list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        QListWidgetItem* item = m_list->itemAt(mouse->pos());
        if (item != nullptr && mouse->button() == Qt::LeftButton &&
            SiteRowDelegate::openRect(m_list->visualItemRect(item)).contains(mouse->pos())) {
            openRow(item);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

} // namespace pldl::ui
