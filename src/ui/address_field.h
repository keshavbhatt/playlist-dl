#pragma once

#include <QLineEdit>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// The browser's address field with the page-load progress drawn inside it
/// as a hairline along its bottom edge (owner: a bar in the layout above the
/// page moved the page by its height every time a load began or ended).
class AddressField : public QLineEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AddressField)

public:
    explicit AddressField(const core::ThemeService& theme, QWidget* parent = nullptr);
    ~AddressField() override = default;

    /// 0 to 100 shows the hairline at that share; a negative value hides it.
    void setProgress(int percent);
    [[nodiscard]] int progress() const { return m_progress; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const core::ThemeService& m_theme;
    int m_progress = -1;
};

} // namespace pldl::ui
