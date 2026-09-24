#include "core/licensing/legacy_account.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;

namespace {

void writeConf(const QTemporaryDir& home, const QByteArray& body)
{
    QDir().mkpath(home.filePath(u".config/org.keshavnrj.ubuntu"_s));
    QFile conf(home.filePath(u".config/org.keshavnrj.ubuntu/Playlist DL.conf"_s));
    QVERIFY(conf.open(QIODevice::WriteOnly));
    conf.write(body);
}

} // namespace

class TestLegacyAccount : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void idShape()
    {
        QVERIFY(looksLikeAccountId(u"3e4bdb050f784b01b1f1"_s));
        QVERIFY(looksLikeAccountId(u"winAbC123xyz"_s));
        QVERIFY(!looksLikeAccountId(u"short"_s));
        QVERIFY(!looksLikeAccountId(u"has space 12345"_s));
        QVERIFY(!looksLikeAccountId(QString()));
    }

    void readsTwoPointXSettings()
    {
        QTemporaryDir home;
        writeConf(home, "[General]\naccountId=3e4bdb050f784b01b1f1\nPlaylist%20DL=YWN0aXZhdGVk\n");
        QCOMPARE(legacyAccountId({home.path()}), u"3e4bdb050f784b01b1f1"_s);
    }

    void fallsBackToDownloadsFile()
    {
        QTemporaryDir home;
        QDir().mkpath(home.filePath(u"Downloads"_s));
        QFile id(home.filePath(u"Downloads/.Playlist DL.id"_s));
        QVERIFY(id.open(QIODevice::WriteOnly));
        id.write("abcdef1234567890\nZW1pdA==\n");
        id.close();
        QCOMPARE(legacyAccountId({home.path()}), u"abcdef1234567890"_s);
    }

    void firstHomeWinsAndMissingIsEmpty()
    {
        QTemporaryDir a;
        QTemporaryDir b;
        QDir().mkpath(b.filePath(u"Downloads"_s));
        QFile id(b.filePath(u"Downloads/.Playlist DL.id"_s));
        QVERIFY(id.open(QIODevice::WriteOnly));
        id.write("bbbbbbbb1111\n");
        id.close();
        QCOMPARE(legacyAccountId({a.path(), b.path()}), u"bbbbbbbb1111"_s);
        QVERIFY(legacyAccountId({a.path()}).isEmpty());
        QVERIFY(!legacyAccountSearchHomes().isEmpty());
    }

    void downloadFolderOnlyWhenChangedAndPresent()
    {
        QTemporaryDir home;
        const QString custom = home.filePath(u"Videos/Lists"_s);
        QDir().mkpath(custom);
        writeConf(home, (u"[General]\ndownload_path="_s + custom + u"\n"_s).toUtf8());
        QCOMPARE(legacyDownloadFolder({home.path()}), custom);

        QTemporaryDir defaults;
        writeConf(defaults, (u"[General]\ndownload_path="_s + defaults.filePath(u"Downloads/Playlist DL"_s) + u"\n"_s).toUtf8());
        QVERIFY(legacyDownloadFolder({defaults.path()}).isEmpty());

        QTemporaryDir gone;
        writeConf(gone, "[General]\ndownload_path=/nonexistent/folder/for/test\n");
        QVERIFY(legacyDownloadFolder({gone.path()}).isEmpty());
        QVERIFY(legacyDownloadFolder({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLegacyAccount)
#include "tst_legacy_account.moc"
