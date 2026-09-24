#include "ui/account_dialog.h"

#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/plans_dialog.h"
#include "ui/pldl_style.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

AccountDialog::AccountDialog(services::LicenseService& license, const core::ThemeService& theme,
                             QWidget* parent)
    : QDialog(parent)
    , m_license(license)
    , m_theme(theme)
{
    setWindowTitle(tr("Account"));
    setModal(false);
    setMinimumWidth(640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(16);
    root->addWidget(buildAccountCard());
    root->addWidget(buildPlanCard());
    root->addWidget(buildBanner());

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(close);
    root->addLayout(buttons);

    connect(&m_license, &services::LicenseService::statusChanged, this, &AccountDialog::refresh);
    connect(&m_license, &services::LicenseService::checkingChanged, this, [this](bool) { refresh(); });
    refresh();
}

QWidget* AccountDialog::buildAccountCard()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    auto* card = new QFrame(this);
    card->setProperty("pldlCard", true);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(18, 16, 18, 16);
    v->setSpacing(10);
    auto* heading = new QLabel(tr("Your account"), card);
    heading->setProperty("pldlTitle", true);
    v->addWidget(heading);

    // The key-value list of the mock: Account id, Plan, Status, Devices.
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);
    auto key = [card](const QString& text) {
        auto* label = new QLabel(text, card);
        label->setProperty("pldlMuted", true);
        return label;
    };
    grid->addWidget(key(tr("Account id")), 0, 0);
    auto* idRow = new QWidget(card);
    auto* idLayout = new QHBoxLayout(idRow);
    idLayout->setContentsMargins(0, 0, 0, 0);
    idLayout->setSpacing(8);
    m_id = new QLabel(idRow);
    QFont mono = m_id->font();
    mono.setFamilies({u"JetBrains Mono"_s, u"DejaVu Sans Mono"_s, u"monospace"_s});
    m_id->setFont(mono);
    m_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    idLayout->addWidget(m_id);
    m_copy = new QPushButton(idRow);
    m_copy->setProperty("pldlFlat", true);
    m_copy->setFixedSize(32, 32); // room for the focus ring (28 clipped it, owner bug)
    m_copy->setIconSize(QSize(16, 16));
    m_copy->setCursor(Qt::PointingHandCursor);
    m_copy->setToolTip(tr("Copy the account id"));
    m_copy->setAccessibleName(tr("Copy the account id"));
    m_copy->setIcon(icons::themed(u"copy"_s, t.muted));
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_license.accountId());
        m_copy->setToolTip(tr("Copied"));
        QTimer::singleShot(1500, m_copy, [this] { m_copy->setToolTip(tr("Copy the account id")); });
    });
    idLayout->addWidget(m_copy);
    idLayout->addStretch(1);
    grid->addWidget(idRow, 0, 1);

    grid->addWidget(key(tr("Plan")), 1, 0);
    m_planBadge = new QLabel(card);
    m_planBadge->setProperty("pldlBadge", true);
    grid->addWidget(m_planBadge, 1, 1, Qt::AlignLeft);
    grid->addWidget(key(tr("Status")), 2, 0);
    m_status = new QLabel(card);
    grid->addWidget(m_status, 2, 1);
    // Owner request: the facts behind the status, when they exist.
    m_expiryKey = key(tr("Expires"));
    grid->addWidget(m_expiryKey, 3, 0);
    m_expiry = new QLabel(card);
    grid->addWidget(m_expiry, 3, 1);
    m_nextKey = key(tr("Next check"));
    grid->addWidget(m_nextKey, 4, 0);
    m_next = new QLabel(card);
    grid->addWidget(m_next, 4, 1);
    grid->addWidget(key(tr("Devices")), 5, 0);
    m_devices = new QLabel(tr("This computer"), card);
    grid->addWidget(m_devices, 5, 1);
    grid->setColumnStretch(1, 1);
    v->addLayout(grid);
    m_note = new QLabel(card);
    m_note->setProperty("pldlMuted", true);
    m_note->setWordWrap(true);
    m_note->hide();
    v->addWidget(m_note);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_restore = new QPushButton(tr("Restore purchase"), card);
    m_restore->setCursor(Qt::PointingHandCursor);
    m_restore->setToolTip(tr("Check the licence with the server again"));
    m_restore->setIcon(icons::themed(u"retry"_s, t.text));
    connect(m_restore, &QPushButton::clicked, this, [this] { m_license.refresh(); });
    actions->addWidget(m_restore);
    m_portal = new QPushButton(tr("Self-service portal"), card);
    m_portal->setCursor(Qt::PointingHandCursor);
    m_portal->setToolTip(tr("Invoices, payment details and the subscription, on the web"));
    m_portal->setIcon(icons::themed(u"external"_s, t.text));
    connect(m_portal, &QPushButton::clicked, this, [this] { platform::openUrl(m_license.portalUrl()); });
    actions->addWidget(m_portal);
    actions->addStretch(1);
    v->addLayout(actions);
    return card;
}

