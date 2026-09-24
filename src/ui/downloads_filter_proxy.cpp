#include "ui/downloads_filter_proxy.h"

#include "core/downloads/download_queue.h"

namespace pldl::ui {

DownloadsFilterProxy::DownloadsFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void DownloadsFilterProxy::setFilter(Filter filter)
{
    if (m_filter == filter) {
        return;
    }
    beginFilterChange();
    m_filter = filter;
    endFilterChange(Direction::Rows);
}

bool DownloadsFilterProxy::matches(Filter filter, core::DownloadState state)
{
    switch (filter) {
    case Filter::All:
        return true;
    case Filter::Active:
        return !core::isFinishedState(state);
    case Filter::Finished:
        return state == core::DownloadState::Completed;
    case Filter::Failed:
        return state == core::DownloadState::Failed || state == core::DownloadState::Cancelled;
    }
    return true;
}

int DownloadsFilterProxy::countFor(const core::DownloadQueue& queue, Filter filter)
{
    int n = 0;
    for (const core::DownloadJob& job : queue.jobs()) {
        if (matches(filter, job.state)) {
            ++n;
        }
    }
    return n;
}

bool DownloadsFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return matches(m_filter, index.data(core::DownloadQueue::StateRole).value<core::DownloadState>());
}

} // namespace pldl::ui
