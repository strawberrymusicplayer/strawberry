/*
 * Strawberry Music Player
 * Copyright 2024, Octavio Calleya Garcia <ogarcia.extern@autofleetcontrol.de>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "gtest_include.h"
#include "gmock_include.h"
#include "test_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include "core/song.h"
#include "includes/shared_ptr.h"
#include "playlistparsers/m3uparser.h"
#include "playlistparsers/parserbase.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// Writes text content to a file.
bool WriteFile(const QString &path, const QString &content) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  f.write(content.toUtf8());
  return true;
}

class M3UParserTest : public ::testing::Test {
 protected:
  ~M3UParserTest() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    tagreader_client_->deleteLater();
    tagreader_client_thread_->deleteLater();
  }

  void SetUp() override {
    tagreader_client_ = new TagReaderClient();
    tagreader_client_thread_ = new QThread();
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  // Constructs a parser with the test TagReaderClient and no collection backend.
  // The shared_ptr uses a no-op deleter because tagreader_client_ lifetime is managed by the fixture (deleteLater in the destructor).
  M3UParser MakeParser() const {
    return M3UParser(SharedPtr<TagReaderClient>(tagreader_client_, [](TagReaderClient *) {}), nullptr);
  }

  // Loads a parent playlist file via M3UParser::Load, mirroring the real SongLoader call site.
  static ParserBase::LoadResult Load(M3UParser &parser, const QString &playlist_path) {
    QFile file(playlist_path);
    if (!file.open(QIODevice::ReadOnly)) return ParserBase::LoadResult();
    return parser.Load(&file, playlist_path, QFileInfo(playlist_path).dir(), /*collection_lookup=*/false);
  }

  QThread *tagreader_client_thread_ = nullptr;
  TagReaderClient *tagreader_client_ = nullptr;
};

// M3UParser returns all resolved audio tracks from every referenced child playlist.
TEST_F(M3UParserTest, NestedExpansionLoadsAllChildTracks) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  // Two real leaf audio files.
  TemporaryResource leaf1(u":/audio/strawberry.mp3"_s);
  TemporaryResource leaf2(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf1.isOpen());
  ASSERT_TRUE(leaf2.isOpen());

  // child_a.m3u -> leaf1
  const QString child_a = tmp.filePath(u"child_a.m3u"_s);
  ASSERT_TRUE(WriteFile(child_a, leaf1.fileName() + u'\n'));

  // child_b.m3u -> leaf2
  const QString child_b = tmp.filePath(u"child_b.m3u"_s);
  ASSERT_TRUE(WriteFile(child_b, leaf2.fileName() + u'\n'));

  // parent.m3u -> child_a, child_b
  const QString parent = tmp.filePath(u"parent.m3u"_s);
  ASSERT_TRUE(WriteFile(parent, u"child_a.m3u\nchild_b.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent);
  EXPECT_EQ(result.songs.size(), 2);
}

// M3UParser preserves top-to-bottom source order when mixing direct audio entries with nested playlist references.
TEST_F(M3UParserTest, MixedAudioAndReferencesPreservesOrder) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf_direct(u":/audio/strawberry.mp3"_s);
  TemporaryResource leaf_nested(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf_direct.isOpen());
  ASSERT_TRUE(leaf_nested.isOpen());

  // child.m3u -> leaf_nested
  const QString child = tmp.filePath(u"child.m3u"_s);
  ASSERT_TRUE(WriteFile(child, leaf_nested.fileName() + u'\n'));

  // parent.m3u: direct track first, then nested reference
  const QString parent = tmp.filePath(u"parent.m3u"_s);
  ASSERT_TRUE(WriteFile(parent, leaf_direct.fileName() + u"\nchild.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent);
  ASSERT_EQ(result.songs.size(), 2);
  // The direct track comes first; the nested track comes second.
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf_direct.fileName()));
  EXPECT_EQ(result.songs[1].url(), QUrl::fromLocalFile(leaf_nested.fileName()));
}

