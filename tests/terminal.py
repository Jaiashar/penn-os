"""Bounded PTY interaction for exercising the real interactive executable."""
import errno
import os
import pty
import select
import signal
import termios
import time


class Terminal:
    def __init__(self, argv, cwd, prompt):
        self.prompt = prompt.encode()
        self.buffer = b""
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.chdir(cwd)
                attrs = termios.tcgetattr(0)
                attrs[3] &= ~termios.ECHO
                termios.tcsetattr(0, termios.TCSANOW, attrs)
                os.execv(argv[0], argv)
            except BaseException:
                os._exit(127)
        self.reaped = False

    def expect_prompt(self, timeout=10):
        deadline = time.monotonic() + timeout
        while self.prompt not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f"Prompt timeout; output: {self.buffer!r}")
            if not select.select([self.fd], [], [], min(0.2, remaining))[0]:
                continue
            try:
                chunk = os.read(self.fd, 65536)
            except OSError as exc:
                if exc.errno != errno.EIO:
                    raise
                chunk = b""
            if not chunk:
                raise AssertionError(f"Exited before prompt; output: {self.buffer!r}")
            self.buffer += chunk
        end = self.buffer.index(self.prompt) + len(self.prompt)
        output, self.buffer = self.buffer[:end], self.buffer[end:]
        return output[:-len(self.prompt)].decode(errors="replace").replace("\r", "")

    def command(self, command):
        os.write(self.fd, (command + "\n").encode())
        output = self.expect_prompt()
        print(f"> {command}\n{output}", flush=True)
        return output

    def finish(self, data):
        os.write(self.fd, data)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.reaped = True
                assert os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0, status
                return
            time.sleep(0.05)
        raise AssertionError("Program did not exit cleanly within 10 seconds")

    def close(self):
        if not self.reaped:
            try:
                os.killpg(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(self.pid, 0)
        os.close(self.fd)
