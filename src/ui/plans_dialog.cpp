#include "ui/plans_dialog.h"

#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {

using Tier = services::LicenseService::Tier;

/// A plan card: name (and badge), the one-line price, the check list and the
/// full-width action at the bottom.
QFrame* planCard(const QString& name, const QString& badge, const QString& price, const QStringList& items,
                 QPushButton* action, const Tokens& t, qreal dpr, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setProperty("pldlCard", true);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(18, 16, 18, 16);
    v->setSpacing(8);
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    auto* title = new QLabel(name, card);
    title->setProperty("pldlTitle", true);
    head->addWidget(title);
    if (!badge.isEmpty()) {
        auto* label = new QLabel(badge, card);
        label->setProperty("pldlBadge", true);
        label->setProperty("pldlPro", true);
        head->addWidget(label);
    }
    head->addStretch(1);
    v->addLayout(head);
    auto* priceLabel = new QLabel(price, card);
    priceLabel->setProperty("pldlMuted", true);
    v->addWidget(priceLabel);
    v->addSpacing(4);
    const QPixmap check = icons::pixmap(u"check"_s, t.success, 16, dpr);
    for (const QString& item : items) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* glyph = new QLabel(card);
        glyph->setPixmap(check);
        glyph->setFixedSize(16, 16);
        row->addWidget(glyph, 0, Qt::AlignTop);
        row->addWidget(new QLabel(item, card), 1);
        v->addLayout(row);
    }
    v->addStretch(1);
    v->addSpacing(8);
    action->setParent(card);
    action->setCursor(Qt::PointingHandCursor);
    v->addWidget(action);
    return card;
}

} // namespace

QStringList accountFreeItems()
{
    return {QObject::tr("Search YouTube playlists"), QObject::tr("Play in the built-in browser, signed in"),
            QObject::tr("Up to %1 downloads a day").arg(services::LicenseService::kFreeDownloadsPerDay),
            QObject::tr("Every quality up to 4K and lossless audio"),
            QObject::tr("Subtitles and thumbnails embedded")};
}

QStringList accountProItems()
{
    return {QObject::tr("Everything in Free"), QObject::tr("No daily limit: unlimited downloads"),
            QObject::tr("Whole playlists in one go, however long"),
            QObject::tr("Keeps the app maintained")};
}

PlansDialog::PlansDialog(services::LicenseService& license, const core::ThemeService& theme, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Plans"));
    setModal(true);
    setMinimumWidth(680);
    const Tokens t = Tokens::forScheme(theme.isDark());
    const Tier current = license.tier();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* cards = new QHBoxLayout;
    cards->setSpacing(14);
    auto* freeAction = new QPushButton(current == Tier::Pro ? tr("Close") : tr("Continue with Free"));
    freeAction->setProperty("pldlFlat", true);
    connect(freeAction, &QPushButton::clicked, this, &QDialog::accept);
    cards->addWidget(planCard(tr("Free"), QString(), tr("Free"), accountFreeItems(), freeAction, t,
                              devicePixelRatioF(), this),
                     1);
    auto* proAction = new QPushButton(current == Tier::Pro ? tr("Your plan") : tr("Buy Pro"));
    proAction->setProperty("pldlPrimary", true);
    proAction->setEnabled(current != Tier::Pro);
    connect(proAction, &QPushButton::clicked, this, [url = license.checkoutUrl()] { platform::openUrl(url); });
    QFrame* pro = planCard(tr("Pro"), u"PRO"_s, tr("One-time purchase"), accountProItems(), proAction, t,
                           devicePixelRatioF(), this);
    // The accent outline of the mock: the card that is recommended.
    pro->setStyleSheet(u"QFrame[pldlCard=\"true\"] { border: 2px solid %1; }"_s.arg(t.accent.name()));
    cards->addWidget(pro, 1);
    root->addLayout(cards);

    auto* note = new QLabel(tr("Purchases are tied to your account id and work on every computer you use."), this);
    note->setProperty("pldlMuted", true);
    note->setWordWrap(true);
    root->addWidget(note);

    auto* buttons = new QHBoxLayout;
    auto* restore = new QPushButton(tr("Restore purchase"), this);
    restore->setProperty("pldlFlat", true);
    restore->setCursor(Qt::PointingHandCursor);
    restore->setToolTip(tr("Check the licence with the server again"));
    restore->setEnabled(!license.checking());
    connect(restore, &QPushButton::clicked, this, [&license] { license.refresh(); });
    connect(&license, &services::LicenseService::checkingChanged, restore,
            [restore](bool checking) { restore->setEnabled(!checking); });
    buttons->addWidget(restore);
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

} // namespace pldl::ui
