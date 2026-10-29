"""Minimal GDB Remote Serial Protocol client for driving an emulator's GDB stub
(DuckStation: Settings > Advanced > Enable GDB server, default port 2345)."""
import socket
import struct
import time


class GdbError(Exception):
    pass


class Gdb:
    def __init__(self, host='127.0.0.1', port=2345, timeout=30.0, connect_wait=60.0):
        deadline = time.time() + connect_wait
        while True:
            try:
                self.sock = socket.create_connection((host, port), timeout=timeout)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.5)
        self.buf = b''

    def close(self):
        self.sock.close()

    # --- framing ---------------------------------------------------------
    def _recv_byte(self):
        while not self.buf:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise GdbError('connection closed')
            self.buf += chunk
        b, self.buf = self.buf[:1], self.buf[1:]
        return b

    def _send(self, payload):
        data = payload.encode('latin-1')
        pkt = b'$' + data + b'#' + b'%02x' % (sum(data) & 0xFF)
        for _ in range(5):
            self.sock.sendall(pkt)
            ack = self._recv_byte()
            while ack not in b'+-':
                ack = self._recv_byte()
            if ack == b'+':
                return
        raise GdbError('packet not acknowledged')

    def _recv(self):
        while self._recv_byte() != b'$':
            pass
        data = bytearray()
        while True:
            c = self._recv_byte()
            if c == b'#':
                break
            data += c
        self._recv_byte()
        self._recv_byte()
        self.sock.sendall(b'+')
        return data.decode('latin-1')

    def cmd(self, payload):
        self._send(payload)
        return self._recv()

    # --- commands --------------------------------------------------------
    def halt(self):
        self.sock.sendall(b'\x03')
        return self._recv()

    def read(self, addr, length):
        out = bytearray()
        while length:
            n = min(length, 0x800)
            r = self.cmd(f'm{addr:x},{n:x}')
            if r.startswith('E') and len(r) == 3:
                raise GdbError(f'read {addr:#x}: {r}')
            out += bytes.fromhex(r)
            addr += n
            length -= n
        return bytes(out)

    def read32(self, addr):
        return struct.unpack('<I', self.read(addr, 4))[0]

    def write(self, addr, data):
        r = self.cmd(f'M{addr:x},{len(data):x}:{data.hex()}')
        if r != 'OK':
            raise GdbError(f'write {addr:#x}: {r}')

    def regs(self):
        """General registers r0-r31, then sr, lo, hi, bad, cause, pc (MIPS gdb order)."""
        raw = bytes.fromhex(self.cmd('g').replace('x', '0'))  # 'x' = unavailable
        vals = struct.unpack(f'<{len(raw) // 4}I', raw)
        names = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3',
                 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
                 's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7',
                 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra',
                 'sr', 'lo', 'hi', 'bad', 'cause', 'pc']
        return dict(zip(names, vals))

    def break_at(self, addr):
        r = self.cmd(f'Z0,{addr:x},4')
        if r != 'OK':
            raise GdbError(f'breakpoint {addr:#x}: {r!r}')

    def unbreak(self, addr):
        self.cmd(f'z0,{addr:x},4')

    def cont(self, timeout=None):
        """Continue and wait for a stop reply. Returns the reply, or None on timeout
        (the target is halted again in that case)."""
        self._send('c')
        old = self.sock.gettimeout()
        self.sock.settimeout(timeout)
        try:
            return self._recv()
        except socket.timeout:
            self.sock.settimeout(old)
            return None if self.halt() else None
        finally:
            self.sock.settimeout(old)
