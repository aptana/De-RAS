"""Developer tool (not shipped): writes the GeoTIFF fixtures in tests/data/geotiff
for tests/test_m3.cpp. Values: z(c, r) = 100 + 0.25*c - 0.5*r (r = row from the top),
nodata -9999 at (c=2, r=1). Origin x0=500000, y0=9100000, cell 2 m x 2 m, EPSG 32748.
Uses Pillow (libtiff) for LZW/Deflate strips and hand-built files for tiles and predictors."""
import struct, sys, zlib
import numpy as np
from PIL import Image, TiffImagePlugin

out = sys.argv[1]
NX, NY = 37, 23
z = np.array([[100 + 0.25 * c - 0.5 * r for c in range(NX)] for r in range(NY)], dtype=np.float32)
z[1, 2] = -9999
geokeys = [1, 1, 0, 3, 1024, 0, 1, 1, 1025, 0, 1, 1, 3072, 0, 1, 32748]

def pil(name, comp):
    ifd = TiffImagePlugin.ImageFileDirectory_v2()
    ifd[33550] = (2.0, 2.0, 0.0); ifd.tagtype[33550] = 12
    ifd[33922] = (0.0, 0.0, 0.0, 500000.0, 9100000.0, 0.0); ifd.tagtype[33922] = 12
    ifd[34735] = tuple(geokeys); ifd.tagtype[34735] = 3
    ifd[42113] = "-9999"; ifd.tagtype[42113] = 2
    Image.fromarray(z, mode="F").save(f"{out}/{name}", compression=comp, tiffinfo=ifd)

pil("strip_none.tif", None)
pil("strip_lzw.tif", "tiff_lzw")
pil("strip_deflate.tif", "tiff_adobe_deflate")

def handmade(name, data_chunks, bw, bh, tiled, bps, fmt, pred, comp, be=False):
    e = ">" if be else "<"
    tags = []
    def t(tag, typ, vals):
        tags.append((tag, typ, vals))
    t(256, 4, [NX]); t(257, 4, [NY]); t(258, 3, [bps]); t(259, 3, [comp]); t(262, 3, [1]); t(277, 3, [1])
    t(284, 3, [1]); t(317, 3, [pred]); t(339, 3, [fmt])
    if tiled:
        t(322, 3, [bw]); t(323, 3, [bh])
    else:
        t(278, 3, [bh])
    t(33550, 12, [2.0, 2.0, 0.0]); t(33922, 12, [0.0, 0.0, 0.0, 500000.0, 9100000.0, 0.0])
    t(34735, 3, geokeys); t(42113, 2, "-9999")
    off_tag, cnt_tag = (324, 325) if tiled else (273, 279)
    t(off_tag, 4, [0] * len(data_chunks)); t(cnt_tag, 4, [len(c) for c in data_chunks])
    tags.sort()
    fmtc = {3: "H", 4: "I", 12: "d"}
    head = (b"MM" if be else b"II") + struct.pack(e + "HI", 42, 8)
    ifd_size = 2 + 12 * len(tags) + 4
    extra = bytearray(); pos = 8 + ifd_size
    entries = []
    offsets_loc = None
    for tag, typ, vals in tags:
        if typ == 2:
            raw = vals.encode() + b"\0"; cnt = len(raw)
        else:
            raw = struct.pack(e + fmtc[typ] * len(vals), *vals); cnt = len(vals)
        if len(raw) <= 4:
            entries.append([tag, typ, cnt, raw.ljust(4, b"\0"), None])
        else:
            if (pos + len(extra)) % 2: extra += b"\0"
            entries.append([tag, typ, cnt, struct.pack(e + "I", pos + len(extra)), pos + len(extra)])
            extra += raw
    if (pos + len(extra)) % 2: extra += b"\0"
    data_start = pos + len(extra)
    offs = []; p = data_start
    for c in data_chunks:
        offs.append(p); p += len(c)
    for ent in entries:
        if ent[0] == off_tag:
            raw = struct.pack(e + "I" * len(offs), *offs)
            if ent[4] is None: ent[3] = raw.ljust(4, b"\0")
            else: extra[ent[4] - pos: ent[4] - pos + len(raw)] = raw
    ifd = struct.pack(e + "H", len(entries))
    for tag, typ, cnt, val, _ in entries:
        ifd += struct.pack(e + "HHI", tag, typ, cnt) + val
    ifd += struct.pack(e + "I", 0)
    with open(f"{out}/{name}", "wb") as f:
        f.write(head + ifd + bytes(extra) + b"".join(data_chunks))

def tiles(arr, bw, bh):
    for ty in range(0, NY, bh):
        for tx in range(0, NX, bw):
            blk = np.zeros((bh, bw), dtype=arr.dtype)
            sub = arr[ty:ty + bh, tx:tx + bw]
            blk[:sub.shape[0], :sub.shape[1]] = sub
            yield blk

def pred3(blk):  # floating point predictor, rows of big-endian byte planes then differencing
    rows = []
    for row in blk:
        b = row.astype(">f4").tobytes()
        n = len(row)
        planes = bytearray(4 * n)
        for k in range(n):
            for j in range(4):
                planes[j * n + k] = b[k * 4 + j]
        for i in range(len(planes) - 1, 0, -1):
            planes[i] = (planes[i] - planes[i - 1]) & 0xFF
        rows.append(bytes(planes))
    return b"".join(rows)

chunks = [zlib.compress(pred3(b)) for b in tiles(z, 16, 16)]
handmade("tile_deflate_pred3.tif", chunks, 16, 16, True, 32, 3, 3, 8)
# int16 decimetres, predictor 2, deflate strips of 5 rows, big endian
zi = np.round(np.where(z == -9999, -9999, (z - 90) * 10)).astype(np.int16)
def pred2(blk):
    d = blk.astype(np.int32).copy()
    d[:, 1:] = (blk[:, 1:].astype(np.int32) - blk[:, :-1].astype(np.int32))
    return ((d + 65536) % 65536).astype(">u2").astype(">i2").tobytes() if False else (((d + 32768) % 65536) - 32768).astype(">i2").tobytes()
chunks = [zlib.compress(pred2(zi[r:r + 5])) for r in range(0, NY, 5)]
handmade("strip_int16_pred2_be.tif", chunks, NX, 5, False, 16, 2, 2, 8, be=True)
print("ok")
