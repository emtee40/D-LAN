#include <priv/FileUpdater/FileUpdater.h>
using namespace FM;

#include <QLinkedList>
#include <QDir>
#include <QTime>

#include <priv/log.h>
#include <priv/Exceptions.h>
#include <priv/Cache/SharedDirectory.h>
#include <priv/Cache/Directory.h>
#include <priv/Cache/File.h>
#include <priv/FileUpdater/WaitCondition.h>

FileUpdater::FileUpdater(FileManager* fileManager)
   : fileManager(fileManager), dirWatcher(DirWatcher::getNewWatcher())/*, currentScanningDir(0)*/
{
   this->dirEvent = WaitCondition::getNewWaitCondition();
}

FileUpdater::~FileUpdater()
{
   if (this->dirEvent)
      delete this->dirEvent;
}

void FileUpdater::addRoot(SharedDirectory* dir)
{
   QMutexLocker(&this->mutex);

   if (this->dirWatcher)
      this->dirWatcher->addDir(dir->getFullPath());
   else
      if (!QDir(dir->getFullPath()).exists())
         throw DirNotFoundException(dir->getFullPath());

   this->dirsToScan << dir;

   this->dirEvent->release();
}

void FileUpdater::rmRoot(SharedDirectory* dir)
{
   QMutexLocker(&this->mutex);

   // If there is a scanning for this directory stop it.
   // (Not used for the moment, maybe reactive later if useful)
   /*this->scanningMutex.lock();
   if (this->currentScanningDir == dir)
   {
      this->currentScanningDir = 0;
      this->scanningStopped.wait(&this->scanningMutex);
   }
   this->scanningMutex.unlock();*/

   this->dirsToRemove << dir;

   this->dirEvent->release();
}

void FileUpdater::run()
{
   forever
   {
      this->computeSomeHashes();

      this->mutex.lock();

      if (!this->dirsToRemove.isEmpty())
      {
         // Stop watching this directory.
         if (this->dirWatcher)
            this->dirWatcher->rmDir(this->dirsToRemove.takeLast()->getFullPath());
      }

      // If there is no watcher capability or no directory to watch then
      // we wait for an added directory.
      if (!this->dirWatcher || this->dirWatcher->nbWatchedDir() == 0 || !this->dirsToScan.empty())
      {
         if (this->dirsToScan.isEmpty())
         {
            LOG_DEBUG("Waiting for a new shared directory added..");
            this->mutex.unlock();

            this->dirEvent->wait();
         }
         else
            this->mutex.unlock();

         SharedDirectory* addedDir = 0;
         this->mutex.lock();
         if (!this->dirsToScan.isEmpty())
            addedDir = this->dirsToScan.takeLast();
         this->mutex.unlock();

         // Synchronize the new directory.
         if (addedDir)
            this->scan(addedDir);
      }
      else // Wait for filesystem modifications.
      {
         // If we have no dir to scan and no file to hash we wait for a new shared file
         // or a filesystem event.
         if (this->dirsToScan.isEmpty() && this->fileWithoutHashes.isEmpty())
         {
            this->mutex.unlock();
            this->treatEvents(this->dirWatcher->waitEvent(this->dirEvent));
         }
         else
         {
            this->mutex.unlock();
            this->treatEvents(this->dirWatcher->waitEvent(0));
         }
      }
   }
}

void FileUpdater::createNewFile(Directory* dir, const QString& filename, qint64 size)
{
   File* file = new File(dir, filename, size);
   this->fileWithoutHashes << file;
}

void FileUpdater::computeSomeHashes()
{
   QTime time;
   time.start();

   QMutableLinkedListIterator<File*> i(this->fileWithoutHashes);
   while (i.hasNext())
   {
      i.next()->computeHashes();
      i.remove();

      if (time.elapsed() / 1000 >= MINIMUM_DURATION_WHEN_HASHING)
         break;
   }
}

void FileUpdater::scan(SharedDirectory* dir)
{
   LOG_DEBUG("Start scanning a shared directory : " + dir->getFullPath());

   if (dir->isDeleted())
   {
      LOG_DEBUG("Cannot scan a deleted shared directory : " + dir->getFullPath());
      return;
   }

   /*this->scanningMutex.lock();
   this->currentScanningDir = dir;
   this->scanningMutex.unlock();*/

   QLinkedList<Directory*> dirsToVisit;
   dirsToVisit << dir;

   while (!dirsToVisit.isEmpty())
   {
      /*this->scanningMutex.lock();
      if (!this->currentScanningDir)
      {
         this->scanningStopped.wakeOne();
         this->scanningMutex.unlock();
         LOG_DEBUG("Cannot scan a deleted shared directory : " + dir->getFullPath());
         return;
      }
      this->scanningMutex.unlock();*/

      Directory* currentDir = dirsToVisit.takeFirst();
      foreach (QFileInfo entry, QDir(currentDir->getFullPath()).entryInfoList())
      {
         if (entry.fileName() == "." || entry.fileName() == "..")
            continue;

         if (entry.isDir())
         {
            dirsToVisit << new Directory(currentDir, entry.fileName());
         }
         else
         {
            this->createNewFile(currentDir, entry.fileName(), entry.size());
         }
      }
   }
   LOG_DEBUG("Scan terminated : " + dir->getFullPath());

   /*this->scanningMutex.lock();
   this->currentScanningDir = 0;
   this->scanningStopped.wakeOne();
   this->scanningMutex.unlock();*/
}

void FileUpdater::treatEvents(const QList<WatcherEvent>& events)
{
   // TODO something
   LOG_DEBUG("File structure event occurs");
}
