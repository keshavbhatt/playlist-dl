#pragma once

#include "core/downloads/download_job.h"
#include "core/downloads/download_options.h"
#include "core/downloads/media_info.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QUrl>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace pldl::core {
class Settings;
class ThemeService;
} // namespace pldl::core

namespace pldl::ui {

class KindCard;

/// The Download options sheet (DESIGN.md section 3, FEATURES O1 to O5): the
/// Video / Audio kind cards over their option groups, the folder row with
/// the playlist toggles, the file name preview, Cancel and Download. Opened
/// for a playlist selection (setPlaylist) or one video (setVideo); `job()`
/// is the one queue entry the choice describes. Every choice is written back
/// to the settings as the next default when the sheet is accepted (O5).
class DownloadOptionsSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadOptionsSheet)

public:
    using Kind = core::DownloadOptions::Kind;

    static constexpr int kWidth = 640;

    DownloadOptionsSheet(core::Settings& settings, core::ThemeService& theme, QWidget* parent = nullptr);
    ~DownloadOptionsSheet() override = default;

    /// A playlist and the 1-based indexes of the entries to download.
    void setPlaylist(const core::MediaInfo& info, const QList<int>& selectedIndexes);
    /// One video; `url` is what the engine is given.
    void setVideo(const core::MediaEntry& entry, const QUrl& url);
    [[nodiscard]] bool isPlaylist() const { return m_playlist; }

    /// The job the sheet describes right now.
    [[nodiscard]] core::DownloadJob job() const;
    [[nodiscard]] core::DownloadOptions options() const;
    [[nodiscard]] Kind kind() const;
    void setKind(Kind kind);
    [[nodiscard]] QString folder() const { return m_folder; }
    void setFolder(const QString& directory);

    /// yt-dlp's item spec for a set of 1-based indexes: "1-3,7,9-12" (pure).
    [[nodiscard]] static QString itemSpec(const QList<int>& indexes);
    /// The playlist's folder name (core::sanitiseFolderName).
    [[nodiscard]] static QString sanitiseFolderName(const QString& title);

    [[nodiscard]] KindCard* videoCard() const { return m_videoCard; }
    [[nodiscard]] KindCard* audioCard() const { return m_audioCard; }
    [[nodiscard]] QStackedWidget* groups() const { return m_groups; }
    [[nodiscard]] QComboBox* qualityCombo() const { return m_quality; }
    [[nodiscard]] QComboBox* containerCombo() const { return m_container; }
    [[nodiscard]] QComboBox* subtitlesCombo() const { return m_subtitles; }
    [[nodiscard]] QCheckBox* embedSubtitlesBox() const { return m_embedSubtitles; }
    [[nodiscard]] QCheckBox* embedThumbnailBox() const { return m_embedThumbnail; }
    [[nodiscard]] QCheckBox* embedMetadataBox() const { return m_embedMetadata; }
    [[nodiscard]] QComboBox* audioFormatCombo() const { return m_audioFormat; }
    [[nodiscard]] QComboBox* audioQualityCombo() const { return m_audioQuality; }
    [[nodiscard]] QCheckBox* embedCoverBox() const { return m_embedCover; }
    [[nodiscard]] QCheckBox* embedAudioMetadataBox() const { return m_embedAudioMetadata; }
    [[nodiscard]] QLabel* folderLabel() const { return m_folderLabel; }
    [[nodiscard]] QPushButton* changeButton() const { return m_change; }
    [[nodiscard]] QCheckBox* ownFolderBox() const { return m_ownFolder; }
    [[nodiscard]] QCheckBox* numberBox() const { return m_number; }
    [[nodiscard]] QLabel* previewLabel() const { return m_preview; }
    [[nodiscard]] QPushButton* cancelButton() const { return m_cancel; }
    [[nodiscard]] QPushButton* downloadButton() const { return m_download; }

public Q_SLOTS:
    /// Writes the choices back as the defaults, then closes with Accepted.
    void accept() override;

private:
    void buildHeader();
    void buildKinds();
    void buildVideoGroup();
    void buildAudioGroup();
    void buildFolder();
    void buildFooter();
    void loadDefaults();
    void applyIcons();
    void chooseFolder();
    void updateKind();
    void fitToContents();
    void updateFolderLine();
    void updatePreview();
    [[nodiscard]] QString folderName() const;
    [[nodiscard]] QString subtitleLanguage() const;
    [[nodiscard]] QString displayFolder() const;

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    core::MediaInfo m_info;
    QList<int> m_indexes;
    core::MediaEntry m_entry;
    QUrl m_url;
    QString m_folder;
    bool m_playlist = false;

    QLabel* m_heading = nullptr;
    QLabel* m_subject = nullptr;
    QButtonGroup* m_kinds = nullptr;
    KindCard* m_videoCard = nullptr;
    KindCard* m_audioCard = nullptr;
    QStackedWidget* m_groups = nullptr;
    QComboBox* m_quality = nullptr;
    QComboBox* m_container = nullptr;
    QComboBox* m_subtitles = nullptr;
    QCheckBox* m_embedSubtitles = nullptr;
    QCheckBox* m_embedThumbnail = nullptr;
    QCheckBox* m_embedMetadata = nullptr;
    QComboBox* m_audioFormat = nullptr;
    QComboBox* m_audioQuality = nullptr;
    QCheckBox* m_embedCover = nullptr;
    QCheckBox* m_embedAudioMetadata = nullptr;
    QLabel* m_folderLabel = nullptr;
    QPushButton* m_change = nullptr;
    QCheckBox* m_ownFolder = nullptr;
    QCheckBox* m_number = nullptr;
    QLabel* m_preview = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_download = nullptr;
};

} // namespace pldl::ui
