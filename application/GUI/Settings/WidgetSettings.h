/**
  * Aybabtu - A decentralized LAN file sharing software.
  * Copyright (C) 2010-2011 Greg Burri <greg.burri@gmail.com>
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
  
#ifndef GUI_WIDGETSETTINGS_H
#define GUI_WIDGETSETTINGS_H

#include <QWidget>
#include <QDir>

#include <Common/RemoteCoreController/ICoreConnection.h>

#include <Settings/DirListModel.h>

namespace Ui {
   class WidgetSettings;
}

namespace GUI
{
   class WidgetSettings : public QWidget
   {
      Q_OBJECT
   public:
      explicit WidgetSettings(QSharedPointer<RCC::ICoreConnection> coreConnection, QWidget *parent = 0);
      ~WidgetSettings();

      void coreConnected();
      void coreDisconnected();

   private slots:
      void newState(const Protos::GUI::State& state);
      void saveCoreSettings();
      void saveGUISettings();

      void addIncoming();
      void addShared();
      void removeIncoming();
      void removeShared();

      void resetCoreAddress();

      QStringList askForDirectories();

   protected:
      virtual void showEvent(QShowEvent* event);

   private:
      Ui::WidgetSettings *ui;

      DirListModel incomingDirsModel;
      DirListModel sharedDirsModel;

      QSharedPointer<RCC::ICoreConnection> coreConnection;

      bool initialState;
   };
}

#endif