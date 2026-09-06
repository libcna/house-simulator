#!/usr/bin/env python3
"""Generate the fallback assets: the things drawn when the real asset could not be loaded.

`HOUSE-00144`. All four are AUTHORED HERE, procedurally, which settles their provenance completely
(ADR-0012): there is no source to trace, no licence to read, and no question about redistribution.
That matters more for these than for anything else in the project, because a fallback ships in every
build and is the one asset guaranteed to reach a player's screen if anything goes wrong.

They are also deliberately UGLY. A fallback that looks plausible is worse than no fallback: it hides
a missing asset until someone notices the fridge is a grey box, which may be months. Magenta and a
visible checker are the convention for exactly that reason.

Usage: make_fallback_assets.py <assets-src directory>
"""
import math
import os
import struct
import sys
import zlib


def write_png(path, width, height, pixel):
    """A PNG from a function (x, y) -> (r, g, b, a)."""
    rows = b''
    for y in range(height):
        rows += b'\x00' + b''.join(bytes(pixel(x, y)) for x in range(width))

    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(rows, 9))
           + chunk(b'IEND', b''))
    open(path, 'wb').write(png)
    return len(png)


def write_wav(path, seconds=0.25, rate=44100):
    """Silence. Not an empty file: a zero-length sound is a different failure mode from a
    successfully-loaded silent one, and the fallback has to behave like a real SoundEffect --
    including reporting a duration and finishing."""
    frames = b'\x00\x00' * int(rate * seconds)
    header = (b'RIFF' + struct.pack('<I', 36 + len(frames)) + b'WAVE'
              + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
              + b'data' + struct.pack('<I', len(frames)))
    open(path, 'wb').write(header + frames)
    return len(header) + len(frames)


def write_glb(path):
    """A unit cube, 1 m on a side, sitting on the origin plane.

    One metre because that is the scale check's own unit (§70.5): a fallback that is the wrong size
    would make a missing prop look like a scale bug, which is a worse diagnosis than a missing prop.
    """
    lo, hi = (-0.5, 0.0, -0.5), (0.5, 1.0, 0.5)
    faces = [
        ((0, 0, 1), [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),
        ((0, 0, -1), [(1, 0, 0), (0, 0, 0), (0, 1, 0), (1, 1, 0)]),
        ((1, 0, 0), [(1, 0, 1), (1, 0, 0), (1, 1, 0), (1, 1, 1)]),
        ((-1, 0, 0), [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)]),
        ((0, 1, 0), [(0, 1, 1), (1, 1, 1), (1, 1, 0), (0, 1, 0)]),
        ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
    ]
    pos, nrm, uv, idx = [], [], [], []
    for normal, corners in faces:
        base = len(pos)
        for c, (cx, cy, cz) in enumerate(corners):
            pos.append((lo[0] + cx * (hi[0] - lo[0]),
                        lo[1] + cy * (hi[1] - lo[1]),
                        lo[2] + cz * (hi[2] - lo[2])))
            nrm.append(tuple(float(v) for v in normal))
            uv.append((float(c in (1, 2)), float(c in (2, 3))))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]

    blob = b''
    views, accessors = [], []

    def add(data, fmt, ctype, atype, target):
        nonlocal blob
        offset = len(blob)
        raw = b''.join(struct.pack('<' + fmt, *v) if isinstance(v, tuple)
                       else struct.pack('<' + fmt, v) for v in data)
        blob += raw + b'\0' * ((4 - len(raw) % 4) % 4)
        views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(raw), 'target': target})
        accessor = {'bufferView': len(views) - 1, 'componentType': ctype, 'count': len(data),
                    'type': atype}
        if isinstance(data[0], tuple):
            n = len(data[0])
            accessor['min'] = [min(v[i] for v in data) for i in range(n)]
            accessor['max'] = [max(v[i] for v in data) for i in range(n)]
        else:
            accessor['min'], accessor['max'] = [min(data)], [max(data)]
        accessors.append(accessor)
        return len(accessors) - 1

    a_pos = add(pos, '3f', 5126, 'VEC3', 34962)
    a_nrm = add(nrm, '3f', 5126, 'VEC3', 34962)
    a_uv = add(uv, '2f', 5126, 'VEC2', 34962)
    a_idx = add(idx, 'H', 5123, 'SCALAR', 34963)

    gltf = {
        'asset': {'version': '2.0', 'generator': 'cna-house tools/assets/make_fallback_assets.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}],
        'nodes': [{'name': 'FallbackBox', 'mesh': 0}],
        'meshes': [{'name': 'FallbackBox', 'primitives': [
            {'attributes': {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv},
             'indices': a_idx, 'mode': 4, 'material': 0}]}],
        'materials': [{'name': 'FallbackMaterial', 'doubleSided': False,
                       'pbrMetallicRoughness': {'baseColorFactor': [0.6, 0.6, 0.6, 1.0],
                                                'metallicFactor': 0.0, 'roughnessFactor': 1.0}}],
        'accessors': accessors, 'bufferViews': views, 'buffers': [{'byteLength': len(blob)}],
    }
    import json
    j = json.dumps(gltf, separators=(',', ':')).encode()
    j += b' ' * ((4 - len(j) % 4) % 4)
    out = (struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(blob))
           + struct.pack('<II', len(j), 0x4E4F534A) + j
           + struct.pack('<II', len(blob), 0x004E4942) + blob)
    open(path, 'wb').write(out)
    return len(out)


def main(root):
    models = os.path.join(root, 'Models', 'Fallback')
    textures = os.path.join(root, 'Textures', 'Fallback')
    audio = os.path.join(root, 'Audio', 'Fallback')
    for directory in (models, textures, audio):
        os.makedirs(directory, exist_ok=True)

    written = []

    # The grey box. Neutral, so a scene of them is readable rather than a migraine, and 1 m on a
    # side so a missing prop does not also look like a scale bug.
    written.append(('Models/Fallback/box.glb', write_glb(os.path.join(models, 'box.glb'))))

    # Mid-grey. The stand-in for an albedo that did not load: neutral enough that lighting still
    # reads correctly, so a missing TEXTURE does not also look like a lighting bug.
    written.append(('Textures/Fallback/grey.png',
                    write_png(os.path.join(textures, 'grey.png'), 4, 4,
                              lambda x, y: (128, 128, 128, 255))))

    # Magenta and black, 8x8 checker. Deliberately hideous, and checkered so it is recognisable
    # even at a grazing angle or through a mip chain -- flat magenta can be mistaken for an
    # authored colour, a checker cannot.
    written.append(('Textures/Fallback/missing.png',
                    write_png(os.path.join(textures, 'missing.png'), 16, 16,
                              lambda x, y: (255, 0, 255, 255) if ((x // 8) + (y // 8)) % 2 == 0
                              else (0, 0, 0, 255))))

    # A quarter second of silence. Not an empty file: a zero-length sound is a different failure
    # from a successfully-loaded silent one, and the fallback must behave like a real SoundEffect,
    # reporting a duration and finishing.
    written.append(('Audio/Fallback/silent.wav', write_wav(os.path.join(audio, 'silent.wav'))))

    for name, size in written:
        print('%-34s %6d bytes' % (name, size))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'assets-src')
