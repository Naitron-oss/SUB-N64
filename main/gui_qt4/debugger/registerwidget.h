/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - registerwidget.h                                        *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2009 olejl                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __REGISTERWIDGET_H__
#define __REGISTERWIDGET_H__

#include <QWidget>
#include <QStringListModel>

#include "ui_registerwidget.h"

class TableListModel;

class RegisterWidget : public QWidget, private Ui_RegisterWidget
{
    Q_OBJECT
    public:
        RegisterWidget(QWidget* parent = 0);
        virtual ~RegisterWidget();

        void init_registers();
        void update_registers();
        void init_fgr(void);
        void update_fgr(void);
        long long int gui_fantom_fgr_64[32];


    private:
        QStringList stringlistGpr;
        QStringList stringlistHiLo;
        QStringList stringlistInterupt;

        QStringListModel *modelHiLo;
        QStringListModel *modelInterupt;
        TableListModel *modelGpr;
};
#endif // __REGISTERWIDGET_H__

