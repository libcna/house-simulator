#!/usr/bin/env python3
"""p1-split-skins.py -- split a multi-skin .glb into one single-skin .glb per skin.

HOUSE-00076. CNA exposes a Model's second and later skins only through `getSkinsEXTProperty()`,
which ADR-0001 forbids the runtime to call. Rather than weaken that rule, `cna-house` splits multi-
skin source assets OFFLINE, where nothing is constrained, and ships one skin per runtime `.glb`.

What the split keeps, and why:

  * the WHOLE node graph, including every joint of every skin. The parts must reassemble on a
    shared skeleton, so the same pose evaluated once drives all of them; pruning the other part's
    joints would give each part a different bone numbering and make a shared clip impossible.
  * the whole buffer and every accessor. Pruning the buffer is a size optimisation, not a
    correctness one, and this tool's job here is to establish the SHAPE of the split. The
    production tool (HOUSE-00224) prunes; this one records that it does not.

What it drops: every mesh not bound to the kept skin, and every other skin.

It also writes an attachment record per part -- the skeleton root, the skin's joint names in the
order the vertex blend indices reference them, and the node each part hangs from. That record is
what a `.chanim` sidecar carries, and it is the reason the runtime never needs a skins query.

Usage: p1-split-skins.py <input.glb> <outdir>
"""
import json, os, struct, sys


def read_glb(path):
    data = open(path, 'rb').read()
    magic, version, _length = struct.unpack_from('<III', data, 0)
    if magic != 0x46546C67 or version != 2:
        raise SystemExit('%s is not a glTF 2.0 binary container' % path)
    off, gltf, blob = 12, None, b''
    while off < len(data):
        clen, ctype = struct.unpack_from('<II', data, off)
        chunk = data[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A:
            gltf = json.loads(chunk.decode('utf-8'))
        elif ctype == 0x004E4942:
            blob = chunk
        off += 8 + clen
    if gltf is None:
        raise SystemExit('%s has no JSON chunk' % path)
    return gltf, blob


def write_glb(path, gltf, blob):
    j = json.dumps(gltf, separators=(',', ':')).encode()
    j += b' ' * ((4 - len(j) % 4) % 4)
    b = blob + b'\0' * ((4 - len(blob) % 4) % 4)
    out = (struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(b))
           + struct.pack('<II', len(j), 0x4E4F534A) + j
           + struct.pack('<II', len(b), 0x004E4942) + b)
    open(path, 'wb').write(out)


def split(path, outdir):
    gltf, blob = read_glb(path)
    skins = gltf.get('skins', [])
    if len(skins) < 2:
        print('%s has %d skin(s); nothing to split' % (path, len(skins)))
        return []
    nodes = gltf['nodes']
    base = os.path.splitext(os.path.basename(path))[0]
    records = []
    for si, skin in enumerate(skins):
        part = dict(gltf)
        part['nodes'] = [dict(n) for n in nodes]
        # keep only the meshes this skin drives, renumbered densely
        keep = [i for i, n in enumerate(part['nodes']) if n.get('skin') == si]
        meshIds = [part['nodes'][i]['mesh'] for i in keep if 'mesh' in part['nodes'][i]]
        remap = {m: k for k, m in enumerate(meshIds)}
        part['meshes'] = [gltf['meshes'][m] for m in meshIds]
        part['skins'] = [skin]
        for i, n in enumerate(part['nodes']):
            if n.get('skin') is None:
                continue
            if i in keep:
                n['skin'] = 0
                n['mesh'] = remap[n['mesh']]
            else:
                n.pop('skin', None)
                n.pop('mesh', None)
        name = '%s%s' % (base.replace('TwoSkin', 'Part'), skin.get('name', str(si))[-1:])
        out = os.path.join(outdir, name + '.glb')
        write_glb(out, part, blob)

        record = {
            'part': name,
            'source': os.path.basename(path),
            'skin': skin.get('name', 'skin%d' % si),
            # the skeleton every part shares -- the record's whole point
            'skeletonRoot': nodes[gltf['scenes'][gltf.get('scene', 0)]['nodes'][0]].get('name'),
            'attachmentBone': nodes[skin['skeleton']].get('name') if 'skeleton' in skin else None,
            # the joint order the vertex blend indices reference, by NAME. HOUSE-00074 measured
            # that those indices are skin-local, so this list IS the binding.
            'joints': [nodes[j].get('name') for j in skin['joints']],
            'meshNodes': [part['nodes'][i].get('name') for i in keep],
            'bufferPruned': False,
        }
        open(os.path.join(outdir, name + '.attach.json'), 'w').write(
            json.dumps(record, indent=2) + '\n')
        records.append(record)
        print('%s: %d mesh(es), joints %s, attaches at %s'
              % (name, len(part['meshes']), record['joints'], record['attachmentBone']))
    return records


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    os.makedirs(sys.argv[2], exist_ok=True)
    split(sys.argv[1], sys.argv[2])
