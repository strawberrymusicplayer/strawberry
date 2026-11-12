# SPDX-License-Identifier: GPL-3.0-or-later
"""Optional loopback-only SMB2 fixture; requires impacket in the test interpreter."""

import logging
import signal
import sys
import tempfile
import threading
import wave
from pathlib import Path

from impacket.ntlm import compute_lmhash, compute_nthash
from impacket import smb3structs
from impacket.smbserver import SimpleSMBServer


logging.disable(logging.CRITICAL)
signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
with tempfile.TemporaryDirectory(prefix="strawberry-nas-test-") as root:
    directory = Path(root)
    (directory / "album").mkdir()
    (directory / "album" / "bytes.bin").write_bytes(bytes(range(256)) * 8192)
    with wave.open(str(directory / "album" / "track.wav"), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(8000)
        audio.writeframes(b"\x00\x01" * 8000)
    server = SimpleSMBServer(listenAddress="127.0.0.1", listenPort=0)
    server.getServer().daemon_threads = True
    server.setSMB2Support(True)
    server.addShare("music", root, readOnly="yes")
    # Impacket can return normal command bodies for errors (including EOF).
    # MS-SMB2 2.2.2 requires an SMB2 ERROR body; leave signing and statuses unchanged.
    def error_response(callback):
        def invoke(*args):
            commands, packets, status = callback(*args)
            if status != 0:
                return [smb3structs.SMB2Error()], None, status
            return commands, packets, status
        return invoke

    for command in (smb3structs.SMB2_CREATE, smb3structs.SMB2_READ,
                    smb3structs.SMB2_QUERY_INFO, smb3structs.SMB2_CLOSE,
                    smb3structs.SMB2_QUERY_DIRECTORY):
        original = server.getServer().hookSmb2Command(command, None)
        server.getServer().hookSmb2Command(command, error_response(original))
    # Owned fixture credentials, never real NAS credentials.
    server.addCredential("strawberry-test", 0, compute_lmhash("fixture-only"), compute_nthash("fixture-only"))
    threading.Thread(target=server.start, daemon=True).start()
    print(server.getServer().server_address[1], flush=True)
    try:
        sys.stdin.buffer.read()
    finally:
        server.stop()
