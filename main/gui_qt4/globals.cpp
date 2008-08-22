/*
* Copyright (C) 2008 Louai Al-Khanji
*
* This program is free software; you can redistribute it and/
* or modify it under the terms of the GNU General Public Li-
* cence as published by the Free Software Foundation; either
* version 2 of the Licence, or any later version.
*
* This program is distributed in the hope that it will be use-
* ful, but WITHOUT ANY WARRANTY; without even the implied war-
* ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public Licence for more details.
*
* You should have received a copy of the GNU General Public
* Licence along with this program; if not, write to the Free
* Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
* USA.
*
*/

#include <QIcon>
#include <QString>
#include <QFile>

#include "globals.h"

namespace core {
    extern "C" {
        #include "../config.h"
        #include "../main.h"
    }
}

QStringList romDirectories()
{
    QStringList dirs;
    for(int i = 0; i < core::config_get_number("NumRomDirs", 0); i++) {
        char buf[30];
        sprintf(buf, "RomDirectory[%d]", i);
        dirs << core::config_get_string(buf, "");
    }
    dirs.removeAll("");
    return dirs;
}

QIcon icon(const char* iconName)
{
    QIcon icon;
    QStringList sizes;
    sizes << "16x16" << "22x22" << "32x32" << "";
    foreach (QString size, sizes) {
        QString name = QString("%1/%2").arg(size).arg(iconName);
        QString filename = core::get_iconpath(qPrintable(name));
        if (QFile::exists(filename)) {
            icon.addFile(filename);
        }
    }
    return icon;
}