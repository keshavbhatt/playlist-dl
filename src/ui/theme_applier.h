#pragma once

#include <QObject>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// Pushes the ThemeService palette and Red style sheet onto the QApplication.
/// Fusion style is used everywhere so the sheet renders identically across
/// desktops (FEATURES D1).
class ThemeApplier : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ThemeApplier)

public:
    explicit ThemeApplier(core::ThemeService& theme, QObject* parent = nullptr);
    ~ThemeApplier() override = default;

private:
    void apply();

    core::ThemeService& m_theme;
};

} // namespace pldl::ui
