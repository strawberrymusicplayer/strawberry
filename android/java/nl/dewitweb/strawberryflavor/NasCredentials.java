// SPDX-License-Identifier: GPL-3.0-or-later
package nl.dewitweb.strawberryflavor;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.security.GeneralSecurityException;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class NasCredentials {
    private static final String ALIAS = "strawberry-nas-v1";

    private NasCredentials() {}

    private static SecretKey key(boolean create) throws GeneralSecurityException, IOException {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey) store.getKey(ALIAS, null);
        if (!create) throw new IllegalStateException("NAS key unavailable");
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true).build());
        return generator.generateKey();
    }

    private static AtomicFile file(Context context) {
        return new AtomicFile(new File(context.getNoBackupFilesDir(), "nas-locations-v1"));
    }

    public static synchronized String load(Context context) {
        AtomicFile file = file(context);
        // AtomicFile.openRead also restores a backup left by an interrupted write.
        if (!file.getBaseFile().exists()
                && !new File(file.getBaseFile().getPath() + ".bak").exists()) return "[]";
        try {
            byte[] data = file.readFully();
            if (data.length < 28) throw new IllegalStateException("Invalid NAS vault");
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, data, 0, 12));
            return new String(cipher.doFinal(data, 12, data.length - 12), StandardCharsets.UTF_8);
        } catch (GeneralSecurityException | IOException error) {
            // Do not expose credentials or cryptographic implementation details through JNI logs.
            throw new IllegalStateException("Could not unlock NAS credentials");
        }
    }

    public static synchronized boolean save(Context context, String json) {
        AtomicFile file = file(context);
        FileOutputStream output = null;
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true));
            byte[] encrypted = cipher.doFinal(json.getBytes(StandardCharsets.UTF_8));
            byte[] data = ByteBuffer.allocate(cipher.getIV().length + encrypted.length)
                    .put(cipher.getIV()).put(encrypted).array();
            output = file.startWrite();
            output.write(data);
            file.finishWrite(output);
            return true;
        } catch (GeneralSecurityException | IOException error) {
            if (output != null) file.failWrite(output);
            throw new IllegalStateException("Could not save NAS credentials");
        }
    }
}
