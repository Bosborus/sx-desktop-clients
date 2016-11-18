/*
 *  Copyright (C) 2012-2016 Skylable Ltd. <info-copyright@skylable.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Special exception for linking this software with OpenSSL:
 *
 *  In addition, as a special exception, Skylable Ltd. gives permission to
 *  link the code of this program with the OpenSSL library and distribute
 *  linked combinations including the two. You must obey the GNU General
 *  Public License in all respects for all of the code used other than
 *  OpenSSL. You may extend this exception to your version of the program,
 *  but you are not obligated to do so. If you do not wish to do so, delete
 *  this exception statement from your version.
 */

#include "singleapp/qtsingleapplication.h"
#include <QSystemTrayIcon>
#include <QDebug>
#include <QMessageBox>
#include "maincontroller.h"
#include "sxversion.h"
#include "sxconfig.h"
#include "versioncheck.h"
#include <QCommandLineParser>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QTranslator>
#include <QLibraryInfo>
#include <QFileInfo>
#include "util.h"
#include "shellextensions.h"
#include "util.h"
#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <sxcluster.h>
#include "sxfilter/fake_sx.h"
#include "profilemanager.h"
#include "whitelabel.h"
#include <iostream>
#include "sxlog.h"
#include "sxstate.h"
#include "logsmodel.h"
#include "coloredframe.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif


int get_input(sxc_input_t type, const char* prompt, char *in, int insize)
{
    if (type == SXC_INPUT_PLAIN || type == SXC_INPUT_SENSITIVE)
    {
        bool ok;
        QString input = QInputDialog::getText(0, QApplication::applicationName(), QString::fromUtf8(prompt),
                              type==SXC_INPUT_PLAIN?QLineEdit::Normal:QLineEdit::Password, QString(), &ok,
                              Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        if (ok)
        {
            strncpy(in, input.toUtf8().constData(), insize);
            in[insize-1]=0;
            return 0;
        }
        else
            return 1;
    }
    else
    {
        int result = QMessageBox::question(0, QApplication::applicationName(), QString::fromUtf8(prompt));
        if (result==QMessageBox::Yes)
        {
            *in = 'y';
            return 0;
        }
        else if (result==QMessageBox::No)
        {
            *in = 'n';
            return 0;
        }
        return 1;
    }
}

static const char* s_text_warning  = QT_TRANSLATE_NOOP("Main", "Warning");
static const char* s_text_ssl_missing  = QT_TRANSLATE_NOOP("Main", "Application directory is missing some OpenSSL dll files.");

QString initializeCommandLineParser(QCommandLineParser &parser, const QStringList& arguments)
{
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption("profile", "Use configuration profile <name>", "name"));
    parser.addOption(QCommandLineOption("list-profiles", "List all profiles"));

    parser.addOption(QCommandLineOption("start", QString("Start %1 in background").arg(__applicationName)));
    parser.addOption(QCommandLineOption("autostart-all", QString("Start all profiles with enabled autostart")));
    parser.addOption(QCommandLineOption("close", "Close working instance of "+__applicationName));
    parser.addOption(QCommandLineOption("close-all", "Close all working profiles"));
    parser.addOption(QCommandLineOption("pause", QString("Pause %1").arg(__applicationName)));
    parser.addOption(QCommandLineOption("resume", QString("Resume %1").arg(__applicationName)));
    parser.addOption(QCommandLineOption("status", "Show status of selected profile"));

    parser.addOption(QCommandLineOption("share", "Create public link to share <file> (Main instance of "+__applicationName+" must be running to perform this operation)", "file"));
    parser.addOption(QCommandLineOption("rev", "Show revisions of <file> (Main instance of "+__applicationName+" must be running to perform this operation)", "file"));

    parser.addOption(QCommandLineOption("open-settings", "Open settings window"));
    parser.addOption(QCommandLineOption("open-directory", "Open local directory"));

#if defined Q_OS_MAC || defined Q_OS_WIN
    parser.addOption(QCommandLineOption("update-failed", ""));
#endif

    parser.parse(arguments);
    QString tmp_profile=parser.value("profile");
    if (tmp_profile=="default")
        tmp_profile.clear();

    return tmp_profile;
}

