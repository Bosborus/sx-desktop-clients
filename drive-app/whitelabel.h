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

#ifndef WHITELABEL_H
#define WHITELABEL_H

#include <algorithm>
#include <QString>
#include <QStringList>

extern const QString __applicationName;
extern const QString __organizationName;
extern const QString __organizationDomain;

extern const QString __wwwAbout;
extern const QString __wwwDocs;
extern const QString __copyrights;
extern const bool    __showPoweredBy;

extern const QString __urlRelease;
extern const QString __urlBeta;
extern const QString __urlTemplateCheck;
extern const QString __urlTemplateDownload;

extern const QString __color_background;
extern const QString __color_text;
extern const QString __color_selection;
extern const QString __color_versionText;

extern const QString __hardcodedCluster;
extern const QString __hardcodedSxAuthd;
extern const QString __hardcodedClusterDomain;
extern const QString __hardcodedSxWeb;

extern const QString __applicationId;

const QString generateApplicationId(const QString& name, const QString& domain);

#endif // WHITELABEL_H
