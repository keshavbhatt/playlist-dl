#include "ui/download_options_sheet.h"

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "ui/a11y.h"
#include "ui/icons.h"
#include "ui/kind_card.h"
#include "ui/logging.h"
#include "ui/pldl_style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {

QString homeRelative(const QString& path)
{
    const QString home = QDir::homePath();
    return path.startsWith(home) ? u"~"_s + path.mid(home.size()) : path;
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("pldlMuted", true);
    return label;
}
} // namespace

DownloadOptionsSheet::DownloadOptionsSheet(core::Settings& settings, core::ThemeService& theme, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_folder(settings.downloadDirectory())
{
    setModal(true);
    setWindowTitle(tr("Download options"));
    setMinimumWidth(kWidth);
    setMaximumWidth(kWidth);
    setSizeGripEnabled(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);
    buildHeader();
    buildKinds();
    m_groups = new QStackedWidget(this);
    root->addWidget(m_groups);
    buildVideoGroup();
    buildAudioGroup();
    buildFolder();
    buildFooter();

    loadDefaults();
    applyIcons();
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) { applyIcons(); });
    a11y::nameControlsFromLabels(this);
    a11y::focusFirstField(this);
}

// ---- build -----------------------------------------------------------------

void DownloadOptionsSheet::buildHeader()
{
    auto* root = dynamic_cast<QVBoxLayout*>(layout());
    m_heading = new QLabel(tr("Download options"), this);
    m_heading->setObjectName(u"heading"_s);
    m_heading->setProperty("pldlTitle", true);
    root->addWidget(m_heading);
    m_subject = new QLabel(this);
    m_subject->setObjectName(u"subject"_s);
    m_subject->setProperty("pldlMuted", true);
    root->addWidget(m_subject);
}

void DownloadOptionsSheet::buildKinds()
{
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    m_kinds = new QButtonGroup(this);
    m_kinds->setExclusive(true);
    m_videoCard = new KindCard(u"video"_s, tr("Video"), tr("Picture and sound"), m_theme, this);
    m_videoCard->setObjectName(u"videoCard"_s);
    m_audioCard = new KindCard(u"music"_s, tr("Audio only"), tr("Just the sound"), m_theme, this);
    m_audioCard->setObjectName(u"audioCard"_s);
    m_kinds->addButton(m_videoCard, static_cast<int>(Kind::Video));
    m_kinds->addButton(m_audioCard, static_cast<int>(Kind::Audio));
    row->addWidget(m_videoCard, 1);
    row->addWidget(m_audioCard, 1);
    connect(m_kinds, &QButtonGroup::idClicked, this, [this](int) { updateKind(); });
    dynamic_cast<QVBoxLayout*>(layout())->addLayout(row);
}

void DownloadOptionsSheet::buildVideoGroup()
{
    auto* group = new QWidget(m_groups);
    group->setObjectName(u"videoGroup"_s);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_quality = new QComboBox(group);
    m_quality->setObjectName(u"qualityCombo"_s);
    for (const core::VideoQuality q : {core::VideoQuality::Best, core::VideoQuality::Q2160, core::VideoQuality::Q1440,
                                       core::VideoQuality::Q1080, core::VideoQuality::Q720, core::VideoQuality::Q480,
                                       core::VideoQuality::Q360}) {
        m_quality->addItem(core::qualityLabel(q), static_cast<int>(q));
    }
    form->addRow(fieldLabel(tr("Quality"), group), m_quality);

    m_container = new QComboBox(group);
    m_container->setObjectName(u"containerCombo"_s);
    m_container->addItem(u"MP4"_s, static_cast<int>(core::Container::Mp4));
    m_container->addItem(u"MKV"_s, static_cast<int>(core::Container::Mkv));
    m_container->addItem(u"WebM"_s, static_cast<int>(core::Container::Webm));
    form->addRow(fieldLabel(tr("Container"), group), m_container);

    auto* subtitlesRow = new QHBoxLayout;
    subtitlesRow->setSpacing(12);
    m_subtitles = new QComboBox(group);
    m_subtitles->setObjectName(u"subtitlesCombo"_s);
    m_subtitles->addItem(tr("None"), QString());
    QStringList languages = m_settings.subtitleLanguages();
    if (!languages.contains(u"en"_s)) {
        languages << u"en"_s;
    }
    for (const QString& language : std::as_const(languages)) {
        m_subtitles->addItem(QLocale(language).nativeLanguageName().isEmpty()
                                 ? language
                                 : u"%1 (%2)"_s.arg(QLocale::languageToString(QLocale(language).language()), language),
                             language);
    }
    subtitlesRow->addWidget(m_subtitles, 1);
    m_embedSubtitles = new QCheckBox(tr("Embed"), group);
    m_embedSubtitles->setObjectName(u"embedSubtitlesBox"_s);
    m_embedSubtitles->setToolTip(tr("Put the subtitles inside the file instead of next to it"));
    subtitlesRow->addWidget(m_embedSubtitles);
    form->addRow(fieldLabel(tr("Subtitles"), group), subtitlesRow);
    connect(m_subtitles, &QComboBox::currentIndexChanged, this,
            [this](int index) { m_embedSubtitles->setEnabled(index > 0); });

    m_embedThumbnail = new QCheckBox(tr("Embed thumbnail"), group);
    m_embedThumbnail->setObjectName(u"embedThumbnailBox"_s);
    form->addRow(QString(), m_embedThumbnail);
    m_embedMetadata = new QCheckBox(tr("Embed metadata and chapters"), group);
    m_embedMetadata->setObjectName(u"embedMetadataBox"_s);
    form->addRow(QString(), m_embedMetadata);
    m_groups->addWidget(group);

    for (QComboBox* combo : {m_quality, m_container}) {
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    }
}

