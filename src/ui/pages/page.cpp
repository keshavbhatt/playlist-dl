#include "ui/pages/page.h"

#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

Page::Page(const QString& title, core::ThemeService& theme, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_theme(theme)
{
    auto* root = new QVBoxLayout(this);
    m_root = root;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QFrame(this);
    header->setProperty("pldlHeader", true);
    // The header never dictates the page's minimum width: beside the details
    // pane at 1200 px a page has 764 px, and each page's compact mode keeps
    // the header's content fitting (DESIGN.md section 5).
    header->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_header = new QHBoxLayout(header);
    m_header->setContentsMargins(24, 14, 24, 14);
    m_header->setSpacing(12);
    m_titleLabel = new QLabel(title, header);
    m_titleLabel->setProperty("pldlHeading", true);
    m_header->addWidget(m_titleLabel);
    m_header->addStretch(1);
    root->addWidget(header);

    auto* body = new QWidget(this);
    m_content = new QVBoxLayout(body);
    m_content->setContentsMargins(24, 20, 24, 20);
    m_content->setSpacing(16);
    root->addWidget(body, 1);

    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) {
        applyIcons();
        onThemeChanged();
    });
}

QLineEdit* Page::addSearchField(const QString& placeholder)
{
    auto* field = new QLineEdit(this);
    field->setPlaceholderText(placeholder);
    field->setClearButtonEnabled(true);
    field->setFixedWidth(260);
    m_header->addWidget(field);
    return field;
}

QPushButton* Page::addPrimaryButton(const QString& text, const QString& icon)
{
    m_primary = new QPushButton(text, this);
    m_primary->setProperty("pldlPrimary", true);
    m_primary->setCursor(Qt::PointingHandCursor);
    m_primaryIcon = icon;
    connect(m_primary, &QPushButton::clicked, this, &Page::addRequested);
    m_header->addWidget(m_primary);
    applyIcons();
    return m_primary;
}

void Page::setPlaceholder(const QString& text)
{
    auto* label = new QLabel(text, this);
    label->setProperty("pldlMuted", true);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    m_content->addWidget(label, 1);
}

void Page::insertAboveHeader(QWidget* strip)
{
    m_root->insertWidget(0, strip);
}

void Page::setTitleVisible(bool visible)
{
    m_titleLabel->setVisible(visible);
}

void Page::applyIcons()
{
    if (m_primary != nullptr && !m_primaryIcon.isEmpty()) {
        const Tokens t = Tokens::forScheme(m_theme.isDark());
        m_primary->setIcon(icons::themed(m_primaryIcon, t.accentText));
    }
}

} // namespace pldl::ui
