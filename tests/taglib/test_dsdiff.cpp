#include <cstring>
#include <cstdio>
#include <cppunit/extensions/HelperMacros.h>

#include "tag.h"
#include "tbytevectorlist.h"
#include "dsdifffile.h"
#include "utils.h"

using namespace std;
using namespace Strawberry_TagLib::TagLib;

class TestDSDIFF : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestDSDIFF);
  CPPUNIT_TEST(testProperties);
  CPPUNIT_TEST(testTags);
  CPPUNIT_TEST(testSaveID3v2);
  CPPUNIT_TEST(testRepeatedSave);
  CPPUNIT_TEST_SUITE_END();

 public:
  void testProperties() {
    DSDIFF::File f(TEST_FILE_PATH_C("empty10ms.dff"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(10, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(5644, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(2822400, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL((long long)28224, f.audioProperties()->sampleCount());
  }

  void testTags() {
    ScopedFileCopy copy("empty10ms", ".dff");
    string newname = copy.fileName();

    DSDIFF::File *f = new DSDIFF::File(newname.c_str());
    CPPUNIT_ASSERT_EQUAL(String(""), f->tag()->artist());
    f->tag()->setArtist("The Artist");
    f->save();
    delete f;

    f = new DSDIFF::File(newname.c_str());
    CPPUNIT_ASSERT_EQUAL(String("The Artist"), f->tag()->artist());
    delete f;
  }

  void testSaveID3v2() {
    ScopedFileCopy copy("empty10ms", ".dff");
    string newname = copy.fileName();

    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());

      f.tag()->setTitle(L"TitleXXX");
      f.save();
      CPPUNIT_ASSERT(f.hasID3v2Tag());
    }
    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String(L"TitleXXX"), f.tag()->title());

      f.tag()->setTitle("");
      f.save();
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      f.tag()->setTitle(L"TitleXXX");
      f.save();
      CPPUNIT_ASSERT(f.hasID3v2Tag());
    }
    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String(L"TitleXXX"), f.tag()->title());
    }
  }

  void testRepeatedSave() {
    ScopedFileCopy copy("empty10ms", ".dff");
    string newname = copy.fileName();

    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String(""), f.tag()->title());
      f.tag()->setTitle("NEW TITLE");
      f.save();
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE"), f.tag()->title());
      f.tag()->setTitle("NEW TITLE 2");
      f.save();
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE 2"), f.tag()->title());
      CPPUNIT_ASSERT_EQUAL(8252LL, f.length());
      f.save();
      CPPUNIT_ASSERT_EQUAL(8252LL, f.length());
    }
    {
      DSDIFF::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE 2"), f.tag()->title());
    }
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestDSDIFF);