// M3UParser resolves a child playlist's relative entries against the child's own directory, not the parent's.
TEST_F(M3UParserTest, PerFileResolutionUsesChildDirectory) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  // Layout:
  //   tmp/parent/parent.m3u  -> ../child/child.m3u
  //   tmp/child/child.m3u    -> leaf.mp3  (relative, same dir as child.m3u)
  //   tmp/child/leaf.mp3     (real audio)

  const QString parent_dir_path = tmp.filePath(u"parent"_s);
  const QString child_dir_path = tmp.filePath(u"child"_s);
  ASSERT_TRUE(QDir().mkpath(parent_dir_path));
  ASSERT_TRUE(QDir().mkpath(child_dir_path));

  // Copy the real audio resource to child/leaf.mp3.
  const QString leaf_path = child_dir_path + u"/leaf.mp3"_s;
  {
    QFile resource(u":/audio/strawberry.mp3"_s);
    ASSERT_TRUE(resource.open(QIODevice::ReadOnly));
    QFile dest(leaf_path);
    ASSERT_TRUE(dest.open(QIODevice::WriteOnly));
    dest.write(resource.readAll());
  }

  // child/child.m3u references leaf.mp3 by relative path (same directory).
  const QString child_m3u = child_dir_path + u"/child.m3u"_s;
  ASSERT_TRUE(WriteFile(child_m3u, u"leaf.mp3\n"_s));

  // parent/parent.m3u references child via a relative path that crosses directories.
  const QString parent_m3u = parent_dir_path + u"/parent.m3u"_s;
  ASSERT_TRUE(WriteFile(parent_m3u, u"../child/child.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent_m3u);
  ASSERT_EQ(result.songs.size(), 1);
  // The resolved URL must point at child/leaf.mp3, not parent/leaf.mp3.
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(QFileInfo(leaf_path).absoluteFilePath()));
}

// M3UParser terminates cleanly on a cyclic reference (A -> B -> A) and returns whatever tracks resolved before the cycle guard tripped.
TEST_F(M3UParserTest, CycleDetectionTerminates) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  // a.m3u -> leaf + b.m3u
  const QString a_path = tmp.filePath(u"a.m3u"_s);
  const QString b_path = tmp.filePath(u"b.m3u"_s);

  ASSERT_TRUE(WriteFile(a_path, leaf.fileName() + u"\nb.m3u\n"_s));
  // b.m3u -> a.m3u (cycle back to parent)
  ASSERT_TRUE(WriteFile(b_path, u"a.m3u\n"_s));

  M3UParser parser = MakeParser();
  // Must not hang; the cycle guard skips the back-reference.
  const ParserBase::LoadResult result = Load(parser, a_path);
  // Exactly the leaf track is returned; the cyclic back-reference to a.m3u is skipped.
  ASSERT_EQ(result.songs.size(), 1);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf.fileName()));
}

// M3UParser stops descending exactly once kMaxNestingDepth is reached.
// A leaf reachable at the last allowed depth still loads while one just beyond the cap does not, which pins the guard's boundary and catches an off-by-one in either direction.
TEST_F(M3UParserTest, DepthCapStopsRecursion) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource reachable_leaf(u":/audio/strawberry.mp3"_s);
  TemporaryResource unreachable_leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(reachable_leaf.isOpen());
  ASSERT_TRUE(unreachable_leaf.isOpen());

  // Build a chain root.m3u -> level0.m3u -> ... -> level(cap-1).m3u, entered at depths 0..cap-1.
  // level(cap-1) is the deepest playlist LoadNested is allowed to open, so a direct track it lists still loads.
  // level(cap-1) also references level(cap).m3u, whose LoadNested is entered at depth=cap and trips the guard, so that deeper leaf is never loaded.

  const int cap = M3UParser::kMaxNestingDepth;

  // level(cap).m3u -> unreachable leaf (the depth guard fires before this file is opened).
  const QString deepest = tmp.filePath(QStringLiteral("level%1.m3u").arg(cap));
  ASSERT_TRUE(WriteFile(deepest, unreachable_leaf.fileName() + u'\n'));

  // level(cap-1).m3u lists both the over-cap reference and a directly-reachable leaf.
  const QString last_allowed = tmp.filePath(QStringLiteral("level%1.m3u").arg(cap - 1));
  ASSERT_TRUE(WriteFile(last_allowed, QStringLiteral("level%1.m3u\n").arg(cap) + reachable_leaf.fileName() + u'\n'));

  // Build the rest of the chain from cap-2 down to 0, each pointing at the next level.
  for (int i = cap - 2; i >= 0; --i) {
    const QString cur = tmp.filePath(QStringLiteral("level%1.m3u").arg(i));
    ASSERT_TRUE(WriteFile(cur, QStringLiteral("level%1.m3u\n").arg(i + 1)));
  }

  // root.m3u -> level0.m3u
  const QString root = tmp.filePath(u"root.m3u"_s);
  ASSERT_TRUE(WriteFile(root, u"level0.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, root);
  // Only the leaf reachable at the last allowed depth loads; the one beyond the cap does not.
  ASSERT_EQ(result.songs.size(), 1);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(reachable_leaf.fileName()));
}

