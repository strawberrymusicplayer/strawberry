/*
 * Strawberry Music Player
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

package nl.dewitweb.strawberryflavor;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

// Decides what an alarm plays when its own songs are missing, without Android classes, so it is tested on a desktop JVM.
final class AlarmFallback {

  // Why the default alarm sound plays instead of the alarm's music.
  // Constants rather than an enum: the D8 of the Android Gradle plugin that Qt uses fails on nested classes compiled by recent JDKs.
  static final int REASON_NONE = 0;
  static final int REASON_NO_SONG = 1;
  static final int REASON_FAILED = 2;

  private AlarmFallback() {}

  // Whether the source plays songs from the alarm's tracks: copies, or with copying off the original songs.
  static boolean usesTracks(final String source) {
    return !"sound".equals(source) && !"radio".equals(source);
  }

  // Returns the songs copied for any alarm in the alarms directory, one subdirectory per alarm, without the ones already chosen.
  // A random alarm that goes off before its own songs are copied still plays a random song from these.
  static List<String> copies(final File root, final List<String> chosen) {

    final Set<String> tracks = new LinkedHashSet<>();
    final File[] directories = root.listFiles(File::isDirectory);
    if (directories == null) return new ArrayList<>();
    Arrays.sort(directories);
    for (final File directory : directories) {
      final File[] files = directory.listFiles(File::isFile);
      if (files == null) continue;
      Arrays.sort(files);
      for (final File file : files) {
        if (file.length() > 0) tracks.add(file.getPath());
      }
    }
    tracks.removeAll(chosen);
    return new ArrayList<>(tracks);

  }

  // Returns why the default sound plays: nothing to play for a music source, or the music failed.
  static int reason(final String source, final boolean hadItems) {

    if ("sound".equals(source)) return REASON_NONE;
    return hadItems || "radio".equals(source) ? REASON_FAILED : REASON_NO_SONG;

  }

}
