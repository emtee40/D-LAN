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
  
#ifndef DOWNLOADMANAGER_FILEDOWNLOAD_H
#define DOWNLOADMANAGER_FILEDOWNLOAD_H

#include <QList>
#include <QSharedPointer>

#include <Libs/MersenneTwister.h>

#include <Core/FileManager/IFileManager.h>
#include <Core/FileManager/IChunk.h>
#include <Core/PeerManager/IPeerManager.h>
#include <Core/PeerManager/IGetHashesResult.h>

#include <Protos/common.pb.h>

#include <priv/OccupiedPeers.h>
#include <priv/Download.h>
#include <priv/ChunkDownload.h>

namespace DM
{
   class DownloadManager;
   class FileDownload : public Download
   {
      Q_OBJECT
      static MTRand mtrand;
   public:
      FileDownload(
         QSharedPointer<FM::IFileManager> fileManager,
         QSharedPointer<PM::IPeerManager> peerManager,
         OccupiedPeers& occupiedPeersAskingForHashes,
         OccupiedPeers& occupiedPeersDownloadingChunk,
         Common::Hash peerSourceID,
         const Protos::Common::Entry& remoteEntry,
         const Protos::Common::Entry& localEntry,
         Common::TransferRateCalculator& transferRateCalculator,
         bool complete = false
      );
      ~FileDownload();

      void populateRemoteEntry(Protos::Queue::Queue_Entry* entry) const;

      void start();

      int getProgress() const;
      QSet<Common::Hash> getPeers() const;

      QSharedPointer<ChunkDownload> getAChunkToDownload();

      void getUnfinishedChunks(QList< QSharedPointer<IChunkDownload> >& chunks, int n) const;

   public slots:
      bool retreiveHashes();

   signals:
      void newHashKnown();

   protected slots:
      void retrievePeer();

   private slots:
      void result(const Protos::Core::GetHashesResult& result);
      void nextHash(const Common::Hash& hash);
      void getHashTimeout();

      void chunkDownloadStarted();
      void chunkDownloadFinished();

   protected:
      void setStatus(Status newStatus);

   private:
      void updateStatus();
      void connectChunkDownloadSignals(QSharedPointer<ChunkDownload> chunkDownload);

      const int NB_CHUNK;
      DownloadManager* downloadManager;

      QList< QSharedPointer<FM::IChunk> > chunksWithoutDownload;
      QList< QSharedPointer<ChunkDownload> > chunkDownloads;

      OccupiedPeers& occupiedPeersAskingForHashes;
      OccupiedPeers& occupiedPeersDownloadingChunk;

      int nbHashesKnown;
      QSharedPointer<PM::IGetHashesResult> getHashesResult;

      bool fileCreated;

      QTimer timer; // Used to periodically try to retrieve hashes.

      Common::TransferRateCalculator& transferRateCalculator;
   };
}
#endif
