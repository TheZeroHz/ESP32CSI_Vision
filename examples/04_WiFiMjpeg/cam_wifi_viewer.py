#!/usr/bin/env python3
"""Live MJPEG viewer for ESP32CSI_Vision.

Usage:
  python cam_wifi_viewer.py 192.168.0.3
  python cam_wifi_viewer.py 192.168.0.3 81
  python cam_wifi_viewer.py 192.168.0.3 --port 81 --ui-port 80

Stream defaults to port 81 (MJPEG). Status/snapshot use UI port 80.
"""

from __future__ import annotations

import argparse
import io
import json
import socket
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox
from urllib.request import urlopen

try:
    from PIL import Image, ImageTk
except ImportError:
    sys.exit("pip install pillow")


class MjpegClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port  # MJPEG stream port (default 81)
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.latest_jpeg: bytes | None = None
        self.latest_size = (0, 0)
        self.fps = 0.0
        self.kbps = 0.0
        self.error = ""
        self.connected = False
        self.frames = 0
        self._thread: threading.Thread | None = None

    @property
    def base(self) -> str:
        return f"http://{self.host}:{self.port}"

    def start(self) -> None:
        self.stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def close(self) -> None:
        self.stop.set()

    def snapshot_bytes(self) -> bytes | None:
        with self.lock:
            return self.latest_jpeg

    def _run(self) -> None:
        while not self.stop.is_set():
            try:
                self._stream_once()
            except Exception as exc:
                self.connected = False
                self.error = str(exc)
                time.sleep(0.8)

    def _stream_once(self) -> None:
        sock = socket.create_connection((self.host, self.port), timeout=6)
        try:
            req = (
                f"GET /stream HTTP/1.1\r\nHost: {self.host}\r\n"
                "Connection: keep-alive\r\n\r\n"
            ).encode()
            sock.sendall(req)
            sock.settimeout(4.0)
            buf = b""
            header_done = False
            t0 = time.time()
            nbytes = 0
            nframes = 0
            self.connected = True
            self.error = ""
            while not self.stop.is_set():
                chunk = sock.recv(65536)
                if not chunk:
                    raise ConnectionError("stream closed")
                buf += chunk
                if not header_done:
                    sep = buf.find(b"\r\n\r\n")
                    if sep < 0:
                        continue
                    buf = buf[sep + 4 :]
                    header_done = True
                while True:
                    soi = buf.find(b"\xff\xd8")
                    if soi < 0:
                        if len(buf) > 2_000_000:
                            buf = buf[-64:]
                        break
                    eoi = buf.find(b"\xff\xd9", soi + 2)
                    if eoi < 0:
                        if soi > 0:
                            buf = buf[soi:]
                        break
                    jpeg = buf[soi : eoi + 2]
                    buf = buf[eoi + 2 :]
                    self._accept_jpeg(jpeg)
                    nframes += 1
                    nbytes += len(jpeg)
                    now = time.time()
                    dt = now - t0
                    if dt >= 1.0:
                        self.fps = nframes / dt
                        self.kbps = (nbytes * 8 / 1000.0) / dt
                        t0 = now
                        nframes = 0
                        nbytes = 0
        finally:
            self.connected = False
            try:
                sock.close()
            except OSError:
                pass

    def _accept_jpeg(self, jpeg: bytes) -> None:
        try:
            img = Image.open(io.BytesIO(jpeg))
            img.load()
            size = img.size
        except Exception:
            return
        with self.lock:
            self.latest_jpeg = jpeg
            self.latest_size = size
            self.frames += 1


