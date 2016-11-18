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

#include <QApplication>
#include "scoutconfig.h"
#include "mainwindow.h"
#include "scoutwizard.h"
#include "versioncheck.h"
#include "version.h"
#include "sxlog.h"
#include "logsmodel.h"
#include "singleapp/qtsingleapplication.h"
#include "scoutcontroller.h"

int main(int argc, char *argv[])
{
    QtSingleApplication app("SXScout", argc, argv);
    app.setApplicationName("SXScout");
    app.setOrganizationName("Skylable");
    app.setOrganizationDomain("skylable.com");
    app.setApplicationVersion(SCOUTVERSION);
    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().count() > 2) {
        qCritical() << "invalid arguments";
        return 1;
    }
    if (app.arguments().count() == 2) {
        if (app.arguments().at(1)=="--close-all") {
            if (app.isRunning()) {
                app.sendMessage("closeAll");
                return 0;
            }
            else
                qCritical() << "SXScout is not running";
        }
        else
            qCritical() << "invalid arguments";
        return 1;
    }

    if (app.isRunning()) {
        app.sendMessage("newWindow");
        return 0;
    }
    ScoutController mainController;
    QObject::connect(&app, &QtSingleApplication::messageReceived, &mainController, &ScoutController::messageReceived);
    return app.exec();
}
