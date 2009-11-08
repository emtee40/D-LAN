#ifndef FILEMANAGER_FILE_H
#define FILEMANAGER_FILE_H

#include <exception>

#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QFile>
#include <QList>
#include <QSharedPointer>

#include <Common/Hashes.h>
#include <Protos/common.pb.h>
#include <Protos/files_cache.pb.h>
#include <priv/Cache/Entry.h>

namespace FM
{
   class IChunk;
   class Chunk;
   class Directory;
   class Cache;

   class File : public Entry
   {
   public:

      /**
        * Create a new file into a given directory.
        * The file may or may not have a correponding local file.
        */
      File(Directory* dir, const QString& name, qint64 size, const Common::Hashes& hashes = Common::Hashes());

   private:
      virtual ~File();

   public:

      File* restoreFromFileCache(const Protos::FileCache::Hashes_File& file);

      void populateHashesFile(Protos::FileCache::Hashes_File& fileToFill) const;

      void populateFileEntry(Protos::Common::FileEntry* entry) const;

      /**
        * Set as deleted and delete all chunks.
        */
      void setDeleted();

      /**
        * Called only by its chunks.
        * If the file is marked as deleted and has no more chunk
        * it will commit a suicide.
        */
      void chunkDeleted(Chunk* chunk);

      QString getPath() const;
      QString getFullPath() const;
      Directory* getRoot() const;

      void newDataWriterCreated();
      void newDataReaderCreated();

      void dataWriterDeleted();
      void dataReaderDeleted();

      /**
        * Write some bytes to the file at the given offset.
        * If the buffer exceed the file size then only the begining of the buffer is
        * used, the file is not resizing.
        * @param buffer The buffer.
        * @param offset An offset.
        * @return true if end of file reached.
        */
      bool write(const QByteArray& buffer, qint64 offset);

      /**
        * Fill the buffer with the read bytes from the given offset.
        * If the end of file is reached the buffer will be partialy filled.
        * @param buffer The buffer.
        * @param offset An offset.
        * @return the number of bytes read.
        */
      qint64 read(QByteArray& buffer, qint64 offset);

      /**
        * It will open the file, read it and calculate all theirs chunk hashes.
        * If it already owns some chunks, there are destroyed first.
        * This method can be called from an another thread than the main one. For example,
        * from 'FileUpdated' thread.
        * @exception FileNotFoundException
        */
      void computeHashes();

      /*QList<IChunk*> getChunks() const;
      const QList<Chunk*>& getChunksRef() const;*/

      bool haveAllHashes();

   private:
      Directory* dir;
      QList< QSharedPointer<Chunk> > chunks;

      int numDataWriter;
      int numDataReader;
      QFile* fileInWriteMode;
      QFile* fileInReadMode;
      QMutex* writeLock;
      QMutex* readLock;

      int nbChunks;

      // Mutex and wait condition used during hashing.
      // (TODO : It's a bit heavy, try to reduce the memory footprint).
      bool hashing;
      QWaitCondition hashingStopped;
      QMutex hashingMutex;
   };
}
#endif
