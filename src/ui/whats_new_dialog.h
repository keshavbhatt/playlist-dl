#pragma once

#include "core/changelog.h"

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QTextBrowser;
class QWidget;

namespace pldl::ui {

/// Release notes from the bundled CHANGELOG.md, shown once per version on the
/// first start that has not seen them (FEATURES S9) and on demand from About.
/// Opens on the running version; a Version picker (shown when the changelog
/// carries more than one release) swaps in an older release's notes
/// (UMD mocks/whats-new.html, approved 2026-09-24).
class WhatsNewDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WhatsNewDialog)

public:
    WhatsNewDialog(const QString& runningVersion, const QString& changelogMarkdown, QWidget* parent = nullptr);
    ~WhatsNewDialog() override = default;

    /// The whole bundled changelog (`PLDL_DEBUG_CHANGELOG=<path>` swaps in another file).
    [[nodiscard]] static QString bundledChangelog();
    /// The bundled changelog's section for `version`; empty when there is none.
    [[nodiscard]] static QString bundledNotes(const QString& version);

    /// Shows `version`'s notes; ignored when the changelog has no such release.
    void showVersion(const QString& version);
    [[nodiscard]] QString shownVersion() const;
    [[nodiscard]] int releaseCount() const;

private:
    void setupUi();
    void fillNotes(const QString& markdown);

    QString m_running;
    QString m_markdown;
    QList<core::ChangelogRelease> m_releases;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QWidget* m_pickerRow = nullptr;
    QComboBox* m_picker = nullptr;
    QTextBrowser* m_notes = nullptr;
};

} // namespace pldl::ui