QWidget* AccountDialog::buildPlanCard()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    auto* card = new QFrame(this);
    card->setProperty("pldlCard", true);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(18, 16, 18, 16);
    v->setSpacing(10);
    // One line and See plans (owner: the always visible comparison looked
    // odd); the Free and Pro columns live in the plans sheet.
    Q_UNUSED(t);
    auto* heading = new QLabel(tr("Plan"), card);
    heading->setProperty("pldlTitle", true);
    v->addWidget(heading);
    m_planSummary = new QLabel(card);
    m_planSummary->setProperty("pldlMuted", true);
    m_planSummary->setWordWrap(true);
    v->addWidget(m_planSummary);
    auto* actions = new QHBoxLayout;
    auto* plans = new QPushButton(tr("See plans"), card);
    plans->setProperty("pldlPrimary", true);
    plans->setCursor(Qt::PointingHandCursor);
    connect(plans, &QPushButton::clicked, this, &AccountDialog::showPlans);
    actions->addWidget(plans);
    actions->addStretch(1);
    v->addLayout(actions);
    return card;
}

QWidget* AccountDialog::buildBanner()
{
    // mocks/account.html: shown during the evaluation (and, an extension,
    // once it has ended) with the days left and Upgrade.
    m_banner = new QFrame(this);
    m_banner->setProperty("pldlBanner", true);
    m_banner->setProperty("pldlWarn", true);
    auto* h = new QHBoxLayout(m_banner);
    h->setContentsMargins(14, 10, 14, 10);
    h->setSpacing(10);
    m_bannerIcon = new QLabel(m_banner);
    m_bannerIcon->setFixedSize(18, 18);
    h->addWidget(m_bannerIcon);
    m_bannerText = new QLabel(m_banner);
    m_bannerText->setWordWrap(true);
    m_bannerText->setTextFormat(Qt::RichText);
    h->addWidget(m_bannerText, 1);
    m_upgrade = new QPushButton(tr("Upgrade"), m_banner);
    m_upgrade->setProperty("pldlPrimary", true);
    m_upgrade->setCursor(Qt::PointingHandCursor);
    connect(m_upgrade, &QPushButton::clicked, this, [this] { platform::openUrl(m_license.checkoutUrl()); });
    h->addWidget(m_upgrade);
    return m_banner;
}

