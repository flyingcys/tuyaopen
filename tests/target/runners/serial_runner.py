from __future__ import annotations

import os
import queue
import re
import subprocess
import threading
import time
from pathlib import Path
from typing import Pattern

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover - optional dependency in dry-run environments
    serial = None


class RunnerTimeoutError(RuntimeError):
    pass


class _ProcessStreamReader(threading.Thread):
    def __init__(self, stream, output_queue: queue.Queue[str], history: list[str]):
        super().__init__(daemon=True)
        self._stream = stream
        self._output_queue = output_queue
        self._history = history

    def run(self) -> None:
        for line in self._stream:
            self._history.append(line.rstrip())
            self._output_queue.put(line)


class _SerialStreamReader(threading.Thread):
    def __init__(self, port, output_queue: queue.Queue[str], history: list[str], stop_event: threading.Event):
        super().__init__(daemon=True)
        self._port = port
        self._output_queue = output_queue
        self._history = history
        self._stop_event = stop_event

    def run(self) -> None:
        while not self._stop_event.is_set():
            line = self._port.readline()
            if not line:
                continue

            decoded = line.decode("utf-8", errors="replace")
            self._history.append(decoded.rstrip())
            self._output_queue.put(decoded)


class SerialRunner:
    def __init__(self, *, process=None, port=None, reader=None, stop_event=None):
        self._process = process
        self._port = port
        self._reader = reader
        self._stop_event = stop_event or threading.Event()
        self._output_queue: queue.Queue[str] = queue.Queue()
        self._history: list[str] = []

    @classmethod
    def from_process(cls, command: list[str], cwd: Path) -> "SerialRunner":
        process = subprocess.Popen(
            command,
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        runner = cls(process=process)
        runner._reader = _ProcessStreamReader(process.stdout, runner._output_queue, runner._history)
        runner._reader.start()
        return runner

    @classmethod
    def from_port(cls, port: str, baudrate: int = 115200, timeout: float = 0.2) -> "SerialRunner":
        if serial is None:
            raise RuntimeError("pyserial is required to use SerialRunner.from_port")

        serial_port = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        runner = cls(port=serial_port)
        runner._reader = _SerialStreamReader(serial_port, runner._output_queue, runner._history, runner._stop_event)
        runner._reader.start()
        return runner

    def expect(self, pattern: str | Pattern[str], timeout: float = 10) -> str:
        deadline = time.time() + timeout
        regex = re.compile(pattern) if isinstance(pattern, str) else pattern

        while time.time() < deadline:
            remaining = max(deadline - time.time(), 0.01)
            try:
                line = self._output_queue.get(timeout=remaining)
            except queue.Empty:
                if self._process is not None and self._process.poll() is not None:
                    break
                continue

            if regex.search(line):
                return line

        history = "\n".join(self._history[-20:])
        raise RunnerTimeoutError(f"Timed out waiting for pattern {regex.pattern!r}.\nRecent output:\n{history}")

    @property
    def history(self) -> list[str]:
        return list(self._history)

    def write(self, data: str) -> None:
        if self._process is not None and self._process.stdin is not None:
            self._process.stdin.write(data)
            self._process.stdin.flush()
            return

        if self._port is not None:
            payload = data.encode("utf-8")
            self._port.write(payload)
            self._port.flush()
            return

        raise RuntimeError("Runner is not writable")

    def close(self) -> None:
        self._stop_event.set()

        if self._port is not None:
            self._port.close()

        if self._process is not None:
            try:
                if self._process.stdin is not None:
                    self._process.stdin.close()
            except OSError:
                pass

            if self._process.poll() is None:
                self._process.terminate()
                try:
                    self._process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self._process.kill()
                    self._process.wait(timeout=2)
