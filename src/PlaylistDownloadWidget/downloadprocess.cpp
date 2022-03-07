#include "downloadprocess.h"

#include <QDir>
#include <QSettings>
#include "utils.h"
#include "helper.h"

DownloadProcess::DownloadProcess(QObject *parent, QString enginePath, QString UUID, QMap<QString, QString> download_params) : QObject(parent)
{
    this->enginePath = enginePath;
    this->uuid = UUID;
    this->download_params = download_params;

    this->parent_playlistId = QString(UUID).split("__V__").first();

    this->videoId = QString(UUID).split("__V__").last();

    init_progressFile();

    connect(this,SIGNAL(formatReady()),this,SLOT(startDownloadProcess()));

    this->setStatus(Status::Queued);
}


DownloadProcess::~DownloadProcess()
{
    closeAndDelete();
}

DownloadProcess::ProcessState DownloadProcess::getState() const
{
    return this->state;
}

QMap<QString, QString> DownloadProcess::getDownloadProgress()
{
    return this->downloadProgress;
}

DownloadProcess::Status DownloadProcess::getStatus()
{
    //for finished
    if(Helper::isDownloaded(videoId,parent_playlistId))
    {
        this->status = Status::Finished;
        return this->status;
    }

    //for invalid status
    if(this->status >= Status::Finished && this->status <= Status::Queued){
        return Status::Queued;
    }else{
        return Status::Queued;
    }
}

bool DownloadProcess::isRunning()
{
    auto t_state = getState();
    if( t_state == ProcessState::Running ||
            t_state == ProcessState::Starting){
        return true;
    }else{
        return false;
    }
}

QString DownloadProcess::getVideoId()
{
    return this->videoId;
}

QString DownloadProcess::getPlaylistId()
{
    return this->parent_playlistId;
}

void DownloadProcess::stop()
{
    foreach (QProcess* process, this->findChildren<QProcess*>()) {
        disconnect(process,SIGNAL(finished(int)),0,0);
        disconnect(process,SIGNAL(stateChanged(QProcess::ProcessState)),0,0);
        disconnect(process, &QProcess::stateChanged, this, nullptr);
    }
    settings->setValue("status","paused");
    this->closeAndDelete();
}

void DownloadProcess::destroy()
{
    foreach (QProcess* process, this->findChildren<QProcess*>())
    {
        disconnect(process,SIGNAL(finished(int)),0,0);
        disconnect(process,SIGNAL(stateChanged(QProcess::ProcessState)),0,0);
        disconnect(process, &QProcess::stateChanged, this, nullptr);
        process->close();
        process->deleteLater();
    }
    settings->setValue("status","paused");
    this->deleteLater();
}

void DownloadProcess::closeAndDelete()
{
    emit stopped();
    this->disconnect();
    this->close();
    foreach (QProcess* process, this->findChildren<QProcess*>()) {
        process->deleteLater();
    }
    this->deleteLater();
}

void DownloadProcess::close()
{
    foreach (QProcess* process, this->findChildren<QProcess*>()) {
        process->close();
    }
}

/**
 * @brief DownloadItem::init_progressFile
 * create QSetting database file to save this item related settings
 */
void DownloadProcess::init_progressFile()
{
    QString path = QString("progress")+QDir::separator()+this->parent_playlistId;
    QString progresFilePath = utils::returnPath(path);
    //qDebug()<<"Progress file path"<<progresFilePath;

    QString progressFileName = progresFilePath+this->videoId;
    settings =  new QSettings(progressFileName,QSettings::NativeFormat,this);
}


void DownloadProcess::start()
{
    if(settings->value("formatCode").isValid()){
        //qDebug()<<this->objectName()<<"FORMAT CODE ALREADY SET";
        emit formatReady();
    }else{
        startGetFormatsProcess();
    }
}

void DownloadProcess::setState(DownloadProcess::ProcessState processState)
{
    if(processState >= ProcessState::NotRunning && processState <= ProcessState::Running){
        switch (processState) {
        case ProcessState::Starting:
             settings->setValue("status","starting");
             this->state = processState;
            break;
        case ProcessState::Running:
             settings->setValue("status","running");
             this->state = processState;
            break;
        case ProcessState::NotRunning:
             settings->setValue("status","idle");
             this->state = processState;
            break;
        default:
            break;
        }
        emit stateChanged(this->state); //state();
    }
}

void DownloadProcess::setStatus(DownloadProcess::Status status)
{
    if(status >= Status::Finished && status <= Status::Queued){
        this->status = status;
        emit statusChanged(this->status);
    }
}

void DownloadProcess::setDownloadProgress(QMap<QString,QString> downloadProgress)
{
    //save to video record file
    foreach (QString key, downloadProgress.keys()) {
        settings->setValue(key,downloadProgress.value(key));
    }

    this->downloadProgress = downloadProgress;
    emit  downloadProgressChanged(this->downloadProgress);
}


