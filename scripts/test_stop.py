import subprocess
import sys
import threading
import time

ENGINE = r"C:\Users\hmaso\OneDrive\Desktop\Fe64\bin\fe64.exe"

p = subprocess.Popen([ENGINE], cwd=r"C:\Users\hmaso\OneDrive\Desktop\Fe64\bin",
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, text=True, bufsize=1)

lines = []


def reader():
    for line in p.stdout:
        lines.append(line.rstrip())
        print("ENGINE>", line.rstrip(), flush=True)


t = threading.Thread(target=reader, daemon=True)
t.start()


def send(cmd):
    print("BOT   >", cmd, flush=True)
    p.stdin.write(cmd + "\n")
    p.stdin.flush()


send("uci")
time.sleep(0.5)
send("setoption name OwnBook value false")
send("isready")
time.sleep(0.3)
send("ucinewgame")
send("position startpos")
send("go infinite")
time.sleep(3.0)
send("stop")

deadline = time.time() + 10
while time.time() < deadline and not any(l.startswith("bestmove") for l in lines):
    time.sleep(0.2)

time.sleep(0.3)
send("quit")
try:
    p.wait(timeout=5)
except subprocess.TimeoutExpired:
    p.kill()
    print("!!! engine did not exit after quit")

depth_lines = [l for l in lines if l.startswith("info depth")]
has_bestmove = any(l.startswith("bestmove") for l in lines)
print("\n=== RESULT ===")
print("depth info lines:", len(depth_lines))
print("max depth reached:", max((int(l.split()[2]) for l in depth_lines), default=0))
print("bestmove emitted:", has_bestmove)
sys.exit(0 if has_bestmove else 1)
