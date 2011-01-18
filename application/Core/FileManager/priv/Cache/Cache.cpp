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
  
#include <priv/Cache/Cache.h>
using namespace FM;

#include <QDir>

#include <Common/Global.h>
#include <Common/Settings.h>
#include <Common/ProtoHelper.h>

#include <Exceptions.h>
#include <priv/Log.h>
#include <priv/Exceptions.h>
#include <priv/Constants.h>
#include <priv/Cache/SharedDirectory.h>
#include <priv/Cache/File.h>

Cache::Cache(FileManager* fileManager)
   : fileManager(fileManager), mutex(QMutex::Recursive)
{
}

/**
  * a) Search among their shared directory the one who match the given entry.
  * b) In the shared directory try to find the directory corresponding to 'entry.dir.path'.
  * c) Populate the result with directories and files.
  */
Protos::Common::Entries Cache::getEntries(const Protos::Common::Entry& dir) const
{
   Protos::Common::Entries result;

   // If we can't find the shared directory..
   if (!dir.has_shared_dir())
      return result;

   QMutexLocker locker(&this->mutex);

   foreach (SharedDirectory* sharedDir, this->sharedDirs)
   {
      if (sharedDir->getId() == dir.shared_dir().id().hash().data())
      {
         QStringList folders = QDir::cleanPath(Common::ProtoHelper::getStr(dir, &Protos::Common::Entry::path)).split('/', QString::SkipEmptyParts);

         if (!dir.path().empty()) // An empty path means the dir is the root (a SharedDirectory).
            folders << Common::ProtoHelper::getStr(dir, &Protos::Common::Entry::name);

         Directory* currentDir = sharedDir;
         foreach (QString folder, folders)
         {
            currentDir = currentDir->getSubDir(folder);
            if (!currentDir)
               return result;
         }

         foreach (Directory* dir, currentDir->getSubDirs())
            dir->populateEntry(result.add_entry());

         foreach (File* file, currentDir->getFiles())
         {
            if (file->isComplete())
               file->populateEntry(result.add_entry());
         }

         return result;
      }
   }

   return result;
}

Protos::Common::Entries Cache::getEntries() const
{
   QMutexLocker locker(&this->mutex);

   Protos::Common::Entries result;

   foreach (SharedDirectory* sharedDir, this->sharedDirs)
   {
      Protos::Common::Entry* entry = result.add_entry();
      sharedDir->populateEntry(entry);
   }

   return result;
}

/**
  * @path the absolute path to a directory or a file.
  * @return Returns 0 if no entry found.
  */
Entry* Cache::getEntry(const QString& path) const
{
   QMutexLocker locker(&this->mutex);

   foreach (SharedDirectory* sharedDir, this->sharedDirs)
   {
      if (path.startsWith(sharedDir->getFullPath()))
      {
         QString relativePath(path);
         relativePath.remove(0, sharedDir->getFullPath().size());
         const QStringList folders = relativePath.split('/', QString::SkipEmptyParts);

         Directory* currentDir = sharedDir;
         for (QStringListIterator i(folders); i.hasNext();)
         {
            QString folder = i.next();
            Directory* dir = currentDir->getSubDir(folder);
            if (!dir)
            {
               if (!i.hasNext())
               {
                  File* file = currentDir->getFile(folder);
                  if (file)
                     return file;
               }
               return 0;
            }
            currentDir = dir;
         }

         return currentDir;
      }
   }

   return 0;
}

/**
  * Try to find the file into the cache from a reference.
  */
File* Cache::getFile(const Protos::Common::Entry& fileEntry) const
{
   QMutexLocker locker(&this->mutex);

   if (!fileEntry.has_shared_dir())
   {
      L_WARN(QString("Cache::getFile : 'fileEntry' doesn't have the field 'shared_dir' set!"));
      return 0;
   }

   foreach (SharedDirectory* sharedDir, this->sharedDirs)
   {
      if (sharedDir->getId() == fileEntry.shared_dir().id().hash().data())
      {
         QString relativePath(Common::ProtoHelper::getStr(fileEntry, &Protos::Common::Entry::path));
         const QStringList folders = relativePath.split('/', QString::SkipEmptyParts);

         Directory* dir = sharedDir;
         QStringListIterator i(folders);
         forever
         {
            if (dir)
            {
               if (!i.hasNext())
               {
                  File* file = dir->getFile(Common::ProtoHelper::getStr(fileEntry, &Protos::Common::Entry::name));

                  if (file)
                     return file;
                  return 0;
               }
            }
            else
               return 0;

            if (!i.hasNext())
               return 0;

            dir = dir->getSubDir(i.next());
         }

         return 0;
      }
   }

   return 0;
}