void DownloadOptionsSheet::buildAudioGroup()
{
    auto* group = new QWidget(m_groups);
    group->setObjectName(u"audioGroup"_s);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_audioFormat = new QComboBox(group);
    m_audioFormat->setObjectName(u"audioFormatCombo"_s);
    m_audioFormat->addItem(tr("Best (as published)"), static_cast<int>(core::AudioFormat::Best));
    m_audioFormat->addItem(u"MP3"_s, static_cast<int>(core::AudioFormat::Mp3));
    m_audioFormat->addItem(u"M4A"_s, static_cast<int>(core::AudioFormat::M4a));
    m_audioFormat->addItem(u"Opus"_s, static_cast<int>(core::AudioFormat::Opus));
    m_audioFormat->addItem(u"FLAC"_s, static_cast<int>(core::AudioFormat::Flac));
    m_audioFormat->addItem(u"WAV"_s, static_cast<int>(core::AudioFormat::Wav));
    form->addRow(fieldLabel(tr("Format"), group), m_audioFormat);

    m_audioQuality = new QComboBox(group);
    m_audioQuality->setObjectName(u"audioQualityCombo"_s);
    m_audioQuality->addItem(tr("Best"), 0);
    m_audioQuality->addItem(tr("192 kbps"), 192);
    m_audioQuality->addItem(tr("128 kbps"), 128);
    form->addRow(fieldLabel(tr("Quality"), group), m_audioQuality);

    m_embedCover = new QCheckBox(tr("Embed cover art"), group);
    m_embedCover->setObjectName(u"embedCoverBox"_s);
    form->addRow(QString(), m_embedCover);
    m_embedAudioMetadata = new QCheckBox(tr("Embed metadata"), group);
    m_embedAudioMetadata->setObjectName(u"embedAudioMetadataBox"_s);
    form->addRow(QString(), m_embedAudioMetadata);
    m_groups->addWidget(group);

    connect(m_audioFormat, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
}

void DownloadOptionsSheet::buildFolder()
{
    auto* card = new QFrame(this);
    card->setObjectName(u"folderCard"_s);
    card->setProperty("pldlCard", true);
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(14, 12, 14, 12);
    column->setSpacing(8);
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    m_folderLabel = new QLabel(card);
    m_folderLabel->setObjectName(u"folderLabel"_s);
    m_folderLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(m_folderLabel, 1);
    m_change = new QPushButton(tr("Change"), card);
    m_change->setObjectName(u"changeButton"_s);
    m_change->setCursor(Qt::PointingHandCursor);
    m_change->setAccessibleName(tr("Change the download folder"));
    connect(m_change, &QPushButton::clicked, this, &DownloadOptionsSheet::chooseFolder);
    row->addWidget(m_change);
    column->addLayout(row);

    m_ownFolder = new QCheckBox(tr("Put the playlist in its own folder"), card);
    m_ownFolder->setObjectName(u"ownFolderBox"_s);
    connect(m_ownFolder, &QCheckBox::toggled, this, [this] {
        updateFolderLine();
        updatePreview();
    });
    column->addWidget(m_ownFolder);
    m_number = new QCheckBox(tr("Number files in playlist order"), card);
    m_number->setObjectName(u"numberBox"_s);
    connect(m_number, &QCheckBox::toggled, this, [this] { updatePreview(); });
    column->addWidget(m_number);
    dynamic_cast<QVBoxLayout*>(layout())->addWidget(card);

    m_preview = new QLabel(this);
    m_preview->setObjectName(u"previewLabel"_s);
    m_preview->setProperty("pldlMuted", true);
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    dynamic_cast<QVBoxLayout*>(layout())->addWidget(m_preview);
}

void DownloadOptionsSheet::buildFooter()
{
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_cancel->setObjectName(u"cancelButton"_s);
    m_cancel->setCursor(Qt::PointingHandCursor);
    m_cancel->setAutoDefault(false);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(m_cancel);
    m_download = new QPushButton(tr("Download"), this);
    m_download->setObjectName(u"downloadButton"_s);
    m_download->setProperty("pldlPrimary", true);
    m_download->setCursor(Qt::PointingHandCursor);
    m_download->setDefault(true);
    connect(m_download, &QPushButton::clicked, this, &DownloadOptionsSheet::accept);
    row->addWidget(m_download);
    dynamic_cast<QVBoxLayout*>(layout())->addLayout(row);
}

void DownloadOptionsSheet::loadDefaults()
{
    m_quality->setCurrentIndex(std::max(0, m_quality->findData(static_cast<int>(m_settings.defaultQuality()))));
    m_container->setCurrentIndex(std::max(0, m_container->findData(static_cast<int>(m_settings.defaultContainer()))));
    const QStringList languages = m_settings.subtitleLanguages();
    m_subtitles->setCurrentIndex(languages.isEmpty() ? 0 : std::max(0, m_subtitles->findData(languages.first())));
    m_embedSubtitles->setChecked(true);
    m_embedSubtitles->setEnabled(m_subtitles->currentIndex() > 0);
    m_embedThumbnail->setChecked(m_settings.embedThumbnail());
    m_embedMetadata->setChecked(m_settings.embedMetadata());
    m_audioFormat->setCurrentIndex(
        std::max(0, m_audioFormat->findData(static_cast<int>(m_settings.defaultAudioFormat()))));
    m_audioQuality->setCurrentIndex(std::max(0, m_audioQuality->findData(m_settings.defaultAudioBitrate())));
    m_embedCover->setChecked(m_settings.embedThumbnail());
    m_embedAudioMetadata->setChecked(m_settings.embedMetadata());
    m_ownFolder->setChecked(m_settings.organiseDownloads());
    m_number->setChecked(m_settings.numberPlaylistFiles());
    setKind(m_settings.lastDownloadKind() == core::DownloadKind::Audio ? Kind::Audio : Kind::Video);
}

void DownloadOptionsSheet::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    m_download->setIcon(icons::themed(u"download"_s, t.accentText));
    m_change->setIcon(icons::themed(u"folder"_s, t.text, t.muted));
}

