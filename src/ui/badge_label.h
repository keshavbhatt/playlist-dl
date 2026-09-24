#pragma once

#include <QLabel>
#include <QSize>
#include <QString>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// A pill badge carrying a glyph and a word (mocks/mock.css .badge): the
/// Browser toolbar's "Ads blocked: 12" and "Signed in". A rich-text <img>
/// cannot be aligned with the words a QLabel lays out, so the badge paints
/// the pill, the tinted glyph and the text itself from the brand tokens and
/// re-tints on a scheme change. setText() and text() stay QLabel's, so the
/// words are what the accessibility layer and the tests read.
class BadgeLabel : public QLabel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BadgeLabel)

public:
    explicit BadgeLabel(core::ThemeService& theme, QWidget* parent = nullptr);
    ~BadgeLabel() override;

    static constexpr int kGlyph = 14;  ///< the glyph box, centred on the words
    static constexpr int kHeight = 20; ///< the pill of the mocks

    /// The monochrome icon drawn before the words (:/icons/ui/<name>.svg);
    /// empty for a badge of words alone.
    void setGlyph(const QString& name);
    [[nodiscard]] QString glyph() const { return m_glyph; }
    /// The "good news" variant of the mocks (.badge.ok): the success colour on
    /// a success tint instead of the accent pair.
    void setOk(bool ok);
    [[nodiscard]] bool isOk() const { return m_ok; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::ThemeService& m_theme;
    QString m_glyph;
    bool m_ok = false;
};

} // namespace pldl::ui
