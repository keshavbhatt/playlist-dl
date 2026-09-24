#pragma once

#include "core/downloads/download_job.h"

#include <QList>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>

#include <optional>

class QTimer;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class ThumbnailCache;
struct Tokens;

/// Paints one download as an 88 px card (DESIGN.md section 3, Downloads):
/// the thumbnail with a state bar, the title with the quality and format at
/// its right, the uploader or the playlist's place, the progress bar and the
/// status line, and on hover the glyph buttons for the state (pause or
/// resume, cancel, retry, open, show in folder, remove). Buttons are hit
/// tested in editorEvent and reported through actionTriggered.
class DownloadCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadCardDelegate)

public:
    enum class Action
    {
        PauseResume,
        Cancel,
        Retry,
        Open,
        ShowInFolder,
        Remove,
    };
    Q_ENUM(Action)

    DownloadCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~DownloadCardDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    static constexpr int kCardHeight = 88;
    static constexpr int kThumbWidth = 96;
    static constexpr int kThumbHeight = 54;

    /// The button under `pos` when the card of `job` fills the item `rect`.
    [[nodiscard]] std::optional<Action> actionAt(const QPoint& pos, const QRect& rect,
                                                 const core::DownloadJob& job) const;
    /// The actions a card in `state` offers, in their painted order.
    [[nodiscard]] static QList<Action> actionsFor(core::DownloadState state, bool hasFile);
    /// The tooltip and accessible name of an action (Pause and Resume share one).
    [[nodiscard]] static QString actionLabel(Action action, core::DownloadState state);
    /// The colour of a state's bar (DESIGN.md: queued muted, active accent,
    /// paused warning, finished success, failed and cancelled danger).
    [[nodiscard]] static QColor stateColor(core::DownloadState state, bool dark);

Q_SIGNALS:
    void actionTriggered(quint64 jobId, pldl::ui::DownloadCardDelegate::Action action);
    /// An indeterminate bar is on screen: the view repaints on this.
    void repaintNeeded();

private:
    struct HitButton
    {
        QRect rect;
        Action action;
        QString icon;
        QString tooltip;
    };
    [[nodiscard]] QList<HitButton> buttonsFor(const QRect& card, core::DownloadState state, bool hasFile) const;
    [[nodiscard]] static QRect cardRect(const QRect& rect);
    void paintProgress(QPainter* painter, const QRect& track, core::DownloadState state, double progress,
                       const QColor& chunk, const QColor& trackColor) const;
    void paintButtons(QPainter* painter, const QList<HitButton>& buttons, const Tokens& tokens, qreal dpr) const;
    void pulse();

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    QPersistentModelIndex m_hoverIndex;
    QPoint m_hoverPos;
    QTimer* m_pulse = nullptr;
    mutable bool m_animationPainted = false;
};

} // namespace pldl::ui