void DownloadProcess::startGetFormatsProcess()
{
    QStringList arguments = QStringList()<<"-F"<<this->videoId;
    QProcess *getFormatProcess = new QProcess(this);
    getFormatProcess->setProgram("python3");
    QStringList proc_arguments;
    proc_arguments<<this->enginePath;
    proc_arguments.append(arguments);
    getFormatProcess->setArguments(proc_arguments);
    getFormatProcess->setObjectName("__FM__"+this->uuid);

    connect(getFormatProcess,SIGNAL(finished(int)),this,SLOT(formatProcessFinished(int)));
//    connect(getFormatProcess,&QProcess::stateChanged,[=](QProcess::ProcessState state){
//        switch (state) {
//        case QProcess::Starting:
//             this->setState(ProcessState::Starting);
//            break;
//        case QProcess::Running:
//             this->setState(ProcessState::Running);
//            break;
//        case QProcess::NotRunning:
//             this->setState(ProcessState::NotRunning);
//            break;
//        default:
//            break;
//        }
//    });
    getFormatProcess->start();
    this->setState(ProcessState::Starting);
    if(getFormatProcess->waitForStarted() == false){
        //qDebug()<<"failed to start"<<Q_FUNC_INFO;
        getFormatProcess->setProgram("python3");
        getFormatProcess->start();
    }
}

void DownloadProcess::formatProcessFinished(int exitCode)
{
    if(exitCode == 0)
    {
        QProcess* senderProcess = qobject_cast<QProcess*>(sender());
        QString processName = senderProcess->objectName();
        QString UUID = processName.split("__FM__").last();

        QString s_data = senderProcess->readAll();
        QStringList audioFormatCode , videoFormatCode;
        if(!s_data.isEmpty()){
            QStringList formats = s_data.split("resolution note").last().split("\n");
            foreach(QString format,formats)
            {
                if(format.contains("audio only") && !format.isEmpty())
                {
                   QString code,codec,resolution,size;
                   code = format.split(" ").first();
                   codec = format.split(code+" ").last().trimmed().split(" ").first();
                   resolution = format.split(codec+" ").at(1).trimmed().split(" ").first();
                   size = format.split(",").last().trimmed().split(" ").first();
                   audioFormatCode<<code;
                }

                if(format.contains("video only") && !format.isEmpty())
                {
                   QString code,codec,resolution,size;
                   code = format.split(" ").first();
                   codec = format.split(code+" ").last().trimmed().split(" ").first();
                   resolution = format.split(codec+" ").at(1).trimmed().split(" ").first();
                   size = format.split(",").last().trimmed().split(" ").first();
                   videoFormatCode<<code;
                }
            }
        }
        //save format argument in video record file
        //qDebug()<<"settings format for"<<UUID;
        setDownloadArgs(audioFormatCode,videoFormatCode);
    }else{
        settings->setValue("status","error");
        this->setStatus(DownloadProcess::Status::Failed);
        emit stopped();
    }
}


void DownloadProcess::setDownloadArgs(QStringList audioFormatCodes, QStringList videoFormatCodes)
{
    //save to video record file
    foreach (QString key, this->download_params.keys()) {
        settings->setValue(key,this->download_params.value(key));
    }

    //create a format str
    QString audioCode =  settings->value("audio_quality").toString();
    QString videoCode, formatCode;
    QString download_type = settings->value("download_type").toString();
    //qDebug()<<"DOWNLOAD TYPE IS"<<download_type;
    if( download_type == "video")
    {
        videoCode =  settings->value("video_quality").toString();

        int retVcode = roundFormat(videoFormatCodes.count(),videoCode.toInt());
        int retAcode = roundFormat(audioFormatCodes.count(),audioCode.toInt());

        QString videoFormatCode = retVcode == -1 ? "0" : videoFormatCodes.at(retVcode);
        QString audioFormatCode = retAcode == -1 ? "0" : audioFormatCodes.at(retAcode);

        formatCode = videoFormatCode+"+"+audioFormatCode;
    }else{
        formatCode = QString::number(roundAudioFormat(audioCode.toInt()));
    }
    settings->setValue("formatCode",formatCode);

    emit formatReady();
}

int DownloadProcess::roundAudioFormat(int selectedFormatValue)
{
    double formateCode;

    switch (selectedFormatValue) {
    case 0 ... 100:
        formateCode = (((double)selectedFormatValue/100.0) - 1.0)*10.0;
        break;
    default:
        formateCode = 0.0;
        break;
    }
    return abs((int)(formateCode + 0.5));
}

int DownloadProcess::roundFormat(int availableFormatsCount, int selectedFormatValue)
{
    if(availableFormatsCount == 0){
        return -1;
    }
    double rounded = ((double)availableFormatsCount / 100.0)* (double)selectedFormatValue;
    rounded = rounded - 1.0;
    return abs((int)(rounded + 0.5));
}

