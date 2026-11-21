# SPDX-License-Identifier: GPL-3.0-or-later
"""Optional loopback-only SMB2 fixture; requires impacket in the test interpreter.

An optional argument names a directory to share instead of a temporary one, so a
test can change the files directly and restart the server on the same files.
A second optional argument selects the port, so a restarted server is the same NAS.
"""

import logging
import json
import shutil
import signal
import sys
import tempfile
import threading
import time
import wave
from pathlib import Path

from impacket.ntlm import compute_lmhash, compute_nthash
from impacket import smb3structs
from impacket.smbserver import SimpleSMBServer


logging.disable(logging.CRITICAL)
signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
with tempfile.TemporaryDirectory(prefix="strawberry-nas-test-") as temporary:
    root = sys.argv[1] if len(sys.argv) > 1 else temporary
    directory = Path(root)
    created = not (directory / "album").exists()
    (directory / "album").mkdir(exist_ok=True)
    if created:
        (directory / "album" / "bytes.bin").write_bytes(bytes(range(256)) * 8192)
        with wave.open(str(directory / "album" / "track.wav"), "wb") as audio:
            audio.setnchannels(1)
            audio.setsampwidth(2)
            audio.setframerate(8000)
            audio.writeframes(b"\x00\x01" * 8000)
        for album in ("first", "second"):
            (directory / "album" / album).mkdir()
            shutil.copyfile(directory / "album" / "track.wav",
                            directory / "album" / album / "track.wav")
    server = SimpleSMBServer(listenAddress="127.0.0.1", listenPort=int(sys.argv[2]) if len(sys.argv) > 2 else 0)
    server.getServer().daemon_threads = True
    server.setSMB2Support(True)
    server.addShare("music", root, readOnly="yes")
    (directory / "bulk").mkdir(exist_ok=True)
    for index in range(128):
        (directory / "bulk" / f"entry-{index:03}.bin").write_bytes(b"fixture")
    commands_seen = {}
    # Impacket can return normal command bodies for errors (including EOF).
    # MS-SMB2 2.2.2 requires an SMB2 ERROR body; leave signing and statuses unchanged.
    def error_response(command, callback):
        def invoke(*args):
            commands_seen[command] = commands_seen.get(command, 0) + 1
            commands, packets, status = callback(*args)
            if status != 0:
                return [smb3structs.SMB2Error()], None, status
            return commands, packets, status
        return invoke

    for command in (smb3structs.SMB2_CREATE, smb3structs.SMB2_READ,
                    smb3structs.SMB2_QUERY_INFO, smb3structs.SMB2_CLOSE,
                    smb3structs.SMB2_QUERY_DIRECTORY):
        original = server.getServer().hookSmb2Command(command, None)
        server.getServer().hookSmb2Command(command, error_response(command, original))
    for command in (smb3structs.SMB2_NEGOTIATE, smb3structs.SMB2_SESSION_SETUP):
        original = server.getServer().hookSmb2Command(command, None)
        def count_connection(*args, command=command, callback=original):
            commands_seen[command] = commands_seen.get(command, 0) + 1
            return callback(*args)
        server.getServer().hookSmb2Command(command, count_connection)
    # CHANGE_NOTIFY is unsupported by impacket unless a test selects another reply.
    # "once" after a mode replies once with it, then holds the next requests.
    notify = {"mode": "unsupported", "once": False}
    # "queue" reports the "event" commands like a real server, which queues the changes
    # for a handle from its first request on, also while no request is pending.
    events = {"count": 0}
    # The path reported by change notifications, relative to the watched folder.
    notified = {"name": "new.wav"}
    handles = {}
    unsupported_notify = server.getServer().hookSmb2Command(smb3structs.SMB2_CHANGE_NOTIFY, None)
    def change_notify(connId, smbServer, recvPacket):
        mode = notify["mode"]
        if mode == "queue":
            handle = smb3structs.SMB2ChangeNotify(recvPacket["Data"])["FileID"].getData()
            seen = handles.setdefault(handle, events["count"])
            deadline = time.monotonic() + 30
            while events["count"] == seen and time.monotonic() < deadline:
                time.sleep(0.05)
            handles[handle] = events["count"]
            mode = "change"
        if mode == "hold":
            # A held request is answered with the next selected mode, or unsupported after 10 s.
            deadline = time.monotonic() + 10
            while notify["mode"] == "hold" and time.monotonic() < deadline:
                time.sleep(0.05)
            mode = notify["mode"]
        if mode in ("change", "overflow"):
            if notify["once"]:
                notify["mode"] = "hold"
                notify["once"] = False
            time.sleep(0.2)
            response = smb3structs.SMB2ChangeNotify_Response()
            response["OutputBufferOffset"] = 0x48
            if mode == "overflow":
                response["Buffer"] = b""
                return [response], None, 0x0000010C
            info = smb3structs.FILE_NOTIFY_INFORMATION()
            info["Action"] = 1
            info["FileName"] = notified["name"].replace("/", "\\").encode("utf-16le")
            info["FileNameLength"] = len(info["FileName"])
            response["Buffer"] = info.getData()
            response["OutputBufferLength"] = len(response["Buffer"])
            return [response], None, 0
        return unsupported_notify(connId, smbServer, recvPacket)
    server.getServer().hookSmb2Command(smb3structs.SMB2_CHANGE_NOTIFY, change_notify)
    # Owned fixture credentials, never real NAS credentials.
    server.addCredential("strawberry-test", 0, compute_lmhash("fixture-only"), compute_nthash("fixture-only"))
    threading.Thread(target=server.start, daemon=True).start()
    print(server.getServer().server_address[1], flush=True)
    try:
        for line in sys.stdin:
            if line.strip() == "stats":
                print(json.dumps({
                    "negotiate": commands_seen.get(smb3structs.SMB2_NEGOTIATE, 0),
                    "query_info": commands_seen.get(smb3structs.SMB2_QUERY_INFO, 0),
                    "create": commands_seen.get(smb3structs.SMB2_CREATE, 0),
                }), flush=True)
            elif line.startswith("notify "):
                words = line.split()
                notify["once"] = len(words) > 2 and words[2] == "once"
                notify["mode"] = words[1]
                print("notify", flush=True)
            elif line.startswith("name "):
                notified["name"] = line[len("name "):].strip()
                print("name", flush=True)
            elif line.strip() == "event":
                events["count"] += 1
                print("event", flush=True)
            elif line.strip() == "resize":
                (directory / "bulk" / "entry-000.bin").write_bytes(b"changed fixture contents")
                print("resized", flush=True)
    finally:
        server.stop()