class ViewerApp:
    def __init__(self, host: str, port: int, ui_port: int = 80):
        self.root = tk.Tk()
        self.root.title("ESP32-P4 CSI Viewer")
        self.root.geometry("980x780")
        self.root.minsize(720, 560)
        self.root.configure(bg="#0f1419")

        self.host_var = tk.StringVar(value=host)
        self.port_var = tk.StringVar(value=str(port))  # MJPEG stream port
        self.ui_port_var = tk.StringVar(value=str(ui_port))
        self.status_var = tk.StringVar(value="Disconnected")
        self.meta_var = tk.StringVar(value="—")

        self.client: MjpegClient | None = None
        self.photo = None
        self._build()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(40, self._tick)

    def _build(self) -> None:
        top = tk.Frame(self.root, bg="#151c24", padx=14, pady=12)
        top.pack(fill="x")

        tk.Label(top, text="ESP32-P4 CSI", fg="#3dd6c6", bg="#151c24",
                 font=("Segoe UI", 14, "bold")).pack(side="left", padx=(0, 16))

        tk.Label(top, text="Host", fg="#9bb0c0", bg="#151c24",
                 font=("Segoe UI", 9)).pack(side="left")
        tk.Entry(top, textvariable=self.host_var, width=16, bg="#0f1419", fg="#e8eef4",
                 insertbackground="#e8eef4", relief="flat").pack(side="left", padx=6)

        tk.Label(top, text="Stream", fg="#9bb0c0", bg="#151c24",
                 font=("Segoe UI", 9)).pack(side="left")
        tk.Entry(top, textvariable=self.port_var, width=5, bg="#0f1419", fg="#e8eef4",
                 insertbackground="#e8eef4", relief="flat").pack(side="left", padx=6)

        tk.Label(top, text="UI", fg="#9bb0c0", bg="#151c24",
                 font=("Segoe UI", 9)).pack(side="left")
        tk.Entry(top, textvariable=self.ui_port_var, width=5, bg="#0f1419", fg="#e8eef4",
                 insertbackground="#e8eef4", relief="flat").pack(side="left", padx=6)

        self.btn = tk.Button(top, text="Connect", command=self.toggle, bg="#3dd6c6",
                             fg="#0f1419", relief="flat", padx=14, pady=4,
                             font=("Segoe UI", 9, "bold"), cursor="hand2")
        self.btn.pack(side="left", padx=10)

        tk.Button(top, text="Snapshot", command=self.snapshot, bg="#243040", fg="#e8eef4",
                  relief="flat", padx=12, pady=4, font=("Segoe UI", 9),
                  cursor="hand2").pack(side="left")

        self.stage = tk.Label(self.root, bg="#000000", fg="#6a7a88",
                              text="Connect to start live view",
                              font=("Segoe UI", 12))
        self.stage.pack(fill="both", expand=True, padx=14, pady=(12, 8))

        bot = tk.Frame(self.root, bg="#151c24", padx=14, pady=10)
        bot.pack(fill="x")
        tk.Label(bot, textvariable=self.status_var, fg="#3dd6c6", bg="#151c24",
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        tk.Label(bot, textvariable=self.meta_var, fg="#9bb0c0", bg="#151c24",
                 font=("Segoe UI", 9)).pack(side="right")

    def toggle(self) -> None:
        if self.client:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        host = self.host_var.get().strip()
        try:
            port = int(self.port_var.get().strip())
            ui_port = int(self.ui_port_var.get().strip())
        except ValueError:
            messagebox.showerror("Port", "Ports must be numbers")
            return
        self.client = MjpegClient(host, port)
        self.client.start()
        self.btn.configure(text="Disconnect", bg="#e25c5c", fg="#fff")
        self.status_var.set(f"Connecting {host}:{port} …")
        threading.Thread(target=self._fetch_status, args=(host, ui_port), daemon=True).start()

    def disconnect(self) -> None:
        if self.client:
            self.client.close()
            self.client = None
        self.btn.configure(text="Connect", bg="#3dd6c6", fg="#0f1419")
        self.status_var.set("Disconnected")
        self.meta_var.set("—")

    def _fetch_status(self, host: str, port: int) -> None:
        try:
            with urlopen(f"http://{host}:{port}/status", timeout=3) as r:
                data = json.loads(r.read().decode("utf-8", "ignore"))
            self.root.after(0, lambda: self.meta_var.set(
                f"{data.get('sensor','?')}  {data.get('out_w', data.get('w'))}x"
                f"{data.get('out_h', data.get('h'))}  q{data.get('quality','?')}"
            ))
        except Exception:
            pass

    def snapshot(self) -> None:
        data = self.client.snapshot_bytes() if self.client else None
        if not data:
            try:
                ui = int(self.ui_port_var.get().strip())
                with urlopen(f"http://{self.host_var.get().strip()}:{ui}/jpg", timeout=5) as r:
                    data = r.read()
            except Exception as exc:
                messagebox.showerror("Snapshot", str(exc))
                return
        path = filedialog.asksaveasfilename(
            defaultextension=".jpg",
            filetypes=[("JPEG", "*.jpg")],
            initialfile=f"esp32p4_{int(time.time())}.jpg",
        )
        if not path:
            return
        with open(path, "wb") as f:
            f.write(data)

    def _tick(self) -> None:
        c = self.client
        if c:
            jpeg = None
            size = (0, 0)
            with c.lock:
                jpeg = c.latest_jpeg
                size = c.latest_size
            if jpeg:
                try:
                    img = Image.open(io.BytesIO(jpeg)).convert("RGB")
                    cw = max(self.stage.winfo_width(), 320)
                    ch = max(self.stage.winfo_height(), 240)
                    img.thumbnail((cw - 8, ch - 8), Image.Resampling.BILINEAR)
                    self.photo = ImageTk.PhotoImage(img)
                    self.stage.configure(image=self.photo, text="")
                except Exception:
                    pass
            state = "Live" if c.connected else ("Reconnecting…" if c.error else "Connecting…")
            err = f"  ({c.error})" if c.error and not c.connected else ""
            self.status_var.set(
                f"{state}{err}   {size[0]}x{size[1]}   {c.fps:.1f} fps   {c.kbps:.0f} kb/s   "
                f"frames {c.frames}"
            )
        self.root.after(40, self._tick)

    def on_close(self) -> None:
        self.disconnect()
        self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()


def main() -> int:
    p = argparse.ArgumentParser(description="ESP32-P4 CSI MJPEG viewer")
    p.add_argument("host", nargs="?", default="192.168.0.3", help="board IP")
    p.add_argument("stream_port", nargs="?", type=int, default=None, help="MJPEG port (default 81)")
    p.add_argument("--port", type=int, default=81, help="MJPEG stream port (default 81)")
    p.add_argument("--ui-port", type=int, default=80, help="UI/control port for /status /jpg")
    args = p.parse_args()
    stream_port = args.stream_port if args.stream_port is not None else args.port
    ViewerApp(args.host, stream_port, args.ui_port).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
