#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QCommandLineParser>
#include <QSet>
#include <QMap>

#ifdef DEBUGJOUVEN
#include <QDebug>
#endif


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString errorStr;
    QCoreApplication::setApplicationName("avidemuxProjectToEDLmpv");
    QCoreApplication::setApplicationVersion("1.0");
    //Connect the stdout to my qout textstream
    QTextStream qout(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription("avidemuxProjectToEDLmpv generates a mpv edl file from avidemux project file (.py)\nCreated by Jouven");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("target", "Avidemux project file");

    //process the actual command line arguments given by the user
    parser.process(app);

    while (errorStr.isEmpty())
    {
        const QStringList parsedPositionalArgs(parser.positionalArguments());
        if (parsedPositionalArgs.size() < 1)
        {
            errorStr.append("No Avidemux project file argument");
            break;
        }

        QString avidemuxProjectFileStr(parsedPositionalArgs.at(0));
        if (avidemuxProjectFileStr.isEmpty())
        {
            errorStr.append("Avidemux project file argument is an empty string");
            break;
        }

        QFileInfo avidemuxProjectFileFileInfo(QDir::fromNativeSeparators(avidemuxProjectFileStr));
        if (not avidemuxProjectFileFileInfo.exists())
        {
            avidemuxProjectFileStr.prepend("./");
            avidemuxProjectFileFileInfo = QFileInfo(QDir::fromNativeSeparators(avidemuxProjectFileStr));
            if (not avidemuxProjectFileFileInfo.exists())
            {
                errorStr.append("Avidemux project file doesn't exist");
                break;
            }
        }

        QFile savedFileToLoadTmp(avidemuxProjectFileFileInfo.filePath());
        if (not savedFileToLoadTmp.exists())
        {
            errorStr.append("Avidemux project file doesn't exist");
            break;
        }

        QByteArray fileContentsTmp;
        if (savedFileToLoadTmp.open(QIODevice::ReadOnly))
        {
            fileContentsTmp = savedFileToLoadTmp.readAll();
            if (fileContentsTmp.isEmpty())
            {
                errorStr.append("Avidemux project file is an empty file");
                break;
            }
        }
        else
        {
            errorStr.append("Error opening Avidemux project file");
            break;
        }

        int_fast64_t videoIndexTmp(0);
        QMap<int_fast64_t, QString> indexVideoMap;
        QString EDLFileContents("# mpv EDL v0");
        QList<QByteArray> linesTmp(fileContentsTmp.split('\n'));
        for (QString fileLine_ite : linesTmp)
        {
            //newer versions of avidemux, seen in 2.7.6, have validation inside the project file
            //if not adm.loadVideo("somepath"):
            //raise("Cannot load somepath")
            //which is dumb
            if (fileLine_ite.startsWith("if not adm.loadVideo"))
            {
                fileLine_ite.remove(0,7);
            }

            if (fileLine_ite.startsWith("adm.loadVideo"))
            {
                //this works for a line like this
                //adm.loadVideo("/some/path/somevideofile.mkv")
                auto firstDoubleQuotePosTmp(fileLine_ite.indexOf("\""));
                auto secondDoubleQuotePosTmp(fileLine_ite.indexOf("\"", firstDoubleQuotePosTmp + 1));
                auto sizeVideoFilenameStr(secondDoubleQuotePosTmp - firstDoubleQuotePosTmp - 1);
                QString videoFilenameStr(fileLine_ite.mid(firstDoubleQuotePosTmp + 1, sizeVideoFilenameStr));
                indexVideoMap.insert(videoIndexTmp, videoFilenameStr);
                videoIndexTmp = videoIndexTmp + 1;
            }

            if (fileLine_ite.startsWith("adm.appendVideo"))
            {
                //this works for a line like this
                //adm.appendVideo("/some/path/somevideofile.mkv")
                auto firstDoubleQuotePosTmp(fileLine_ite.indexOf("\""));
                auto secondDoubleQuotePosTmp(fileLine_ite.indexOf("\"", firstDoubleQuotePosTmp + 1));
                auto sizeVideoFilenameStr(secondDoubleQuotePosTmp - firstDoubleQuotePosTmp - 1);
                QString videoFilenameStr(fileLine_ite.mid(firstDoubleQuotePosTmp + 1, sizeVideoFilenameStr));
                indexVideoMap.insert(videoIndexTmp, videoFilenameStr);
                videoIndexTmp = videoIndexTmp + 1;
            }

            if (not indexVideoMap.isEmpty() and fileLine_ite.startsWith("adm.addSegment"))
            {
                //this works for lines like these
                //adm.addSegment(0, 0, 49162000)
                //adm.addSegment(0, 67777000, 111301000)
                auto openParenthesisPosTmp(fileLine_ite.indexOf("("));
                auto firstComaPosTmp(fileLine_ite.indexOf(","));
                auto secondComaPosTmp(fileLine_ite.indexOf(",", firstComaPosTmp + 1));
                auto closeParenthesisPosTmp(fileLine_ite.indexOf(")"));
                auto sizeVideoIndexStr(firstComaPosTmp - openParenthesisPosTmp - 1);
                auto sizeFirstTimestampStr(secondComaPosTmp - firstComaPosTmp - 2);
                auto sizeSecondTimestampStr(closeParenthesisPosTmp - secondComaPosTmp - 2);

                QString videoIndexStr(fileLine_ite.mid(openParenthesisPosTmp + 1, sizeVideoIndexStr));
                QString firstTimeStampStr(fileLine_ite.mid(firstComaPosTmp + 2, sizeFirstTimestampStr));
                QString secondTimeStampStr(fileLine_ite.mid(secondComaPosTmp + 2, sizeSecondTimestampStr));

                double firstTimeStampFloatTmp(firstTimeStampStr.toDouble());
                firstTimeStampFloatTmp = firstTimeStampFloatTmp / 1000000;

                double secondTimeStampFloatTmp(secondTimeStampStr.toDouble());
                secondTimeStampFloatTmp = secondTimeStampFloatTmp / 1000000;

                //qout << "videoIndexStr.toLongLong() " << videoIndexStr.toLongLong() << endl;
                QString lineStrTmp;
                lineStrTmp
                .append('\n')
                .append(indexVideoMap.value(videoIndexStr.toLongLong()))
                .append(',')
                .append(QString::number(firstTimeStampFloatTmp,'f', 6))
                .append(',')
                .append(QString::number(secondTimeStampFloatTmp,'f', 6))
                ;

                EDLFileContents.append(lineStrTmp);
            }
        }

        if (not indexVideoMap.isEmpty())
        {
            //qout << "path " << avidemuxProjectFileFileInfo.dir().path() + "/" + avidemuxProjectFileFileInfo.baseName() + ".edl" << endl;
            QFile edlFile(avidemuxProjectFileFileInfo.dir().path() + "/" + avidemuxProjectFileFileInfo.baseName() + ".edl");
            if (edlFile.open(QIODevice::WriteOnly))
            {
                edlFile.write(EDLFileContents.toUtf8());
            }
            else
            {
                errorStr.append("Error writing EDL file");
            }
        }
        else
        {
            errorStr.append("Couldn't find the video file path");
        }
        break;
    }

    if (not errorStr.isEmpty())
    {
        qout << "Errors:\n" << errorStr << Qt::endl;
        return EXIT_FAILURE;
    }
}