QStringList DownloadProcess::getDownloadArgs()
{
    QSettings app_settings;
    QStringList args;
    QString formatCode   = settings->value("formatCode").toString();
    QString downloadType = settings->value("download_type").toString();
    QString container    = settings->value("container").toString();
    QString download_location = utils::returnExactPath(settings->value("download_location").toString());

    if(downloadType == "audio")
    {
       args << "--add-metadata" << "--extract-audio" << "--audio-format" << container
            << "--audio-quality" << formatCode << "--prefer-ffmpeg";
       if(app_settings.value("embed_thumbnail_to_audio",false).toBool() && container == "mp3"){
                       args.append("--embed-thumbnail");
       }
    }else{
        args <<"-f"<< formatCode <<"--merge-output-format"<<container;
    }

    args<<"-o"<< download_location+"%(title)s.%(ext)s";

    return args<<this->videoId;
}


void DownloadProcess::startDownloadProcess()
{
    QProcess *downloadProcess = new QProcess(this);
    downloadProcess->setProgram("python3");
    QStringList proc_arguments;
    proc_arguments<<this->enginePath;
    proc_arguments.append(getDownloadArgs());
    downloadProcess->setArguments(proc_arguments);
    downloadProcess->setObjectName("__IDP__"+this->uuid);

    //qDebug()<<proc_arguments;

    connect(downloadProcess,SIGNAL(readyRead()),this,SLOT(downloadProcessReadyRead()));
    connect(downloadProcess,SIGNAL(finished(int)),this,SLOT(downloadProcessFinished(int)));
    connect(downloadProcess,&QProcess::stateChanged,[=](QProcess::ProcessState state)
    {
        switch (state) {
        case QProcess::Starting:
            this->setState(ProcessState::Starting);
            break;
        case QProcess::Running:
            this->setState(ProcessState::Running);
            break;
        case QProcess::NotRunning:
            this->setState(ProcessState::NotRunning);
            break;
        default:
            break;
        }
    });

    qDebug()<<"starting ";
    this->setState(ProcessState::Starting);
    downloadProcess->start();
    if(downloadProcess->waitForStarted() == false){
        qDebug()<<"failed to start"<<downloadProcess->objectName();
        downloadProcess->setProgram("python3");
        downloadProcess->start();
    }
}


void DownloadProcess::downloadProcessReadyRead()
{
    QProcess* senderProcess = qobject_cast<QProcess*>(sender());
    QString processName = senderProcess->objectName();
    QString UUID = processName.split("__IDP__").last();

    //[download]   0.0% of 2.43GiB at 10.25KiB/s ETA 68:57:55
    QString output = senderProcess->readAll();

    QMap<QString, QString> progressMap;
    QString progressVal, progressValExact;
    if ((output.contains(QString("[download]")))&&(!output.contains("[download] Destination:"))&&
                (!output.contains("Merging formats into"))&&(!output.contains("Resuming download"))&&
                (!output.contains("[ffmpeg]"))&&!output.contains("fragments")){
        QRegExp rx("(\\d+\\.\\d+%)");
        rx.indexIn(output);
        if(!rx.cap(0).isEmpty()) {
            progressVal = rx.cap(0);
            progressVal.chop(3);
        }
        //exact percent value
        QRegExp rxe("(\\d.+%)");
        rxe.indexIn(output);
        if(!rxe.cap(0).isEmpty()) {
            progressValExact = rxe.cap(0).remove("%");
        }
        QString downspeed   = QString(QString(output.split(" at ").last()).split(" ETA").first()).remove("[download]");
        QString eta         = QString(output.split("ETA ").last()).remove("[download]");
        QString size        = QString(QString(output.split("% of ").last()).split(" at ").first()).remove("[download]");

        QRegExp rxp("([A-Za-z]+)");
        QString match;
        rxp.indexIn(size);
        if(!rxp.cap(0).isEmpty()){
            match = rxp.cap(0);
        }
        double tot_size           = QString(size.split(match).first()).toDouble();
        double downloadedSize     = (progressValExact.toDouble()/100)*tot_size;
        QString downloadedSizeStr = QString::number(downloadedSize,'f',2)+match;

        //load value from settings if they are faulty
        if(! utils::is_number(progressValExact.toStdString())){
            progressValExact = settings->value("progress_exact").toString();
        }
        if(! utils::is_number(progressVal.toStdString())){
            progressVal = settings->value("progress").toString();
        }

        progressMap.insert("progress",progressVal);
        progressMap.insert("progress_exact",progressValExact);
        progressMap.insert("size",size);
        progressMap.insert("downloaded",downloadedSizeStr);
        progressMap.insert("eta",eta);
        progressMap.insert("speed",downspeed);

        setDownloadProgress(progressMap);
    }
}

void DownloadProcess::downloadProcessFinished(int exitCode)
{
    qDebug()<<"CALLED"<<this->videoId;
    if(exitCode == 0)
    {
        settings->setValue("status","finished");
        settings->setValue("progress","100.0");
        settings->setValue("progress_exact","100.0");

        this->setStatus(Status::Finished);
    }else{
        settings->setValue("status","error");
        this->setStatus(Status::Failed);
    }
    emit stopped();
}


void DownloadProcess::changeStatus(QString status)
{
    if(status == "queued"){
        this->setStatus(Status::Queued);
    }
    settings->setValue("status",status);
}