void handleMessage(const QString& message, MainController* controller)
{
    if (message=="close")
    {
        qDebug() << __applicationName+" received 'close' message";
        QApplication::quit();
        QTimer *t = new QTimer();
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout, []()
        {
            qWarning() << __applicationName+" is still running";
            exit(1);
        });
        t->start(30000);
    }
    else if (message=="OpenLocalFolder")
    {
        if (controller)
            controller->onOpenLocalFolder();
    }
    else if (message.startsWith("share "))
    {
        if (!controller)
        {
            QMessageBox::warning(0, "ERROR", "MainController not set");
            return;
        }

        QString file = message.mid(6);
        controller->onShowShareFile(file);
    }
    else if (message.startsWith("rev "))
    {
        if (!controller)
        {
            QMessageBox::warning(0, "ERROR", "MainController not set");
            return;
        }
        QString file = message.mid(4);
        controller->onShowRevisions(file);
    }
    else if (message.startsWith("status "))
    {
        QString name = message.mid(7);
        QLocalSocket socket;
        socket.connectToServer(name);
        if (!socket.waitForConnected())
            return;
        QString state = controller->getState().toString();
        socket.write(state.toUtf8());
        socket.flush();
        socket.disconnectFromServer();
        if (socket.state() != QLocalSocket::UnconnectedState)
            socket.waitForDisconnected();
    }
    else if (message == "pause")
    {
        controller->pause();
    }
    else if (message == "resume")
    {
        controller->resume();
    }
    else if (message == "open-settings")
    {
        controller->onShowSettings();
    }
    else if (message == "update-failed") {
        VersionCheck::instance()->updateFailed();
    }
}

void handleLocalSocketConnection(QLocalSocket *socket,  MainController* controller)
{
    QString message;
    socket->waitForReadyRead();
    message = QString::fromLocal8Bit(socket->readAll());
    if (message == "status")
    {
        //SxConfig config(controller->profile());
        QString state = controller->getState().toString();
        if (state == "paused")
        {
            if (controller->isWizardVisible())
                state = "paused-wizard";
        }
        socket->write(state.toUtf8());
        socket->flush();
        socket->waitForBytesWritten();
    }
    socket->close();
    socket->deleteLater();
    if (message == "close") {
        QApplication::exit(0);
    }
}

