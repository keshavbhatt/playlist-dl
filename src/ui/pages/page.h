#pragma once

#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// One page of the main window (DESIGN.md section 2): a header (title,
/// optional search field, optional primary button) over a content area.
/// Pages add their controls to headerLayout() and content().
class Page : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Page)

public:
    Page(const QString& title, core::ThemeService& theme, QWidget* parent = nullptr);
    ~Page() override = default;

    [[nodiscard]] const QString& title() const { return m_title; }
    /// A centred muted placeholder for pages that are not built yet.
    void setPlaceholder(const QString& text);

Q_SIGNALS:
    void addRequested();

protected:
    /// Adds a search field to the header; returns it.
    QLineEdit* addSearchField(const QString& placeholder);
    /// Adds the accent "Add" button to the header; emits addRequested.
    QPushButton* addPrimaryButton(const QString& text, const QString& icon);
    /// Puts `strip` above the header (the Browser page's tab strip).
    void insertAboveHeader(QWidget* strip);
    /// Pages whose header is a toolbar (Browser) hide the title text.
    void setTitleVisible(bool visible);
    [[nodiscard]] QHBoxLayout* headerLayout() const { return m_header; }
    [[nodiscard]] QVBoxLayout* content() const { return m_content; }
    [[nodiscard]] core::ThemeService& theme() const { return m_theme; }
    void applyIcons();
    virtual void onThemeChanged() {}

private:
    QString m_title;
    core::ThemeService& m_theme;
    QVBoxLayout* m_root = nullptr;
    QHBoxLayout* m_header = nullptr;
    QVBoxLayout* m_content = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_primary = nullptr;
    QString m_primaryIcon;
};

} // namespace pldl::ui
