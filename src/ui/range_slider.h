#pragma once

#include <QWidget>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// A two-knob range slider (mocks/add-playlist.html): picks a closed range
/// of 1-based item numbers. Keyboard: arrows move the focused knob.
class RangeSlider : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RangeSlider)

public:
    explicit RangeSlider(core::ThemeService& theme, QWidget* parent = nullptr);
    ~RangeSlider() override = default;

    void setRange(int minimum, int maximum);
    void setValues(int lower, int upper);
    [[nodiscard]] int lower() const { return m_lower; }
    [[nodiscard]] int upper() const { return m_upper; }
    [[nodiscard]] int minimum() const { return m_min; }
    [[nodiscard]] int maximum() const { return m_max; }
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void valuesChanged(int lower, int upper);
    /// The user moved a knob (pointer or keyboard), as opposed to setValues():
    /// the owner reads it as "I want a range".
    void userChanged(int lower, int upper);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    [[nodiscard]] int xFor(int value) const;
    [[nodiscard]] int valueFor(int x) const;

    core::ThemeService& m_theme;
    int m_min = 1;
    int m_max = 1;
    int m_lower = 1;
    int m_upper = 1;
    int m_dragging = -1; ///< 0 lower knob, 1 upper knob
    int m_activeKnob = 0;
public:
    [[nodiscard]] int activeKnob() const { return m_activeKnob; }
private: ///< the knob the arrow keys move; Tab or a click switches it
};

} // namespace pldl::ui
