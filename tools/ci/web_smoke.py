#!/usr/bin/env python3
"""web_smoke.py -- the Web build's headless-Chrome smoke test (`HOUSE-02901`).

    tools/ci/web_smoke.py --build-dir build-consumer            # SwiftShader, as CI has no GPU
    tools/ci/web_smoke.py --build-dir build-consumer --gpu      # the machine's GPU through ANGLE

Serves the built page on localhost, opens it in headless Chrome (no window anywhere) and drives it
the way a player would, with no one at the keyboard: the title's key press (the audio gesture),
Start from the main menu, 300 drawn frames of the walk, then Pause, Settings and one change of the
quality row -- the settings change a browser session makes (the Web build applies it and does not
persist it). Every step waits for the log line that proves it happened, and any error or fatal log,
uncaught exception or missed step fails the run with the step named.

Keys are held for 0.8 s: XNA reads keyboard STATE once a frame, and a software-rendered frame can
be longer than a press, which the game would then never see.

Needs Chrome (`google-chrome`) and the `websocket-client` module. Offline tooling: not runtime code,
not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

import websocket

KEYS = {"Enter": 13, "Escape": 27, "ArrowDown": 40, "ArrowRight": 39}
FRAMES = 300


def free_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Browser:
    def __init__(self, url: str, gpu: bool, profile: Path):
        self.port = free_port()
        flags = ["--enable-gpu", "--ignore-gpu-blocklist", "--use-angle=gl-egl"] if gpu else \
            ["--enable-unsafe-swiftshader"]
        environment = {k: v for k, v in os.environ.items() if k not in ("DISPLAY", "WAYLAND_DISPLAY")}
        self.process = subprocess.Popen(
            ["google-chrome", "--headless=new", f"--remote-debugging-port={self.port}",
             f"--remote-allow-origins=http://127.0.0.1:{self.port}", f"--user-data-dir={profile}",
             "--window-size=1280,720", "--no-first-run", *flags, "about:blank"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=environment)
        page = None
        for _ in range(100):
            try:
                page = next(t for t in json.load(urllib.request.urlopen(f"http://127.0.0.1:{self.port}/json"))
                            if t["type"] == "page")
                break
            except Exception:
                time.sleep(0.2)
        if page is None:
            raise RuntimeError("Chrome did not open its debugging endpoint")
        self.socket = websocket.create_connection(page["webSocketDebuggerUrl"], timeout=120,
                                                  suppress_origin=True)
        self.ids = iter(range(1, 10**9))
        self.console: list[str] = []
        self.call("Runtime.enable")
        self.call("Page.enable")
        self.call("Emulation.setFocusEmulationEnabled", enabled=True)
        self.call("Page.navigate", url=url)

    def call(self, method: str, **params):
        request = next(self.ids)
        self.socket.send(json.dumps({"id": request, "method": method, "params": params}))
        while True:
            message = json.loads(self.socket.recv())
            if message.get("method") == "Runtime.consoleAPICalled":
                self.console.append(" ".join(str(a.get("value", a.get("description", "")))
                                             for a in message["params"]["args"]))
            elif message.get("method") == "Runtime.exceptionThrown":
                self.console.append("EXCEPTION " + json.dumps(message["params"]["exceptionDetails"])[:400])
            if message.get("id") == request:
                return message.get("result", {})

    def idle(self, seconds: float) -> None:
        end = time.time() + seconds
        while time.time() < end:
            self.call("Runtime.evaluate", expression="0")
            time.sleep(0.2)

    def press(self, key: str) -> None:
        code = KEYS[key]
        self.call("Input.dispatchKeyEvent", type="keyDown", key=key, code=key, windowsVirtualKeyCode=code,
                  text="\r" if key == "Enter" else "")
        self.idle(0.8)
        self.call("Input.dispatchKeyEvent", type="keyUp", key=key, code=key, windowsVirtualKeyCode=code)

    def until(self, needle: str, seconds: float, step: str) -> None:
        end = time.time() + seconds
        while time.time() < end:
            if any(needle in line for line in self.console):
                return
            self.idle(1.0)
        raise RuntimeError(f"{step}: no '{needle}' within {seconds:.0f} s")

    def close(self) -> None:
        self.process.terminate()
        try:
            self.process.wait(10)
        except subprocess.TimeoutExpired:
            self.process.kill()


def run(build: Path, gpu: bool, log: Path | None) -> int:
    page = build / "cna-house.html"
    if not page.is_file():
        print(f"web_smoke: {page} does not exist; build the web preset first", file=sys.stderr)
        return 2
    http_port = free_port()
    server = subprocess.Popen([sys.executable, "-m", "http.server", str(http_port), "--bind", "127.0.0.1"],
                              cwd=build, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    profile = Path(tempfile.mkdtemp(prefix="cnahouse-web-smoke-", dir=build))
    browser = None
    step = "load"
    try:
        browser = Browser(f"http://127.0.0.1:{http_port}/cna-house.html", gpu, profile)
        browser.until("audio waiting for a user gesture", 300, "the page loads to the title")
        step = "title"
        browser.idle(2)
        browser.press("Enter")
        browser.until("audio ready", 60, "the title's key press opens audio")
        browser.idle(1)
        browser.press("Enter")
        browser.idle(2)
        step = "start"
        browser.press("Enter")
        browser.until("walking in", 300, "Start loads the house")
        step = "walk"
        browser.call("Runtime.evaluate", expression=
                     "window.__smokeFrames=0;(function f(){window.__smokeFrames++;requestAnimationFrame(f)})()")
        end = time.time() + 600
        while time.time() < end:
            frames = browser.call("Runtime.evaluate", returnByValue=True,
                                  expression="window.__smokeFrames")["result"]["value"]
            if frames >= FRAMES:
                break
            browser.idle(1)
        else:
            raise RuntimeError(f"the walk drew fewer than {FRAMES} frames in 600 s")
        step = "settings"
        browser.press("Escape")
        browser.idle(2)
        browser.press("ArrowDown")
        browser.press("Enter")
        browser.idle(2)
        browser.press("ArrowRight")
        browser.until("settings quality", 60, "the quality row changes the preset")
        browser.idle(3)
        problems = [line for line in browser.console
                    if "[E]" in line or "[F]" in line or line.startswith("EXCEPTION")]
        if problems:
            raise RuntimeError("the console reported: " + " | ".join(problems[:5]))
        print(f"web_smoke: passed -- title, Start, {FRAMES} walk frames and a settings change, no errors")
        return 0
    except Exception as error:
        print(f"web_smoke: FAILED at {step}: {error}", file=sys.stderr)
        return 1
    finally:
        if browser is not None:
            if log is not None:
                log.write_text("\n".join(browser.console) + "\n")
            browser.close()
        server.terminate()
        shutil.rmtree(profile, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--build-dir", type=Path, required=True, help="the web preset's build tree")
    parser.add_argument("--gpu", action="store_true", help="use the GPU instead of SwiftShader")
    parser.add_argument("--log", type=Path, help="write the page's console here")
    arguments = parser.parse_args()
    return run(arguments.build_dir.resolve(), arguments.gpu, arguments.log)


if __name__ == "__main__":
    sys.exit(main())
