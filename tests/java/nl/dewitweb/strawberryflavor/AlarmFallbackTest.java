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
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

// Tests AlarmFallback.java, which chooses what an alarm plays without its own songs, on a desktop JVM without the Android SDK.
public final class AlarmFallbackTest {

  private static int failures = 0;

  private AlarmFallbackTest() {}

  public static void main(final String[] args) throws IOException {

    expect("sound does not use tracks", !AlarmFallback.usesTracks("sound"));
    expect("radio does not use tracks", !AlarmFallback.usesTracks("radio"));
    expect("random uses tracks", AlarmFallback.usesTracks("random"));
    expect("playlist uses tracks", AlarmFallback.usesTracks("playlist"));

    expect("default sound source has no reason", AlarmFallback.reason("sound", false) == AlarmFallback.REASON_NONE);
    expect("random without songs has no song", AlarmFallback.reason("random", false) == AlarmFallback.REASON_NO_SONG);
    expect("playlist with songs that fail failed", AlarmFallback.reason("playlist", true) == AlarmFallback.REASON_FAILED);
    expect("radio without stream failed", AlarmFallback.reason("radio", false) == AlarmFallback.REASON_FAILED);

    // In the working directory of the test rather than the system temporary directory.
    final File root = Files.createTempDirectory(Paths.get("").toAbsolutePath(), "alarmfallback").toFile();
    try {
      expect("missing directory has no copies", AlarmFallback.copies(new File(root, "missing"), Collections.emptyList()).isEmpty());
      final File first = new File(root, "a");
      final File second = new File(root, "b");
      expect("alarm directories created", first.mkdir() && second.mkdir());
      final File song1 = write(new File(first, "1_0.mp3"), 10);
      final File song2 = write(new File(second, "2_0.ogg"), 10);
      write(new File(second, "2_1.mp3"), 0);
      write(new File(root, "stray.mp3"), 10);
      final List<String> copies = AlarmFallback.copies(root, Collections.emptyList());
      expect("copies of all alarms, without empty or stray files", copies.equals(Arrays.asList(song1.getPath(), song2.getPath())));
      expect("chosen songs are left out", AlarmFallback.copies(root, Collections.singletonList(song1.getPath())).equals(Collections.singletonList(song2.getPath())));
    }
    finally {
      delete(root);
    }

    if (failures > 0) {
      System.out.println(failures + " test(s) failed");
      System.exit(1);
    }
    System.out.println("All tests passed");

  }

  private static File write(final File file, final int size) throws IOException {

    Files.write(file.toPath(), new byte[size]);
    return file;

  }

  private static void delete(final File file) {

    final File[] children = file.listFiles();
    if (children != null) {
      for (final File child : children) delete(child);
    }
    if (!file.delete()) System.out.println("Could not delete " + file);

  }

  private static void expect(final String name, final boolean condition) {

    if (condition) return;
    System.out.println("FAILED: " + name);
    ++failures;

  }

}