// ---- subject ---------------------------------------------------------------

void DownloadOptionsSheet::setPlaylist(const core::MediaInfo& info, const QList<int>& selectedIndexes)
{
    m_playlist = true;
    m_info = info;
    m_indexes = selectedIndexes;
    std::sort(m_indexes.begin(), m_indexes.end());
    m_indexes.erase(std::unique(m_indexes.begin(), m_indexes.end()), m_indexes.end());
    m_url = !info.url.isEmpty() ? QUrl(info.url) : QUrl(u"https://www.youtube.com/playlist?list="_s + info.id);
    m_ownFolder->setVisible(true);
    m_number->setVisible(true);
    m_subject->setText(QFontMetrics(m_subject->font()).elidedText(info.title, Qt::ElideRight, kWidth - 48));
    updateKind();
    fitToContents();
}

void DownloadOptionsSheet::setVideo(const core::MediaEntry& entry, const QUrl& url)
{
    m_playlist = false;
    m_entry = entry;
    m_url = url;
    m_indexes.clear();
    m_ownFolder->setVisible(false);
    m_number->setVisible(false);
    m_subject->setText(QFontMetrics(m_subject->font()).elidedText(entry.title, Qt::ElideRight, kWidth - 48));
    updateKind();
    fitToContents();
}