void AccountDialog::refresh()
{
    using Tier = services::LicenseService::Tier;
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const auto& st = m_license.status();
    m_id->setText(m_license.accountId().isEmpty() ? tr("…") : m_license.accountId());
    m_copy->setEnabled(!m_license.accountId().isEmpty());
    m_restore->setEnabled(!m_license.checking());
    m_bannerIcon->setPixmap(icons::pixmap(u"clock"_s, t.warning, 18, devicePixelRatioF()));

    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto dateText = [](qint64 utc) {
        return QLocale().toString(QDateTime::fromSecsSinceEpoch(utc).toLocalTime().date(), QLocale::LongFormat);
    };
    // Plurals by hand: "%n" forms only resolve through a loaded translation.
    auto daysLeft = [](int n) { return n == 1 ? tr("1 day left") : tr("%1 days left").arg(n); };
    // "verified today" of the mock, made true: the server's last answer.
    QString verified;
    if (const qint64 last = m_license.lastCheckedUtc(); last > 0) {
        const qint64 days = QDateTime::fromSecsSinceEpoch(last).toLocalTime().date().daysTo(now.toLocalTime().date());
        verified = days <= 0 ? tr("verified today")
                   : days == 1 ? tr("verified yesterday")
                               : tr("verified %1 days ago").arg(days);
    } else {
        verified = tr("not verified yet");
    }
    if (st.fromNetworkError) {
        verified = tr("the licence server could not be reached, %1").arg(verified);
    }
    const bool lifetime = st.licenseType.contains(u"lifetime"_s, Qt::CaseInsensitive)
                          || !(st.hasExpiryDate && st.expiryTimestamp > 0);

    QString badge;
    bool pro = false;
    QString status;
    QString expiryKey;
    QString expiry;
    QString next;
    QString bannerText;
    QStringList notes;
    if (m_license.checking() && m_license.accountId().isEmpty()) {
        badge = tr("Checking");
        status = tr("Checking your licence…");
    } else {
        switch (m_license.tier()) {
        case Tier::Pro: {
            badge = u"PRO"_s;
            pro = true;
            status = tr("Active, %1").arg(verified);
            expiryKey = tr("Expires");
            if (lifetime) {
                expiry = st.licenseType.isEmpty() ? tr("Never") : tr("Never, %1 licence").arg(st.licenseType);
            } else {
                const int left = st.hasDaysRemaining
                                     ? st.daysRemaining
                                     : static_cast<int>(now.secsTo(QDateTime::fromSecsSinceEpoch(st.expiryTimestamp)) / 86400);
                expiry = left <= 0 ? tr("%1, today").arg(dateText(st.expiryTimestamp))
                                   : tr("%1, %2").arg(dateText(st.expiryTimestamp), daysLeft(left));
                if (!st.licenseType.isEmpty()) {
                    expiry = tr("%1 (%2 licence)").arg(expiry, st.licenseType);
                }
            }
            if (st.nextCheckTimestamp > 0) {
                next = dateText(st.nextCheckTimestamp);
            }
            break;
        }
        case Tier::Evaluation: {
            const int days = std::max(0, m_license.evaluationDaysRemaining());
            // Until the first answer from the licence server the end of the
            // evaluation is not known: no number is better than a wrong one.
            const bool known = st.evaluationEndTimestamp > 0 || days > 0;
            badge = tr("Evaluation");
            status = tr("Evaluation, %1").arg(verified); // the banner below carries the days and the limit
            expiryKey = tr("Evaluation ends");
            expiry = !known                       ? tr("Checking…")
                     : st.evaluationEndTimestamp > 0 ? tr("%1, %2").arg(dateText(st.evaluationEndTimestamp), daysLeft(days))
                                                     : daysLeft(days);
            bannerText = u"<b>%1</b> %2"_s.arg(!known    ? tr("Evaluation.")
                                              : days == 1 ? tr("Evaluation: 1 day left.")
                                                          : tr("Evaluation: %1 days left.").arg(days),
                                              tr("No daily download limit until then; Pro keeps it that way."));
            break;
        }
        case Tier::Free:
            badge = tr("Free");
            status = tr("Free, %1").arg(verified);
            if (m_license.evaluationEnded()) {
                expiryKey = tr("Evaluation ended");
                expiry = st.evaluationEndTimestamp > 0 ? dateText(st.evaluationEndTimestamp)
                                                       : tr("%1 days after the first start").arg(m_license.evaluationDays());
            }
            // Plural written by hand: "%n" rendered literally once (LESSONS).
            bannerText = u"<b>%1</b> %2"_s.arg(
                services::LicenseService::kFreeDownloadsPerDay == 1
                    ? tr("Free plan: one download a day.")
                    : tr("Free plan: %1 downloads a day.").arg(services::LicenseService::kFreeDownloadsPerDay),
                tr("Pro has no daily limit."));
            break;
        }
        if (m_license.checking()) {
            status = tr("%1 (checking)").arg(status);
        }
    }
    if (m_license.migratedFromPreviousVersion()) {
        notes << tr("The account id was carried over from the previous version, so a purchase made there counts here.");
    }
    if (st.anomalyDetected) {
        notes << tr("The clock on this computer looks wrong, so the licence is checked again sooner.");
    }
    static const QStringList kRoutineMessages{u"License is active and valid"_s, u"Evaluation active"_s,
                                              u"Evaluation period ended"_s};
    if (!st.message.isEmpty() && !kRoutineMessages.contains(st.message) && !st.message.startsWith(u"[DEBUG]"_s)) {
        notes << tr("The licence server says: %1").arg(st.message);
    }
    m_expiryKey->setText(expiryKey);
    m_expiryKey->setVisible(!expiry.isEmpty());
    m_expiry->setText(expiry);
    m_expiry->setVisible(!expiry.isEmpty());
    m_nextKey->setVisible(!next.isEmpty());
    m_next->setText(next);
    m_next->setVisible(!next.isEmpty());
    m_note->setText(notes.join(u' '));
    m_note->setVisible(!notes.isEmpty());
    if (isVisible()) {
        fitToContent();
    }
    m_planSummary->setText(
        pro ? tr("Pro: unlimited downloads with no daily count, whole playlists in one go however long they "
                 "are, and every quality up to 4K and lossless audio.")
            : tr("Free covers search, the built-in browser with sign-in, every quality up to 4K and lossless "
                 "audio, subtitles, and up to %1 downloads a day; a playlist counts each video you pick. "
                 "Pro removes the daily limit.")
                  .arg(services::LicenseService::kFreeDownloadsPerDay));
    m_planBadge->setText(badge);
    m_planBadge->setProperty("pldlPro", pro);
    m_planBadge->style()->unpolish(m_planBadge);
    m_planBadge->style()->polish(m_planBadge);
    m_status->setText(status);
    m_portal->setVisible(pro);
    m_bannerText->setText(bannerText);
    m_banner->setVisible(!bannerText.isEmpty());
}

void AccountDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    fitToContent();
}

void AccountDialog::fitToContent()
{
    // Sized once polished: the wrapped labels' height for the width is only
    // right with the style applied, and before that the id row was squeezed
    // (owner bug: a clipped copy button) or the cards padded.
    layout()->activate();
    const int h = hasHeightForWidth() ? heightForWidth(width()) : sizeHint().height();
    setMinimumHeight(h);
    resize(width(), h);
}

void AccountDialog::showPlans()
{
    auto* dialog = new PlansDialog(m_license, m_theme, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void AccountDialog::highlightPurchase()
{
    if (m_banner->isVisible()) {
        m_upgrade->setFocus();
        m_upgrade->setDefault(true);
    }
}

} // namespace pldl::ui