void handleCommandLineArguments(QtSingleApplication &app, QCommandLineParser& parser, QString &profile, bool &openFolder)
{
    if (parser.isSet("start"))
    {
        QProcess p;
        QStringList argments;
        if (!profile.isEmpty())
            argments << "--profile" << profile;
        if (parser.isSet("open-directory"))
            argments << "--open-directory";
        p.startDetached(QApplication::applicationFilePath(), argments);
        exit(0);
    }
    if (parser.isSet("autostart-all")) {
        QStringList profiles = ProfileManager::instance()->listProfiles();
        bool startDefault = false;
        foreach (QString profile, profiles) {
            QStringList tmp = ProfileManager::instance()->profileStatus(profile);
            if (tmp.at(1) == "enabled") {
                QProcess p;
                QStringList argments;
                if (profile != "default") {
                    argments << "--profile" << profile;
                    p.startDetached(QApplication::applicationFilePath(), argments);
                }
                else
                    startDefault = true;
            }
        }
        if (startDefault) {
            QProcess p;
            p.startDetached(QApplication::applicationFilePath());
        }
        exit(0);
    }
    if (parser.isSet("close"))
    {
        if(app.isRunning())
        {
            if (app.sendMessage("close"))
               qDebug() << "Sent close message to main instance of "+__applicationName;
            else
                qDebug() << "Failed to send close message to another instance of "+__applicationName+profile;
        }
        else {
            qDebug() << __applicationName+"-"+profile+" is not running";
            exit(1);
        }
        exit(0);
    }
    if (parser.isSet("close-all")) {
        ProfileManager::instance()->closeAllProfiles();
        exit(0);
    }
    if (parser.isSet("share"))
    {
        if (app.isRunning())
        {
            QString value = parser.value("share");
            if (app.sendMessage("share "+value))
            {
                qDebug() << "Sent share message to main instance of "+__applicationName;
                exit(0);
            }
        }
        qDebug() << "Main instance of "+__applicationName+" is not working";
        exit(1);
    }
    if (parser.isSet("rev"))
    {
        if (app.isRunning())
        {
            QString value = parser.value("rev");
            if (app.sendMessage("rev "+value))
            {
                qDebug() << "Sent rev message to main instance of "+__applicationName;
                exit(0);
            }
        }
        qDebug() << "Main instance of "+__applicationName+" is not working";
        exit(1);
    }
    if (parser.isSet("list-profiles"))
    {
        QStringList profiles = ProfileManager::instance()->listProfiles();
        if (parser.isSet("status"))
        {
            QList<QStringList> output;
            output.append({"profile", "status", "autostart"});
            int len = 6;
            int second_len = 6;
            foreach (QString p, profiles) {
                if (p.length() > len)
                    len = p.length();
            }
            foreach (QString p, profiles) {
                QStringList tmp = ProfileManager::instance()->profileStatus(p);
                output.append({p, tmp.at(0), tmp.at(1)});
                if (tmp.at(0).length() > second_len)
                    second_len = tmp.at(0).length();
            }

            foreach (QStringList l, output) {
                QString line = QString("%1   %2   %3")
                        .arg(l.at(0).leftJustified(len, ' '))
                        .arg(l.at(1).leftJustified(second_len, ' '))
                        .arg(l.at(2));
                qDebug() << line;
            }
        }
        else
        {
            foreach (QString p, profiles) {
                qDebug() << p;
            }
        }
        exit(0);
    }
    if (parser.isSet("status"))
    {
        SxConfig config(profile);
        QString status = "off";
        if (app.isRunning()) {
            status = "not responding";
            QLocalSocket socket;
            socket.connectToServer(ProfileManager::getLocalServerName(profile));
            if (!socket.waitForConnected(1000))
                goto printStatus;
            socket.write(QString("status").toUtf8());
            socket.flush();
            if (!socket.waitForReadyRead())
                goto printStatus;
            status = QString::fromUtf8(socket.readAll());
            socket.disconnectFromServer();
            if (socket.state() != QLocalSocket::UnconnectedState)
                socket.waitForDisconnected();
        }
        printStatus:
        qDebug() << "status: "+status;
        qDebug() << "autostart: "+QString(config.desktopConfig().autostart()?"enabled":"disabled");
        exit(0);
    }

    if (parser.isSet("pause"))
    {
        if (app.isRunning())
        {
            app.sendMessage("pause");
            exit(0);
        }
        else
        {
            qDebug() << QString("%1 is not running").arg(__applicationName);
            exit(1);
        }
    }

    if (parser.isSet("resume"))
    {
        if (app.isRunning())
        {
            app.sendMessage("resume");
            exit(0);
        }
        else
        {
            qDebug() << QString("%1 is not running").arg(__applicationName);
            exit(1);
        }
    }

    if (parser.isSet("open-settings"))
    {
        if (app.isRunning())
        {
            app.sendMessage("open-settings");
            exit(0);
        }
        else
        {
            qDebug() << QString("%1 is not running").arg(__applicationName);
            exit(1);
        }
    }

    if (parser.isSet("open-directory"))
    {
        if (app.isRunning())
        {
            app.sendMessage("OpenLocalFolder");
            exit(0);
        }
        else
            openFolder = true;
    }
#if defined Q_OS_MAC || defined Q_OS_WIN
    if (parser.isSet("update-failed")) {
        if (!profile.isEmpty())
            exit(1);
        if (!app.isRunning())
            exit(1);
        app.sendMessage("update-failed");
        exit(0);
    }
#endif
}

