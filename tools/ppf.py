"""PPF 3.0 creation (diff of two equal-size images) and application."""
import struct

MAX_CHUNK = 255


def create(orig, mod, description='', out=None):
    assert len(orig) == len(mod), 'images must be the same size'
    desc = description.encode('ascii')[:50].ljust(50, b' ')
    # magic, encoding 2 (PPF3), description, imagetype 0 (BIN), no blockcheck, no undo, pad
    buf = bytearray(b'PPF30' + bytes([2]) + desc + bytes([0, 0, 0, 0]))
    n = len(orig)
    i = 0
    # compare in 64 KiB blocks to skip identical regions quickly
    BLK = 1 << 16
    while i < n:
        if orig[i:i + BLK] == mod[i:i + BLK]:
            i += BLK
            continue
        end = min(i + BLK, n)
        while i < end:
            if orig[i] == mod[i]:
                i += 1
                continue
            j = i
            while j < n and j - i < MAX_CHUNK and orig[j] != mod[j]:
                j += 1
            buf += struct.pack('<QB', i, j - i) + mod[i:j]
            i = j
    if out:
        with open(out, 'wb') as f:
            f.write(buf)
    return bytes(buf)


def apply(img, patch):
    """Apply a PPF3 patch to a bytearray in place."""
    assert patch[:5] == b'PPF30', 'only PPF 3.0 supported'
    blockcheck, undo = patch[57], patch[58]
    p = 60 + (1024 if blockcheck else 0)
    end = len(patch)
    diz = patch.rfind(b'@BEGIN_FILE_ID.DIZ')
    if diz >= 0 and patch.find(b'@END_FILE_ID.DIZ', diz) >= 0:
        end = diz  # FILE_ID.DIZ trailer (its length field is 2 or 4 bytes depending on the tool)
    while p < end:
        off, ln = struct.unpack_from('<QB', patch, p)
        p += 9
        img[off:off + ln] = patch[p:p + ln]
        p += ln + (ln if undo else 0)
    return img


if __name__ == '__main__':
    import sys
    from pathlib import Path
    if len(sys.argv) != 4:
        sys.exit('usage: ppf.py IMAGE PATCH OUT  (apply a PPF 3.0 patch to a copy of IMAGE)')
    img = bytearray(Path(sys.argv[1]).read_bytes())
    Path(sys.argv[3]).write_bytes(apply(img, Path(sys.argv[2]).read_bytes()))
