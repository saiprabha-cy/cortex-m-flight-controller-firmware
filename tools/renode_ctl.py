#!/usr/bin/env python3
"""renode_ctl.py - start/stop a headless Renode instance and drive it through its telnet monitor.
Used by the HIL pytest suite and the demo scripts. Requires `renode` on PATH (or RENODE=/path/to/renode)."""
import os, pathlib, shutil, socket, subprocess, threading, time

ROOT = pathlib.Path(__file__).resolve().parent.parent


class Renode:
    def __init__(self, monitor_port=12345, extra_args=()):
        exe = os.environ.get("RENODE") or shutil.which("renode")
        if not exe:
            raise RuntimeError("renode not found: install it or set RENODE=/path/to/renode")
        self.port = monitor_port
        self.proc = subprocess.Popen([exe, "--disable-xwt", "--hide-log", "--plain", "--port", str(monitor_port), *extra_args],
                                     cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        self.sock = None; self.lock = threading.Lock(); self._runner = None
        t0 = time.time()
        while self.sock is None:
            try:
                self.sock = socket.create_connection(("localhost", monitor_port), timeout=2)
            except OSError:
                if time.time() - t0 > 60: raise RuntimeError("could not connect to Renode monitor")
                time.sleep(0.3)
        self.sock.settimeout(120); self._drain(1.0)

    def _drain(self, t=0.3):
        self.sock.settimeout(t); data = b""
        try:
            while True:
                chunk = self.sock.recv(65536)
                if not chunk: break
                data += chunk
        except (socket.timeout, TimeoutError): pass
        return data.decode(errors="replace")

    def cmd(self, text, wait=0.4):
        with self.lock:
            self.sock.sendall((text + "\n").encode()); return self._drain(wait)

    def include(self, resc, **variables):
        for k, v in variables.items():
            self.cmd(f"${k}={v}")
        return self.cmd(f"include @{resc}", wait=3.0)

    def run_async(self, seconds):
        """Advance virtual time by `seconds` in the background (returns immediately)."""
        def go():
            with self.lock:
                self.sock.sendall(f'emulation RunFor "00:00:{seconds:06.3f}"\n'.encode()); self.sock.settimeout(3600)
                try: self.sock.recv(65536)
                except Exception: pass
        self._runner = threading.Thread(target=go, daemon=True); self._runner.start()

    def start(self): self.cmd("start")
    def pause(self): self.cmd("pause")

    def close(self):
        try: self.cmd("quit", wait=0.2)
        except Exception: pass
        try: self.proc.terminate(); self.proc.wait(5)
        except Exception: self.proc.kill()