QList< QSharedPointer<IChunk> > Cache::newFile(Protos::Common::Entry& fileEntry)
{
   QMutexLocker locker(&this->mutex);

   const QString dirPath = QDir::cleanPath(Common::ProtoHelper::getStr(fileEntry, &Protos::Common::Entry::path));

   // If we know where to put the file.
   Directory* dir = 0;
   if (fileEntry.has_shared_dir())
   {
      SharedDirectory* sharedDir = this->getSharedDirectory(fileEntry.shared_dir().id().hash().data());
      if (sharedDir)
         dir = sharedDir->createSubDirectories(dirPath.split('/', QString::SkipEmptyParts), true);
      else
         fileEntry.clear_shared_dir(); // The shared directory is invalid.
   }

   if (!dir)
      dir = this->getWriteableDirectory(dirPath, fileEntry.size() + SETTINGS.get<quint32>("minimum_free_space"));

   if (!dir)
      throw UnableToCreateNewFileException();

   Common::Hashes hashes;
   for (int i = 0; i < fileEntry.chunk_size(); i++)
      hashes << (fileEntry.chunk(i).has_hash() ? Common::Hash(fileEntry.chunk(i).hash().data()) : Common::Hash());

   const QString name = Common::ProtoHelper::getStr(fileEntry, &Protos::Common::Entry::name);

   // If a file with the same name already exists it will be reset.
   File* file;
   if (file = dir->getFile(name))
   {
      file->setToUnfinished(fileEntry.size(), hashes);
   }
   else
   {
      file = new File(
         dir,
         name,
         fileEntry.size(),
         QDateTime::currentDateTime(),
         hashes,
         true
      );
   }

   fileEntry.set_exists(true); // File has been physically created.
   dir->populateEntrySharedDir(&fileEntry); // We set the shared directory.

   // TODO : is there a better way to up cast ?
   QList< QSharedPointer<IChunk> > chunks;
   foreach (QSharedPointer<Chunk> chunk, file->getChunks())
      chunks << chunk;
   return chunks;
}

/**
  * Access the chunks of a file in an atomic way.
  */
QList< QSharedPointer<Chunk> > Cache::getChunks(const Protos::Common::Entry& fileEntry)
{
   QMutexLocker locker(&this->mutex);

   File* file = this->getFile(fileEntry);
   if (!file)
      return QList< QSharedPointer<Chunk> >();

   return file->getChunks();
}

