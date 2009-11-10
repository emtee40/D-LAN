#include <Tests.h>
using namespace FM;

#include <QtDebug>
#include <QDir>
#include <QStringList>

#include <Protos/common.pb.h>
#include <Common/PersistantData.h>
#include <Exceptions.h>
#include <priv/Constants.h>

Tests::Tests()
{
}

void Tests::initTestCase()
{
   Common::PersistantData::rmValue(FILE_CACHE); // Reset the stored cache.
   this->fileManager = Builder::newFileManager();
}

void Tests::addASharedDirectory()
{
   this->sharedDirs << QDir::currentPath().append("/../../sharedDirs/share1");
   this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   QStringList paths = this->fileManager->getSharedDirsReadOnly();
   QVERIFY(paths.size() == 1);
   QCOMPARE(paths.at(0), QDir::cleanPath(this->sharedDirs.at(0)));
}

void Tests::addAnAlreadySharedDirectory()
{
   this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   QStringList paths = this->fileManager->getSharedDirsReadOnly();
   QVERIFY(paths.size() == 1);
   QCOMPARE(paths.at(0), QDir::cleanPath(this->sharedDirs.at(0)));
}

void Tests::addInexistantSharedDirectory()
{
   this->sharedDirs << QDir::currentPath().append("/this_is_spartaaaaaa"); // This directory doesn't exit.
   try
   {
      this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   }
   catch (DirsNotFoundException& e)
   {
      QVERIFY(e.paths.size() == 1);
      QCOMPARE(e.paths.at(0), QDir::cleanPath(this->sharedDirs.last()));
      qDebug() << "This directory hasn't been found : " << e.paths.at(0) << " (Exception thrown)";
   }
   this->sharedDirs.removeLast();
}

void Tests::addSubSharedDirectories()
{
   this->sharedDirs << QDir::currentPath().append("/../../sharedDirs/share1/subdir");
   this->sharedDirs << QDir::currentPath().append("/../../sharedDirs/share1/anotherSubdir");
   try
   {
      this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   }
   catch(SuperDirectoryExistsException& e)
   {
      QCOMPARE(e.directory, QDir::cleanPath(this->sharedDirs.at(0)));
      qDebug() << "There is already a super directory : " << e.directory;
   }
   this->sharedDirs.removeLast();
   this->sharedDirs.removeLast();
}

void Tests::cleanupTestCase()
{
   // This call is only used to stop the fileUpdater and wait for it to finish.
   // It's should not be used in a normal code.
   this->fileManager.clear();
}

/*

   this->sharedDirs << QDir::currentPath().append("/asdasdasd"); // This directory doesn't exit.

   try
   {
      this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   }
   catch (DirsNotFoundException& e)
   {
      foreach (QString path, e.getPaths())
         qDebug() << "This directory hasn't been found : " << path << " (Exception thrown)";
   }

   this->sharedDirs.removeLast(); // Remove the nonexistent directory.
   */

void Tests::search()
{
   /**
     * Execute many times the search in parallel
     * with the indexing.
     */
   for (int i = 0; i < 20; i++)
   {
      this->printAmount();
      this->doASearch(false);
   }

   QTest::qSleep(500);

   this->doASearch(true);
   this->printAmount();

   this->sharedDirs << QDir::currentPath().append("/../../aSharedDir");
   this->fileManager->setSharedDirsReadWrite(this->sharedDirs);

   QTest::qSleep(500);

   QString terms("xxxx");
   Protos::Common::FindResult result = this->fileManager->find(terms);
   this->printSearch(terms, result);
}

void Tests::addSuperSharedDirectories()
{
   // TODO : doesn't work!!!!!!
   /*
   this->sharedDirs << QDir::currentPath().append("/../..");
   try
   {
      this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   }
   catch(SubDirectoriesWithDifferentRightsExistsException& e)
   {
      qDebug() << "There is already sub directories with different rights : "; // TODO : print more information
      this->sharedDirs.removeLast();
   }*/
}

void Tests::rmSharedDirectories()
{
   return;

   this->sharedDirs.clear();
//   this->fileManager->setSharedDirsReadOnly(this->sharedDirs);
   this->fileManager->setSharedDirsReadWrite(this->sharedDirs);
}

void Tests::addSuperSharedDirectoriesAndMerge()
{
   /*
   QStringList dirs;
   dirs << QDir::currentPath().append("/../../aSharedDir/subdir") <<
         QDir::currentPath().append("/../../aSharedDir/anotherSubdir") <<
         QDir::currentPath().append("/../../aSharedDir");
   this->fileManager2->setSharedDirsReadOnly(dirs);*/
}

void Tests::doASearch(bool checkResult)
{
   QString terms("aaaa bbbb cccc");
   Protos::Common::FindResult result = this->fileManager->find(terms);
   this->printSearch(terms, result);

   if (checkResult)
   {
      quint32 levelResults[] = {
         0, 1, 2, 3, 3, 4, 5, 5
      };
      QList<QString> fileResults[] = {
         QList<QString>() << "aaaa bbbb cccc.txt",
         QList<QString>() << "aaaa bbbb.txt",
         QList<QString>() << "aaaa cccc.txt",
         QList<QString>() << "cccc bbbbbb.txt" << "bbbb cccc.txt",
         QList<QString>() << "aaaa dddddd.txt",
         QList<QString>() << "bbbb dddd.txt" << "bbbb.txt"
      };
      for (int i = 0; i < result.file_size(); i++)
      {
         QVERIFY(result.file(i).level() == levelResults[i]);
         QVERIFY(fileResults[result.file(i).level()].contains(QString(result.file(i).file().file().name().data())));
      }
   }
}

void Tests::printSearch(const QString& terms, const Protos::Common::FindResult& result)
{
   qDebug() << "Search : '" << terms << "'";
   for (int i = 0; i < result.file_size(); i++)
      qDebug() << "[" << result.file(i).level() << "] " << result.file(i).file().file().name().data();
}

void Tests::printAmount()
{
   qDebug() << "Sharing amount : " << this->fileManager->getAmount() << " bytes";
}