// M3UParser skips a missing nested .m3u reference with a warning; sibling entries in the same parent still load.
TEST_F(M3UParserTest, MissingNestedReferenceIsSkipped) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  // parent.m3u: a bad reference followed by a real track.
  const QString parent = tmp.filePath(u"parent.m3u"_s);
  ASSERT_TRUE(WriteFile(parent, u"nonexistent.m3u\n"_s + leaf.fileName() + u'\n'));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent);
  // The bad reference is skipped; the sibling leaf is still loaded.
  EXPECT_EQ(result.songs.size(), 1);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf.fileName()));
}

// A remote .m3u8 stream URL carries a URL scheme, so nested-playlist detection must leave it for the track path (LoadSong) rather than treating it as a local playlist to expand — otherwise HLS/remote stream references are silently dropped.
TEST_F(M3UParserTest, RemoteStreamReferenceLoadsAsTrack) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  // parent.m3u: a real local track followed by a remote .m3u8 stream URL.
  const QString stream_url = u"http://example.com/stream.m3u8"_s;
  const QString parent = tmp.filePath(u"parent.m3u"_s);
  ASSERT_TRUE(WriteFile(parent, leaf.fileName() + u'\n' + stream_url + u'\n'));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent);
  // Both entries survive: the local leaf and the remote stream.
  ASSERT_EQ(result.songs.size(), 2);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf.fileName()));
  EXPECT_TRUE(result.songs[1].is_stream());
  EXPECT_EQ(result.songs[1].url(), QUrl(stream_url));
}

// The same nested playlist referenced more than once (a repeated or diamond reference, not a cycle) must expand at every occurrence — only a reference back to a file on the active recursion path is a cycle.
TEST_F(M3UParserTest, RepeatedNonCyclicReferenceExpandsEachTime) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  // child.m3u -> one real leaf track.
  const QString child = tmp.filePath(u"child.m3u"_s);
  ASSERT_TRUE(WriteFile(child, leaf.fileName() + u'\n'));

  // parent.m3u references child.m3u twice, e.g. a compilation that opens and closes with the same sub-playlist.
  // Both occurrences must load, not be dropped as a false cycle.
  const QString parent = tmp.filePath(u"parent.m3u"_s);
  ASSERT_TRUE(WriteFile(parent, u"child.m3u\nchild.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, parent);
  ASSERT_EQ(result.songs.size(), 2);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf.fileName()));
  EXPECT_EQ(result.songs[1].url(), QUrl::fromLocalFile(leaf.fileName()));
}

// A file whose expansion was truncated by the depth cap deep in the tree must not have that truncated result reused at a shallower reference that still has depth budget to expand fully.
// The cache is keyed by remaining depth budget, so the shallow reference re-expands instead of reusing the deep, truncated entry.
TEST_F(M3UParserTest, DepthAwareCacheReexpandsShallowReference) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  const int cap = M3UParser::kMaxNestingDepth;

  // shared.m3u -> deep.m3u, and deep.m3u -> leaf. Reached shallow, shared expands two levels down to the leaf.
  const QString deep = tmp.filePath(u"deep.m3u"_s);
  ASSERT_TRUE(WriteFile(deep, leaf.fileName() + u'\n'));
  const QString shared = tmp.filePath(u"shared.m3u"_s);
  ASSERT_TRUE(WriteFile(shared, u"deep.m3u\n"_s));

  // A pass-through chain chain0 -> chain1 -> ... that reaches shared.m3u at depth cap-1.
  // shared is opened there (allowed), but its own reference to deep.m3u lands at the cap and is skipped, so the deep expansion of shared is truncated to zero tracks under this tight budget.
  for (int i = cap - 2; i >= 0; --i) {
    const QString cur = tmp.filePath(QStringLiteral("chain%1.m3u").arg(i));
    const QString next = (i == cap - 2) ? u"shared.m3u\n"_s : QStringLiteral("chain%1.m3u\n").arg(i + 1);
    ASSERT_TRUE(WriteFile(cur, next));
  }

  // root.m3u references the deep chain first (caching shared's truncated expansion) and then shared.m3u directly with full budget.
  const QString root = tmp.filePath(u"root.m3u"_s);
  ASSERT_TRUE(WriteFile(root, u"chain0.m3u\nshared.m3u\n"_s));

  M3UParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, root);
  // The deep path contributes nothing (truncated), but the shallow reference to shared must still resolve the leaf through deep.m3u.
  ASSERT_EQ(result.songs.size(), 1);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf.fileName()));
}

}  // namespace