QList<Common::SharedDir> Cache::getSharedDirs() const
{
   QMutexLocker locker(&this->mutex);

   QList<Common::SharedDir> list;

   for (QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
   {
      SharedDirectory* dir = i.next();
      list << Common::SharedDir(dir->getId(), dir->getFullPath());
   }
   return list;
}

SharedDirectory* Cache::getSharedDirectory(const Common::Hash& ID) const
{
   for (QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
   {
      SharedDirectory* dir = i.next();
      if (dir->getId() == ID)
         return dir;
   }
   return 0;
}

/**
  * @exception DirsNotFoundException
  */
void Cache::setSharedDirs(const QStringList& dirs)
{
   QMutexLocker locker(&this->mutex);

   // Filter the actual shared directories by looking theirs rights.
   QList<SharedDirectory*> sharedDirsCopy(this->sharedDirs);

   QMutableListIterator<SharedDirectory*> j(sharedDirsCopy);

   // Remove already shared directories from 'sharedDirsCopy'.
   // /!\ O(n^2).
   for(QListIterator<QString> i(dirs); i.hasNext();)
   {
      QString dir =  QDir::cleanPath(i.next());
      j.toFront();
      while(j.hasNext())
      {
         SharedDirectory* sharedDir = j.next();
         if (sharedDir->getFullPath() == dir)
         {
            j.remove();
            break;
         }
      }
   }

   // Remove shared directories.
   for (j.toFront(); j.hasNext();)
   {
      SharedDirectory* dir = j.next();
      L_DEBU("Remove a shared directory : " + dir->getFullPath());
      this->removeSharedDir(dir);
   }

   // The duplicate dirs are found further.
   this->createSharedDirs(dirs);
}

/**
  * Will inform the fileUpdater and delete 'dir'.
  * If 'dir2' is given 'dir' content (sub dirs + files) will be give to 'dir2'.
  * The directory is deleted by fileUpdater.
  */
void Cache::removeSharedDir(SharedDirectory* dir, Directory* dir2)
{
   QMutexLocker locker(&this->mutex);

   this->sharedDirs.removeOne(dir);

   emit sharedDirectoryRemoved(dir, dir2);
}

SharedDirectory* Cache::getSuperSharedDirectory(const QString& path) const
{
   QMutexLocker locker(&this->mutex);
   const QStringList& folders = path.split('/', QString::SkipEmptyParts);

   for (QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
   {
      SharedDirectory* sharedDir = i.next();
      const QStringList& foldersShared = sharedDir->getFullPath().split('/', QString::SkipEmptyParts);
      if (folders.size() <= foldersShared.size())
         continue;

      for (int i = 0; i < foldersShared.size(); i++)
         if (folders[i] != foldersShared[i])
            goto nextSharedDir;

      return sharedDir;
      nextSharedDir:;
   }

   return 0;
}

QList<SharedDirectory*> Cache::getSubSharedDirectories(const QString& path) const
{
   QMutexLocker locker(&this->mutex);
   QList<SharedDirectory*> ret;

   const QStringList& folders = path.split('/', QString::SkipEmptyParts);

   for (QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
   {
      SharedDirectory* sharedDir = i.next();
      const QStringList& foldersShared = sharedDir->getFullPath().split('/', QString::SkipEmptyParts);

      if (foldersShared.size() <= folders.size())
         continue;

      for (int i = 0; i < folders.size(); i++)
         if (folders[i] != foldersShared[i])
            goto nextSharedDir;

      ret << sharedDir;

      nextSharedDir:;
   }

   return ret;
}

/**
  * If path matches a shared directory or one of its sub directories then true is returned.
  */
bool Cache::isShared(const QString& path) const
{
   QMutexLocker locker(&this->mutex);
   foreach (SharedDirectory* dir, this->sharedDirs)
      if (dir->getFullPath() == path)
         return true;
   return false;
}

/**
  * Returns the directory that best matches to the given path.
  * For example, path = /home/peter/linux/distrib/debian/etch
  *  This directory exists in cache : /home/peter/linux/distrib
  *  Thus, this directory 'distrib' will be returned.
  * @param path An absolute path.
  * @return If no directory can be match 0 is returned.
  */
Directory* Cache::getFittestDirectory(const QString& path) const
{
   QMutexLocker locker(&this->mutex);

   foreach (SharedDirectory* sharedDir, this->sharedDirs)
   {
      if (path.startsWith(sharedDir->getFullPath()))
      {
         QString relativePath(path);
         relativePath.remove(0, sharedDir->getFullPath().size());
         const QStringList folders = relativePath.split('/', QString::SkipEmptyParts);

         Directory* currentDir = sharedDir;
         foreach (QString folder, folders)
         {
            Directory* nextdir = currentDir->getSubDir(folder);
            if (!nextdir)
               break;
            currentDir = nextdir;
         }
         return currentDir;
      }
   }

   return 0;
}

/**
  * Define the shared directories from the persisted given data.
  * The directories and files are not created here but later by the fileUpdater, see the FileManager ctor.
  */
void Cache::retrieveFromFile(const Protos::FileCache::Hashes& hashes)
{
   this->createSharedDirs(hashes);
}

/**
  * Populate the given structure to be persisted later.
  */
void Cache::saveInFile(Protos::FileCache::Hashes& hashes) const
{
   QMutexLocker locker(&this->mutex);

   hashes.set_version(FILE_CACHE_VERSION);
   hashes.set_chunksize(SETTINGS.get<quint32>("chunk_size"));

   for (QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
   {
      SharedDirectory* sharedDir = i.next();
      Protos::FileCache::Hashes_SharedDir* sharedDirMess = hashes.add_shareddir();
      sharedDirMess->mutable_id()->set_hash(sharedDir->getId().getData(), Common::Hash::HASH_SIZE);
      Common::ProtoHelper::setStr(*sharedDirMess, &Protos::FileCache::Hashes_SharedDir::set_path, sharedDir->getFullPath());

      sharedDir->populateHashesDir(*sharedDirMess->mutable_root());
   }
}

quint64 Cache::getAmount() const
{
   QMutexLocker locker(&this->mutex);

   quint64 amount = 0;
   for(QListIterator<SharedDirectory*> i(this->sharedDirs); i.hasNext();)
      amount += i.next()->getSize();
   return amount;
}

void Cache::onEntryAdded(Entry* entry)
{
   emit entryAdded(entry);
}

void Cache::onEntryRemoved(Entry* entry)
{
   if (SharedDirectory* sharedDir = dynamic_cast<SharedDirectory*>(entry))
   {
      QMutexLocker locker(&this->mutex);
      this->sharedDirs.removeOne(sharedDir);
   }

   emit entryRemoved(entry);
}

void Cache::onChunkHashKnown(QSharedPointer<Chunk> chunk)
{
   emit chunkHashKnown(chunk);
}

void Cache::onChunkRemoved(QSharedPointer<Chunk> chunk)
{
   emit chunkRemoved(chunk);
}

/**
  * Create new shared directories and inform the fileUpdater.
  * @exception DirsNotFoundException
  */
void Cache::createSharedDirs(const QStringList& dirs, const QList<Common::Hash>& ids)
{
   QMutexLocker locker(&this->mutex);

   QStringList dirsNotFound;

   QListIterator<QString> i(dirs);
   QListIterator<Common::Hash> k(ids);
   while (i.hasNext())
   {
      QString path = i.next();

      try
      {
         SharedDirectory* dir = k.hasNext() ?
            new SharedDirectory(this, path, k.next()) :
            new SharedDirectory(this, path);

         L_DEBU(QString("Add a new shared directory : %1").arg(path));
         this->sharedDirs << dir;

         emit newSharedDirectory(dir);
      }
      catch (DirNotFoundException& e)
      {
         L_WARN(QString("Directory not found : %1").arg(e.path));
         dirsNotFound << e.path;
      }
      catch (DirAlreadySharedException&)
      {
         L_DEBU(QString("Directory already shared : %1").arg(path));
      }
   }

   if (!dirsNotFound.isEmpty())
      throw DirsNotFoundException(dirsNotFound);
}

void Cache::createSharedDirs(const Protos::FileCache::Hashes& hashes)
{
   QStringList paths;
   QList<Common::Hash> ids;

   // Add the shared directories from the file cache.
   for (int i = 0; i < hashes.shareddir_size(); i++)
   {
      const Protos::FileCache::Hashes_SharedDir& dir = hashes.shareddir(i);
      paths << Common::ProtoHelper::getStr(dir, &Protos::FileCache::Hashes_SharedDir::path);
      ids << Common::Hash(dir.id().hash().data());
   }
   this->createSharedDirs(paths, ids);
}

/**
  * Returns a directory which matches to the path, it will choose the shared directory which :
  *  - Has at least the needed space.
  *  - Has the most directories in common with 'path'.
  *
  * The missing directories will be automatically created.
  *
  * @param path A relative path to a directory. Must be a cleaned path (QDir::cleanPath).
  * @return The directory, 0 if unkown error.
  * @exception InsufficientStorageSpaceException
  * @exception NoWriteableDirectoryException
  */
Directory* Cache::getWriteableDirectory(const QString& path, qint64 spaceNeeded) const
{
   QMutexLocker locker(&this->mutex);

   const QStringList folders = path.split('/', QString::SkipEmptyParts);

   if (this->sharedDirs.isEmpty())
      throw NoWriteableDirectoryException();

   // Search for the best fitted shared directory.
   SharedDirectory* currentSharedDir = 0;
   int currentNbDirsInCommon = -1;

   foreach (SharedDirectory* dir, this->sharedDirs)
   {
      if (Common::Global::availableDiskSpace(dir->getFullPath()) < spaceNeeded)
         continue;

      Directory* currentDir = dir;
      int nbDirsInCommon = 0;
      foreach (QString folder, folders)
      {
         currentDir = currentDir->getSubDir(folder);
         if (currentDir)
            nbDirsInCommon += 1;
         else
            break;
      }
      if (nbDirsInCommon > currentNbDirsInCommon)
      {
         currentNbDirsInCommon = nbDirsInCommon;
         currentSharedDir = dir;
      }
   }

   if (!currentSharedDir)
      throw InsufficientStorageSpaceException();

   // Create the missing directories.
   return currentSharedDir->createSubDirectories(folders, true);
}