void DownloadOptionsSheet::fitToContents()
{
    // The playlist toggles just went or came. Their card's layout learns of
    // it at once, but the card's cached hint inside the sheet's layout only
    // clears through a posted request: updateGeometry() clears it now, so
    // the sheet takes its new height before it is shown.
    m_ownFolder->parentWidget()->updateGeometry();
    adjustSize();
}

// ---- kind and folder -------------------------------------------------------

DownloadOptionsSheet::Kind DownloadOptionsSheet::kind() const
{
    return m_audioCard->isChecked() ? Kind::Audio : Kind::Video;
}

void DownloadOptionsSheet::setKind(Kind kind)
{
    (kind == Kind::Audio ? m_audioCard : m_videoCard)->setChecked(true);
    updateKind();
}

void DownloadOptionsSheet::updateKind()
{
    const bool audio = kind() == Kind::Audio;
    m_groups->setCurrentIndex(audio ? 1 : 0);
    if (m_playlist) {
        const int count = static_cast<int>(m_indexes.size());
        m_download->setText(count == 1 ? tr("Download 1 video") : tr("Download %1 videos").arg(count));
        m_download->setEnabled(count > 0);
    } else {
        m_download->setText(audio ? tr("Download audio") : tr("Download video"));
        m_download->setEnabled(!m_url.isEmpty());
    }
    updateFolderLine();
    updatePreview();
}

void DownloadOptionsSheet::setFolder(const QString& directory)
{
    if (directory.isEmpty()) {
        return;
    }
    m_folder = directory;
    updateFolderLine();
    updatePreview();
}

void DownloadOptionsSheet::chooseFolder()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Download folder"), m_folder);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

QString DownloadOptionsSheet::folderName() const
{
    if (m_playlist) {
        return m_ownFolder->isChecked() ? sanitiseFolderName(m_info.title) : QString();
    }
    return m_settings.organiseDownloads() ? core::downloadFolder(kind(), false, false) : QString();
}

QString DownloadOptionsSheet::displayFolder() const
{
    const QString sub = folderName();
    return homeRelative(sub.isEmpty() ? m_folder : QDir(m_folder).filePath(sub));
}

void DownloadOptionsSheet::updateFolderLine()
{
    const QString text = tr("Save to %1").arg(displayFolder());
    m_folderLabel->setText(QFontMetrics(m_folderLabel->font()).elidedText(text, Qt::ElideMiddle, kWidth - 180));
    m_folderLabel->setToolTip(displayFolder());
}

QString DownloadOptionsSheet::subtitleLanguage() const
{
    return m_subtitles->currentData().toString();
}

// ---- the choice ------------------------------------------------------------

core::DownloadOptions DownloadOptionsSheet::options() const
{
    core::DownloadOptions o;
    o.kind = kind();
    o.quality = static_cast<core::VideoQuality>(m_quality->currentData().toInt());
    o.container = static_cast<core::Container>(m_container->currentData().toInt());
    o.audioFormat = static_cast<core::AudioFormat>(m_audioFormat->currentData().toInt());
    o.audioBitrateKbps = m_audioQuality->currentData().toInt();
    if (o.kind == Kind::Video) {
        if (const QString language = subtitleLanguage(); !language.isEmpty()) {
            o.subtitleLanguages = {language};
        }
        o.embedSubtitles = m_embedSubtitles->isChecked();
        o.embedThumbnail = m_embedThumbnail->isChecked();
        o.embedMetadata = m_embedMetadata->isChecked();
    } else {
        o.embedThumbnail = m_embedCover->isChecked();
        o.embedMetadata = m_embedAudioMetadata->isChecked();
    }
    o.outputDirectory = m_folder;
    o.folder = folderName();
    o.filenamePattern = m_settings.filenamePattern();
    o.isPlaylist = m_playlist;
    if (m_playlist) {
        o.playlistItems = itemSpec(m_indexes);
        o.playlistSubfolder = false; // the folder above carries the title already
        o.numberPlaylistItems = m_number->isChecked();
    }
    return o;
}

