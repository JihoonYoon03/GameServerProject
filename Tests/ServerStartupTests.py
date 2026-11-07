import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

# 실제 운영 main을 사용한 독립 검증용 실행 파일을 인자로 받는다.
exe = str(Path(sys.argv[1]).resolve())
checks = 0

def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)

def free_port():
    with socket.socket() as peer:
        peer.bind(("127.0.0.1", 0))
        return peer.getsockname()[1]

for args in [[], ["abc", "9001"], ["0", "9001"], ["65536", "9001"],
             ["-1", "9001"], ["9000", "999999999999999999999"],
             ["9000", "9001", "extra"], ["9000", "0"], ["9000", "abc"]]:
    result = subprocess.run([exe] + args, input=b"\n", capture_output=True, timeout=5)
    check(result.returncode == 1 and b"Usage:" in result.stdout, "invalid arguments: " + repr(args))

for repeat in range(3):
    tcp, udp = free_port(), free_port()
    process = subprocess.Popen([exe, str(tcp), str(udp)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 5
        while True:
            try:
                peer = socket.create_connection(("127.0.0.1", tcp), timeout=0.2)
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.02)
        with peer:
            peer.settimeout(3)
            data = b""
            while len(data) < 42:
                part = peer.recv(42 - len(data))
                check(bool(part), "handshake bytes")
                data += part
            check(struct.unpack("!IHHI", data[:12]) == (42, 5, 1, 0), "wire header")
            check(struct.unpack("!H", data[30:32])[0] == udp, "UDP port")
            output, error = process.communicate(b"\n", timeout=5)
            check(process.returncode == 0 and b"Server started:" in output, "Enter exit")
            check(peer.recv(1) == b"", "peer closed")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()

# 다른 프로세스가 포트를 소유할 때 main이 오류 종료하는지도 확인한다.
with socket.socket() as occupied:
    occupied.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
    occupied.bind(("0.0.0.0", 0))
    occupied.listen()
    result = subprocess.run([exe, str(occupied.getsockname()[1]), str(free_port())],
                            input=b"\n", capture_output=True, timeout=5)
    check(result.returncode == 1 and b"Server start failed." in result.stdout, "occupied port exit")
print("PASS: actual main startup audit,", checks, "checks")