void createPidFile(QtSingleApplication &app) {
#if defined Q_OS_MAC
    qint64 pid = app.applicationPid();
    QFile pid_file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/sxdrive.pid");
    if (pid_file.open(QIODevice::WriteOnly))
    {
        QTextStream fstr(&pid_file);
        fstr << pid;
        if (!pid_file.flush())
            qWarning() << "Pid file: flush failed";
        pid_file.close();
    }
    else
        qWarning() << "Unable to create pid file";
#else
    Q_UNUSED(app);
#endif
}

void checkWindowsDlls(QtSingleApplication &app) {
    Q_UNUSED(app);
#if defined Q_OS_WIN
    static const QStringList files = {"libeay32.dll", "ssleay32.dll"};
    bool missing = false;
    foreach (auto f, files) {
        QFileInfo fi(app.applicationDirPath()+"/"+f);
        if (!fi.exists())
        {
            missing = true;
            qCritical() << f << " is missing";
        }
    }
    if (missing)
    {
        QMessageBox::critical(0,QApplication::translate("Main", s_text_warning), QApplication::translate("Main", s_text_ssl_missing));
    }
#else
    Q_UNUSED(s_text_warning);
    Q_UNUSED(s_text_ssl_missing);
#endif
}

int main(int argc, char **argv) {
#ifdef Q_OS_WIN
    AttachConsole(-1);
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    freopen("CON", "r", stdin);
#endif

    ColoredFrame::setupColors(__color_background, __color_text, __color_selection, __color_versionText);
    MainController *controller = 0;

    QStringList arguments;
    for (int i=0; i<argc; i++)
        arguments << QString::fromLocal8Bit(argv[i]);
    QCommandLineParser parser;
    QString profile=initializeCommandLineParser(parser, arguments);

    QtSingleApplication app(__applicationId+(profile.isEmpty()?"":"-"+profile), argc, argv);
    app.setOrganizationName(__organizationName);
    app.setOrganizationDomain(__organizationDomain);
    app.setApplicationName(__applicationName+(profile.isEmpty()?"":"-"+profile));
    app.setApplicationVersion(SXVERSION);
    app.setQuitOnLastWindowClosed(false);
    parser.process(arguments);

    setIsRetina(app.devicePixelRatio()>=2);

    QObject::connect(&app, &QtSingleApplication::messageReceived, [&controller](const QString& message) {
        handleMessage(message, controller);
    });

    bool openFolder = false;
    handleCommandLineArguments(app, parser, profile, openFolder);

    if(app.isRunning()) {
        qDebug() << "Another instance of "+__applicationName+" is already running for the " + (profile.isEmpty() ? "default" : profile) + " profile." ;
        return 0;
    }

    QLocalServer::removeServer(ProfileManager::getLocalServerName(profile));
    QLocalServer localServer;

    QObject::connect(&localServer, &QLocalServer::newConnection, [&localServer, &controller] {
        QLocalSocket* socket = localServer.nextPendingConnection();
        if (socket)
            handleLocalSocketConnection(socket, controller);
    });
    if (!localServer.listen(ProfileManager::getLocalServerName(profile)))
    {
        qWarning() << "Unable to initialize local server";
        return 1;
    }

    SxConfig config(profile);
    qDebug() << app.applicationName() << "version" << SXVERSION;
    QThread::currentThread()->setProperty("name", "GUI_THREAD");
    SxLog::instance().setLogModel(LogsModel::instance());
    LogLevel logLevel = static_cast<LogLevel>(static_cast<int>(LogLevel::Info)-config.desktopConfig().logLevel());
    SxLog::instance().setLogLevel(logLevel);

    QString startMessage = QString("%1 version %2 started").arg(__applicationName).arg(SXVERSION);
    logInfo(QString(startMessage.length(), '-'));
    logInfo(startMessage);
    logInfo(QString(startMessage.length(), '-'));

    {
        auto shapeDesc = config.desktopConfig().trayIconMark();
        QString colorName = shapeDesc.second;
        if (colorName.isEmpty() || !QColor::isValidColor(colorName)) {
            config.desktopConfig().setTrayIconMark(shapeDesc.first, __color_background);
            config.syncConfig();
        }
    }

    ShellExtensions::instance()->setConfig(&config);
    ShellExtensions::instance()->disable();

    SxCluster::setClientVersion(__applicationName+"-"+SXVERSION);
    controller = new MainController(&config);
    ProfileManager::instance()->setMainController(controller);

    if (openFolder)
    {
        QTimer::singleShot(0, controller, SLOT(onOpenLocalFolder()));
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        qWarning() << "System tray icon is not available";
        QTimer *t = new QTimer();
        QObject::connect(t, &QTimer::timeout, [controller, t]()
        {
            static int counter = 0;
            if (QSystemTrayIcon::isSystemTrayAvailable())
            {
                t->stop();
                t->disconnect();
                t->deleteLater();
                controller->reinitTray();
            }
            else
                counter ++;
            if (counter > 15)
            {
                QMessageBox::critical(nullptr, __applicationName, "System tray support is required to run this application");
                exit(1);
            }
        });
        t->start(1000);
    }
    createPidFile(app);
    checkWindowsDlls(app);

    VersionCheck::initializeVersionCheck("sxdrive", SXVERSION, __urlRelease, __urlBeta, __urlTemplateCheck, __urlTemplateDownload, "sxdrive_upgrade");
    bool startMaincontroller = !config.firstRun();
    if (profile.isEmpty())
    {
        VersionCheck *vc = VersionCheck::instance();
        QObject::connect(vc, &VersionCheck::updateSuccessful, controller, &MainController::onShowWhatsNew);
        QObject::connect(vc, &VersionCheck::newVersionAvailable, controller, &MainController::onNewVersionAvailable);
        QObject::connect(vc, &VersionCheck::initialCheckFinished, controller, &MainController::startMainControler);
        QObject::connect(vc, &VersionCheck::noNewVersion, controller, &MainController::onNoNewVersion);
        QObject::connect(vc, &VersionCheck::versionCheckFailed, controller, &MainController::onVersionCheckError);
        QObject::connect(controller, &MainController::checkNowForNewVersion, vc, &VersionCheck::checkVersionShowResult);

        DesktopConfig &desktopConfig = config.desktopConfig();
        if (desktopConfig.checkUpdates())
        {
            vc->setEnabled(true);
            vc->setCheckingBeta(desktopConfig.checkBetaVersions());
#if defined Q_OS_WIN || defined Q_OS_MAC
            startMaincontroller = !vc->initialCheck();
#else
            vc->checkNow();
#endif
        }
    }
    if (startMaincontroller)
        controller->startMainControler();

#if defined Q_OS_LINUX
    QTimer timer;
    if (profile.isEmpty()) {
        QObject::connect(&timer, &QTimer::timeout, [&timer]() {
            QProcess sxdrive;
            sxdrive.start(QApplication::applicationFilePath(), {"--version"});
            if (!sxdrive.waitForStarted())
                return;
            if (!sxdrive.waitForFinished())
                return;
            QString result = QString::fromLocal8Bit(sxdrive.readAll());
            QRegExp regexp("("+__applicationName+" )(\\d+)[.](\\d+)[.](\\d+)([.]beta.(\\d+))?(\n)", Qt::CaseInsensitive, QRegExp::RegExp2);
            if (regexp.exactMatch(result)) {
                QString version = result.mid(__applicationName.length()+1, result.size()-__applicationName.length()-2);
                VersionCheck::SxVersion currentVersion = {SXVERSION};
                VersionCheck::SxVersion binaryVersion = {version};
                if (binaryVersion > currentVersion) {
                    logVerbose("detected newer "+ __applicationName + "version installed in system");
                    timer.stop();
                    QString message =
                            QApplication::translate("MainController"," A new version (<b>%2</b>) of %1 has been installed.")
                                .arg(__applicationName).arg(version)+"<br>"+
                            QApplication::translate("MainController", "Restart %1 to start using the new version.")
                                .arg(__applicationName);
                    QMessageBox::information(nullptr, __applicationName, message);
                }
            }
        });
        timer.start(120*1000);
    }
#endif
    bool retVal = app.exec();
    delete controller;
    ShellExtensions::instance()->disable();

    return retVal;
}
