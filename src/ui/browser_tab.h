#pragma once

#include <QAbstractButton>
#include <QIcon>
#include <QString>

class QTimer;
class QToolButton;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// One tab of the Browser page's strip (mocks/browser.html): a glyph or the
/// site's icon, the elided title and a close button. The checked tab is
/// painted in the page background so it reads as attached to the toolbar;
/// the others sit on the rail colour. Shrinks with the strip down to a glyph
/// and a few letters, never scrolls (DESIGN.md section 5).
class BrowserTabButton : public QAbstractButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BrowserTabButton)

public:
    explicit BrowserTabButton(core::ThemeService& theme, QWidget* parent = nullptr);
    ~BrowserTabButton() override;

    static constexpr int kMaxWidth = 220;
    /// Below this the title goes and the tab is its glyph alone (many tabs);
    /// the strip therefore never runs past the window edge.
    static constexpr int kMinWidth = 76;
    static constexpr int kCompactWidth = 40;
    static constexpr int kHeight = 34;

    void setTitle(const QString& title);
    [[nodiscard]] const QString& title() const { return m_title; }
    /// The site's icon; an empty icon falls back to the glyph.
    void setSiteIcon(const QIcon& icon);
    /// "globe" for a page, "film" when it plays media (FEATURES B4).
    void setGlyph(const QString& glyph);
    void setLoading(bool loading);
    [[nodiscard]] bool isLoading() const { return m_loading; }
    [[nodiscard]] QToolButton* closeButton() const { return m_close; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void tick();
    void applyTheme();
    void placeClose();

    core::ThemeService& m_theme;
    QToolButton* m_close = nullptr;
    QString m_title;
    QIcon m_siteIcon;
    QString m_glyph;
    bool m_loading = false;
    QTimer* m_spin = nullptr; ///< turns the loader while the page loads (owner: it looked frozen)
    int m_spinAngle = 0;
    bool m_hover = false;
};

} // namespace pldl::ui
