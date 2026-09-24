#pragma once

#include <QList>
#include <QPointer>
#include <QWidget>

class QFrame;
class QLabel;
class QTimer;

namespace pldl::ui {

/// Short confirmations in the bottom left of the window (DESIGN.md section 5):
/// "Added to queue", "Copied link". Stacked, auto-dismissed after four seconds,
/// hover pauses the clock, every toast is announced to assistive technology.
class ToastHost : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ToastHost)

public:
    enum class Kind
    {
        Info,
        Success,
        Error,
    };
    Q_ENUM(Kind)

    static constexpr int kDismissMs = 4000;
    static constexpr int kMaxVisible = 3;

    explicit ToastHost(QWidget* parent);
    ~ToastHost() override;

    void show(const QString& text, Kind kind = Kind::Info);
    [[nodiscard]] int visibleCount() const { return static_cast<int>(m_toasts.size()); }
    /// Re-anchors to the parent's bottom left; the owner calls this on resize.
    void reposition();

Q_SIGNALS:
    void shown(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Toast
    {
        QFrame* frame = nullptr;
        QTimer* timer = nullptr;
    };
    void dismiss(QFrame* frame);
    void layoutToasts();

    QList<Toast> m_toasts;
};

} // namespace pldl::ui
