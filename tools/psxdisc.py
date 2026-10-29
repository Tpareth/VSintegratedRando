"""Raw PS1 disc image (MODE2/2352) access: ISO9660 browsing, file extraction and
in-place file replacement with EDC/ECC regeneration."""
import struct

SECTOR = 2352
USER = 2048
DATA_OFF = 24  # sync(12) + header(4) + subheader(8)

# --- EDC/ECC -----------------------------------------------------------------
_ecc_f = bytearray(256)
_ecc_b = bytearray(256)
_edc = [0] * 256
for _i in range(256):
    _j = ((_i << 1) ^ (0x11D if _i & 0x80 else 0)) & 0xFF
    _ecc_f[_i] = _j
    _ecc_b[_i ^ _j] = _i
    _e = _i
    for _ in range(8):
        _e = (_e >> 1) ^ (0xD8018001 if _e & 1 else 0)
    _edc[_i] = _e


def edc(data):
    e = 0
    lut = _edc
    for b in data:
        e = (e >> 8) ^ lut[(e ^ b) & 0xFF]
    return e


def _ecc_block(src, major_count, minor_count, major_mult, minor_inc, dest, dest_off):
    size = major_count * minor_count
    f = _ecc_f
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = src[index]
            index += minor_inc
            if index >= size:
                index -= size
            a = f[a ^ t]
            b ^= t
        a = _ecc_b[f[a] ^ b]
        dest[dest_off + major] = a
        dest[dest_off + major + major_count] = a ^ b


def fix_sector(sec):
    """Regenerate EDC (and ECC for Form 1) of a raw Mode 2 sector in place."""
    submode = sec[18]
    if submode & 0x20:  # Form 2: EDC over subheader+2324 data, stored at end
        struct.pack_into('<I', sec, 0x92C, edc(sec[16:0x92C]))
        return
    struct.pack_into('<I', sec, 0x818, edc(sec[16:0x818]))
    hdr = bytes(sec[12:16])
    sec[12:16] = b'\0\0\0\0'
    body = bytes(sec[12:0x8C8])
    _ecc_block(body, 86, 24, 2, 86, sec, 0x81C)
    body = bytes(sec[12:0x930])
    _ecc_block(body, 52, 43, 86, 88, sec, 0x8C8)
    sec[12:16] = hdr


# --- ISO9660 -----------------------------------------------------------------
class Entry:
    def __init__(self, path, lba, size, is_dir, rec_lba=None, rec_off=None):
        self.path, self.lba, self.size, self.is_dir = path, lba, size, is_dir
        self.rec_lba, self.rec_off = rec_lba, rec_off  # where the directory record lives

    @property
    def sectors(self):
        return (self.size + USER - 1) // USER

    def __repr__(self):
        return f'{self.path} lba={self.lba} size={self.size}'


class Disc:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.img = bytearray(f.read())
        self.entries = {}
        pvd = self.read_sector(16)
        assert pvd[1:6] == b'CD001', 'not ISO9660'
        root = pvd[156:156 + 34]
        lba, size = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
        self._walk('', lba, size)

    def sector_count(self):
        return len(self.img) // SECTOR

    def raw(self, lba):
        return memoryview(self.img)[lba * SECTOR:(lba + 1) * SECTOR]

    def read_sector(self, lba):
        o = lba * SECTOR + DATA_OFF
        return bytes(self.img[o:o + USER])

    def read_lba(self, lba, size):
        out = bytearray()
        for i in range((size + USER - 1) // USER):
            out += self.read_sector(lba + i)
        return bytes(out[:size])

    def _walk(self, prefix, lba, size):
        data = self.read_lba(lba, size)
        p = 0
        while p < len(data):
            ln = data[p]
            if ln == 0:  # records never cross sector boundaries
                p = (p // USER + 1) * USER
                continue
            elba = struct.unpack_from('<I', data, p + 2)[0]
            esize = struct.unpack_from('<I', data, p + 10)[0]
            flags = data[p + 25]
            nlen = data[p + 32]
            name = data[p + 33:p + 33 + nlen]
            p += ln
            if name in (b'\0', b'\1'):
                continue
            name = name.decode('ascii').split(';')[0]
            path = f'{prefix}/{name}' if prefix else name
            is_dir = bool(flags & 2)
            rec = p - ln
            self.entries[path] = Entry(path, elba, esize, is_dir, lba + rec // USER, rec % USER)
            if is_dir:
                self._walk(path, elba, esize)

    def files(self):
        return [e for e in self.entries.values() if not e.is_dir]

    def find(self, path):
        return self.entries[path.strip('/').upper()]

    def read_file(self, path):
        e = self.find(path)
        return self.read_lba(e.lba, e.size)

    def write_lba(self, lba, data):
        """Write user data over Form 1 sectors starting at lba, fixing EDC/ECC."""
        for i in range(0, len(data), USER):
            chunk = data[i:i + USER]
            sec = self.raw(lba + i // USER)
            assert not (sec[18] & 0x20), f'sector {lba + i // USER} is Form 2'
            sec[DATA_OFF:DATA_OFF + len(chunk)] = chunk
            fix_sector(sec)

    def write_file(self, path, data):
        """Replace a file's contents in place. The new data must fit in the
        file's existing sector allocation; the directory size is not changed."""
        e = self.find(path)
        assert len(data) <= e.sectors * USER, f'{path}: {len(data)} > {e.sectors * USER}'
        pad = (-len(data)) % USER
        self.write_lba(e.lba, bytes(data) + bytes(pad) if pad else bytes(data))

    def write_form1(self, lba, data):
        """Write data as fresh Form 1 sectors (e.g. into empty Form 2 padding),
        rewriting the subheaders as plain data with EOR/EOF on the last sector."""
        count = (len(data) + USER - 1) // USER
        data = bytes(data) + bytes(count * USER - len(data))
        for i in range(count):
            sec = self.raw(lba + i)
            assert sec[15] == 2, f'sector {lba + i} is not Mode 2'
            submode = 0x89 if i == count - 1 else 0x08
            sec[16:24] = bytes([0, 0, submode, 0]) * 2
            sec[DATA_OFF:DATA_OFF + USER] = data[i * USER:(i + 1) * USER]
            fix_sector(sec)

    def is_free(self, lba, count):
        """True if no file or directory uses [lba, lba+count) and the sectors are empty."""
        for e in self.entries.values():
            if e.lba < lba + count and lba < e.lba + max(1, e.sectors):
                return False
        return not any(any(self.read_sector(l)) for l in range(lba, lba + count))

    def relocate(self, path, new_lba, data):
        """Write a file's new contents at new_lba and point its directory record there."""
        e = self.find(path)
        self.write_form1(new_lba, data)
        rec = bytearray(self.read_sector(e.rec_lba))
        struct.pack_into('<I', rec, e.rec_off + 2, new_lba)
        struct.pack_into('>I', rec, e.rec_off + 6, new_lba)
        struct.pack_into('<I', rec, e.rec_off + 10, len(data))
        struct.pack_into('>I', rec, e.rec_off + 14, len(data))
        self.write_lba(e.rec_lba, rec)
        e.lba, e.size = new_lba, len(data)

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(self.img)


if __name__ == '__main__':
    import sys
    d = Disc(sys.argv[1])
    for e in sorted(d.files(), key=lambda e: e.lba):
        print(f'{e.lba:7d} {e.size:9d} {e.path}')