core::DownloadJob DownloadOptionsSheet::job() const
{
    core::DownloadJob job;
    job.url = m_url.toString();
    job.options = options();
    job.createdAt = QDateTime::currentDateTime();
    if (m_playlist) {
        job.title = m_info.title;
        job.uploader = m_info.uploader;
        job.thumbnail = m_info.thumbnail;
        job.itemCount = static_cast<int>(m_indexes.size());
        for (const int index : m_indexes) {
            if (index >= 1 && index <= m_info.entries.size()) {
                const core::MediaEntry& entry = m_info.entries.at(index - 1);
                job.duration += std::max(0.0, entry.duration);
                job.entries.append(core::PlaylistEntry{entry.id, entry.title, {}});
                if (job.thumbnail.isEmpty()) {
                    job.thumbnail = entry.thumbnail.isEmpty() ? core::thumbnailUrl(entry.id).toString()
                                                              : entry.thumbnail;
                }
            }
        }
        return job;
    }
    job.videoId = m_entry.id.isEmpty() ? core::classifyYouTubeUrl(m_url).videoId : m_entry.id;
    job.title = m_entry.title;
    job.uploader = m_entry.uploader;
    job.thumbnail = m_entry.thumbnail.isEmpty() && !job.videoId.isEmpty() ? core::thumbnailUrl(job.videoId).toString()
                                                                           : m_entry.thumbnail;
    job.duration = m_entry.duration;
    return job;
}

void DownloadOptionsSheet::updatePreview()
{
    const core::DownloadOptions o = options();
    QString name;
    if (m_playlist) {
        const int first = m_indexes.isEmpty() ? 1 : m_indexes.first();
        const core::MediaEntry entry =
            first >= 1 && first <= m_info.entries.size() ? m_info.entries.at(first - 1) : core::MediaEntry{};
        name = core::previewFileName(o, entry.title, entry.id, entry.uploader.isEmpty() ? m_info.uploader
                                                                                          : entry.uploader,
                                     first);
    } else {
        name = core::previewFileName(o, m_entry.title, m_entry.id, m_entry.uploader);
    }
    const QString text = tr("Files like %1").arg(name);
    m_preview->setText(QFontMetrics(m_preview->font()).elidedText(text, Qt::ElideMiddle, kWidth - 48));
    m_preview->setToolTip(name);
}

QString DownloadOptionsSheet::itemSpec(const QList<int>& indexes)
{
    QList<int> sorted = indexes;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    sorted.removeIf([](int i) { return i < 1; });
    QStringList parts;
    for (qsizetype i = 0; i < sorted.size();) {
        qsizetype j = i;
        while (j + 1 < sorted.size() && sorted.at(j + 1) == sorted.at(j) + 1) {
            ++j;
        }
        if (j > i) {
            parts << u"%1-%2"_s.arg(sorted.at(i)).arg(sorted.at(j));
        } else {
            parts << QString::number(sorted.at(i));
        }
        i = j + 1;
    }
    return parts.join(u',');
}

QString DownloadOptionsSheet::sanitiseFolderName(const QString& title)
{
    return core::sanitiseFolderName(title);
}

// ---- accept ----------------------------------------------------------------

void DownloadOptionsSheet::accept()
{
    const core::DownloadOptions o = options();
    m_settings.setLastDownloadKind(o.kind == Kind::Audio ? core::DownloadKind::Audio : core::DownloadKind::Video);
    m_settings.setDefaultQuality(o.quality);
    m_settings.setDefaultContainer(o.container);
    m_settings.setDefaultAudioFormat(o.audioFormat);
    m_settings.setDefaultAudioBitrate(o.audioBitrateKbps);
    m_settings.setEmbedThumbnail(o.embedThumbnail);
    m_settings.setEmbedMetadata(o.embedMetadata);
    if (o.kind == Kind::Video) {
        // The chosen language leads the list; the others stay known for next time.
        QStringList languages = m_settings.subtitleLanguages();
        const QString chosen = subtitleLanguage();
        languages.removeAll(chosen);
        if (!chosen.isEmpty()) {
            languages.prepend(chosen);
        }
        m_settings.setSubtitleLanguages(chosen.isEmpty() ? QStringList() : languages);
    }
    if (m_playlist) {
        m_settings.setOrganiseDownloads(m_ownFolder->isChecked());
        m_settings.setNumberPlaylistFiles(m_number->isChecked());
    }
    if (m_folder != m_settings.downloadDirectory()) {
        m_settings.setDownloadDirectory(m_folder);
    }
    qCInfo(lcUi) << "download options accepted for" << m_url << (m_playlist ? o.playlistItems : QString());
    QDialog::accept();
}

} // namespace pldl::ui
