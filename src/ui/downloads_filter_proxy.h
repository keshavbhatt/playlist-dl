#pragma once

#include "core/downloads/download_job.h"

#include <QSortFilterProxyModel>

namespace pldl::core {
class DownloadQueue;
}

namespace pldl::ui {

/// The Downloads page's All / Active / Finished / Failed filter over the
/// queue's StateRole. Active is everything that is not over yet (queued,
/// running, paused); Finished is completed; Failed is failed and cancelled.
class DownloadsFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadsFilterProxy)

public:
    enum class Filter
    {
        All,
        Active,
        Finished,
        Failed,
    };
    Q_ENUM(Filter)

    explicit DownloadsFilterProxy(QObject* parent = nullptr);
    ~DownloadsFilterProxy() override = default;

    void setFilter(Filter filter);
    [[nodiscard]] Filter filter() const { return m_filter; }

    /// True when a job in `state` belongs to `filter`.
    [[nodiscard]] static bool matches(Filter filter, core::DownloadState state);
    /// How many of the queue's jobs `filter` shows.
    [[nodiscard]] static int countFor(const core::DownloadQueue& queue, Filter filter);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    Filter m_filter = Filter::All;
};

} // namespace pldl::ui
