#!/usr/bin/env python3
"""Generate the deterministic .glb fixtures the phase-1 model probes measure against.

Self-authored geometry only -- no downloaded asset, no licence question, and every number in the
file is derivable from this script, so a probe can compare against an *analytic* expectation
rather than against "whatever the importer produced".

Usage: p1-make-gltf.py <outdir>
"""
import json, struct, sys, os

def glb(gltf: dict, blob: bytes) -> bytes:
    j = json.dumps(gltf, separators=(',', ':')).encode()
    j += b' ' * ((4 - len(j) % 4) % 4)
    b = blob + b'\0' * ((4 - len(blob) % 4) % 4)
    return (struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(b))
            + struct.pack('<II', len(j), 0x4E4F534A) + j
            + struct.pack('<II', len(b), 0x004E4942) + b)

def accessor(gltf, blob, data, fmt, ctype, atype, target, minmax=True):
    """Append one accessor + bufferView, return the accessor index."""
    off = len(blob)
    assert off % 4 == 0
    raw = b''.join(struct.pack('<' + fmt, *v) if isinstance(v, (list, tuple))
                   else struct.pack('<' + fmt, v) for v in data)
    blob += raw
    blob += b'\0' * ((4 - len(blob) % 4) % 4)
    gltf['bufferViews'].append({'buffer': 0, 'byteOffset': off,
                                'byteLength': len(raw), 'target': target})
    acc = {'bufferView': len(gltf['bufferViews']) - 1, 'componentType': ctype,
           'count': len(data), 'type': atype}
    if minmax:
        if isinstance(data[0], (list, tuple)):
            n = len(data[0])
            acc['min'] = [min(v[i] for v in data) for i in range(n)]
            acc['max'] = [max(v[i] for v in data) for i in range(n)]
        else:
            acc['min'] = [min(data)]
            acc['max'] = [max(data)]
    gltf['accessors'].append(acc)
    return len(gltf['accessors']) - 1, blob

# --- the box ----------------------------------------------------------------------------------
# Deliberately asymmetric: all six plane coordinates are distinct, so ANY axis swap, negation or
# handedness flip in the importer moves a number that the probe checks by name.
BMIN = (-1.0, -2.0, -3.0)
BMAX = ( 4.0,  5.0,  6.0)

# outward-facing, counter-clockwise as glTF 2.0 requires (§3.7.2.1 "the winding order determines
# front- vs back-facing ... counter-clockwise ... is front-facing")
FACES = [
    ((0, 0, 1),  [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),   # +Z
    ((0, 0, -1), [(1, 0, 0), (0, 0, 0), (0, 1, 0), (1, 1, 0)]),   # -Z
    ((1, 0, 0),  [(1, 0, 1), (1, 0, 0), (1, 1, 0), (1, 1, 1)]),   # +X
    ((-1, 0, 0), [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)]),   # -X
    ((0, 1, 0),  [(0, 1, 1), (1, 1, 1), (1, 1, 0), (0, 1, 0)]),   # +Y
    ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),   # -Y
]

def box(bmin=BMIN, bmax=BMAX):
    pos, nrm, uv, idx = [], [], [], []
    for f, (n, corners) in enumerate(FACES):
        base = len(pos)
        for c, (cx, cy, cz) in enumerate(corners):
            pos.append((bmin[0] + cx * (bmax[0] - bmin[0]),
                        bmin[1] + cy * (bmax[1] - bmin[1]),
                        bmin[2] + cz * (bmax[2] - bmin[2])))
            nrm.append(tuple(float(x) for x in n))
            # asymmetric, per-vertex-unique, and never 0.5 -- so a V flip or a channel swap shows
            uv.append((0.0625 * (f + 1), 0.0625 * (c + 1) + 0.5))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return pos, nrm, uv, idx

def write_static(path):
    pos, nrm, uv, idx = box()
    g = {'asset': {'version': '2.0', 'generator': 'cna-house p1-make-gltf.py'},
         'scene': 0, 'scenes': [{'nodes': [0]}],
         'nodes': [{'name': 'P1SlabNode', 'mesh': 0}],
         'meshes': [{'name': 'P1Slab', 'primitives': [{'attributes': {}, 'mode': 4,
                                                       'material': 0}]}],
         'materials': [{'name': 'P1Mat', 'doubleSided': False,
                        'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0],
                                                 'metallicFactor': 0.0,
                                                 'roughnessFactor': 1.0}}],
         'accessors': [], 'bufferViews': [], 'buffers': []}
    blob = b''
    a_pos, blob = accessor(g, blob, pos, '3f', 5126, 'VEC3', 34962)
    a_nrm, blob = accessor(g, blob, nrm, '3f', 5126, 'VEC3', 34962)
    a_uv,  blob = accessor(g, blob, uv,  '2f', 5126, 'VEC2', 34962)
    a_idx, blob = accessor(g, blob, idx, 'H',  5123, 'SCALAR', 34963)
    p = g['meshes'][0]['primitives'][0]
    p['attributes'] = {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv}
    p['indices'] = a_idx
    g['buffers'].append({'byteLength': len(blob)})
    open(path, 'wb').write(glb(g, blob))
    return {'min': BMIN, 'max': BMAX, 'vertices': len(pos), 'indices': len(idx),
            'positions': pos, 'uvs': uv, 'normals': nrm, 'index_data': idx}

# --- the single-sided quad --------------------------------------------------------------------
# A CLOSED solid cannot measure winding: with either cull mode you still see a face -- the near one
# or, through it, the far one -- and the silhouette is identical. The winding probe therefore needs
# an OPEN, single-sided surface, where the wrong cull mode renders nothing at all.
QMIN = (-1.0, -2.0, 0.0)
QMAX = ( 4.0,  5.0, 0.0)

def write_quad(path):
    # counter-clockwise seen from +Z, which is glTF's front face
    pos = [(QMIN[0], QMIN[1], 0.0), (QMAX[0], QMIN[1], 0.0),
           (QMAX[0], QMAX[1], 0.0), (QMIN[0], QMAX[1], 0.0)]
    nrm = [(0.0, 0.0, 1.0)] * 4
    uv = [(0.125, 0.625), (0.875, 0.625), (0.875, 0.1875), (0.125, 0.1875)]
    idx = [0, 1, 2, 0, 2, 3]
    g = {'asset': {'version': '2.0', 'generator': 'cna-house p1-make-gltf.py'},
         'scene': 0, 'scenes': [{'nodes': [0]}],
         'nodes': [{'name': 'P1QuadNode', 'mesh': 0}],
         'meshes': [{'name': 'P1Quad', 'primitives': [{'attributes': {}, 'mode': 4,
                                                       'material': 0}]}],
         'materials': [{'name': 'P1QuadMat', 'doubleSided': False,
                        'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0],
                                                 'metallicFactor': 0.0,
                                                 'roughnessFactor': 1.0}}],
         'accessors': [], 'bufferViews': [], 'buffers': []}
    blob = b''
    a_pos, blob = accessor(g, blob, pos, '3f', 5126, 'VEC3', 34962)
    a_nrm, blob = accessor(g, blob, nrm, '3f', 5126, 'VEC3', 34962)
    a_uv, blob = accessor(g, blob, uv, '2f', 5126, 'VEC2', 34962)
    a_idx, blob = accessor(g, blob, idx, 'H', 5123, 'SCALAR', 34963)
    p = g['meshes'][0]['primitives'][0]
    p['attributes'] = {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv}
    p['indices'] = a_idx
    g['buffers'].append({'byteLength': len(blob)})
    open(path, 'wb').write(glb(g, blob))
    return {'min': QMIN, 'max': QMAX}

# --- the node hierarchy -----------------------------------------------------------------------
# HOUSE-00072 needs a skeleton whose absolute transforms are wrong under ANY of the plausible
# mistakes: a transposed matrix, a reversed multiplication order, a column/row-vector mix-up, or a
# parent link read from the wrong node. So: depth three, a sibling branch, a non-commuting
# rotation, a non-uniform scale, and one node authored as an explicit `matrix` rather than TRS --
# which is the only place CNA's importer converts anything at all.
#
# Every matrix below is built in XNA's convention directly: ROW-major storage, ROW-vector
# transform (p' = p * M), local = Scale * Rotation * Translation, absolute = local * absolute(parent).
def mat_identity():
    return [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]

def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

def mat_scale(sx, sy, sz):
    m = mat_identity(); m[0][0], m[1][1], m[2][2] = sx, sy, sz; return m

def mat_translate(x, y, z):
    m = mat_identity(); m[3][0], m[3][1], m[3][2] = x, y, z; return m

def mat_from_quat(x, y, z, w):
    """XNA Matrix::CreateFromQuaternion, row-vector convention."""
    m = mat_identity()
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    m[0][0] = 1 - 2 * (yy + zz); m[0][1] = 2 * (xy + wz);     m[0][2] = 2 * (xz - wy)
    m[1][0] = 2 * (xy - wz);     m[1][1] = 1 - 2 * (xx + zz); m[1][2] = 2 * (yz + wx)
    m[2][0] = 2 * (xz + wy);     m[2][1] = 2 * (yz - wx);     m[2][2] = 1 - 2 * (xx + yy)
    return m

def local_matrix(t, q, s):
    return mat_mul(mat_mul(mat_scale(*s), mat_from_quat(*q)), mat_translate(*t))

SQ = 0.7071067811865476   # sin/cos of 45 degrees -> a 90-degree turn about Z
# name, parent index, translation, quaternion(x,y,z,w), scale, authored-as
HIER = [
    ('P1Base', -1, (10.0, 0.0, 0.0),  (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0), 'trs'),
    ('P1Mid',   0, (0.0, 5.0, 0.0),   (0.0, 0.0, SQ,  SQ),  (1.0, 1.0, 1.0), 'trs'),
    ('P1Tip',   1, (0.0, 0.0, 3.0),   (0.0, 0.0, 0.0, 1.0), (2.0, 0.5, 4.0), 'matrix'),
    ('P1Wing',  0, (0.0, 0.0, -4.0),  (SQ,  0.0, 0.0, SQ),  (1.0, 1.0, 1.0), 'trs'),
]

def write_hier(path):
    pos, nrm, uv, idx = box((-0.5, -0.5, -0.5), (0.5, 0.5, 0.5))
    g = {'asset': {'version': '2.0', 'generator': 'cna-house p1-make-gltf.py'},
         'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [], 'meshes': [],
         'materials': [{'name': 'P1HierMat',
                        'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0]}}],
         'accessors': [], 'bufferViews': [], 'buffers': []}
    blob = b''
    a_pos, blob = accessor(g, blob, pos, '3f', 5126, 'VEC3', 34962)
    a_nrm, blob = accessor(g, blob, nrm, '3f', 5126, 'VEC3', 34962)
    a_uv,  blob = accessor(g, blob, uv,  '2f', 5126, 'VEC2', 34962)
    a_idx, blob = accessor(g, blob, idx, 'H',  5123, 'SCALAR', 34963)
    locals_, absolutes = [], []
    for i, (name, parent, t, q, s, kind) in enumerate(HIER):
        g['meshes'].append({'name': name + 'Mesh', 'primitives': [
            {'attributes': {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv},
             'indices': a_idx, 'mode': 4, 'material': 0}]})
        node = {'name': name, 'mesh': i}
        if kind == 'trs':
            node['translation'] = list(t)
            node['rotation'] = list(q)
            node['scale'] = list(s)
        else:
            # glTF stores a matrix column-major with the column-vector convention; that is the
            # numerical transpose of XNA's row-major row-vector matrix for the SAME transform, so
            # the flat 16 floats are exactly the row-major flattening of the XNA matrix.
            m = local_matrix(t, q, s)
            node['matrix'] = [m[r][c] for r in range(4) for c in range(4)]
        children = [j for j, h in enumerate(HIER) if h[1] == i]
        if children:
            node['children'] = children
        g['nodes'].append(node)
        m = local_matrix(t, q, s)
        locals_.append(m)
        absolutes.append(m if parent < 0 else mat_mul(m, absolutes[parent]))
    g['buffers'].append({'byteLength': len(blob)})
    open(path, 'wb').write(glb(g, blob))
    return locals_, absolutes

def cfloat(v):
    """A C++ float literal that is exact and always well-formed -- '%g' alone yields '1f'."""
    t = '%.9g' % v
    if '.' not in t and 'e' not in t and 'n' not in t:
        t += '.0'
    return t + 'f'

def mat_literal(m):
    return '{' + ','.join(cfloat(m[r][c]) for r in range(4) for c in range(4)) + '}'

# --- the skinned ribbon -----------------------------------------------------------------------
# HOUSE-00074 asks a question that only a fixture with a KNOWN vertex-to-joint binding can answer:
# do the blend indices in the compiled vertex buffer index `Model::Bones`, or the skin's own joint
# list? Those two differ by exactly the synthetic `Root` bone HOUSE-00072 found, so a fixture whose
# every vertex has an analytically known joint distinguishes them on the first run.
#
# A flat two-column ribbon, five rows tall, bound to a three-joint chain. Row 0 is J0 alone, row 4
# is J2 alone, rows 1 and 3 are 50/50 blends -- so a probe can also tell a dropped weight from a
# swapped one.
SKIN_JOINTS = [
    # name, parent (-1 = the skin root), local translation, world bind translation
    ('P1J0', -1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
    ('P1J1',  0, (0.0, 2.0, 0.0), (0.0, 2.0, 0.0)),
    ('P1J2',  1, (0.0, 2.0, 0.0), (0.0, 4.0, 0.0)),
]
# row -> [(joint, weight), ...]; every row sums to exactly 1.0 in binary floating point
SKIN_ROWS = [
    [(0, 1.0)],
    [(0, 0.5), (1, 0.5)],
    [(1, 1.0)],
    [(1, 0.5), (2, 0.5)],
    [(2, 1.0)],
]

def write_skin(path):
    pos, nrm, uv, joints, weights, idx = [], [], [], [], [], []
    for row in range(5):
        for col in range(2):
            pos.append((-0.5 + col * 1.0, float(row), 0.0))
            nrm.append((0.0, 0.0, 1.0))
            uv.append((0.25 * col + 0.125, 0.125 * row + 0.0625))
            j = [0, 0, 0, 0]
            w = [0.0, 0.0, 0.0, 0.0]
            for k, (ji, wi) in enumerate(SKIN_ROWS[row]):
                j[k], w[k] = ji, wi
            joints.append(tuple(j))
            weights.append(tuple(w))
    for row in range(4):
        b = row * 2
        idx += [b, b + 1, b + 3, b, b + 3, b + 2]   # counter-clockwise seen from +Z

    g = {'asset': {'version': '2.0', 'generator': 'cna-house p1-make-gltf.py'},
         'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [],
         'meshes': [{'name': 'P1Ribbon', 'primitives': [{'attributes': {}, 'mode': 4,
                                                         'material': 0}]}],
         'materials': [{'name': 'P1SkinMat',
                        'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0]}}],
         'skins': [], 'accessors': [], 'bufferViews': [], 'buffers': []}
    # node 0 is the skin root; nodes 1..3 the joint chain; node 4 the skinned mesh
    g['nodes'].append({'name': 'P1SkinRoot', 'children': [1, 4]})
    for i, (name, parent, t, _w) in enumerate(SKIN_JOINTS):
        node = {'name': name, 'translation': list(t)}
        kids = [1 + j for j, h in enumerate(SKIN_JOINTS) if h[1] == i]
        if kids:
            node['children'] = kids
        g['nodes'].append(node)
    g['nodes'].append({'name': 'P1RibbonNode', 'mesh': 0, 'skin': 0})

    blob = b''
    a_pos, blob = accessor(g, blob, pos, '3f', 5126, 'VEC3', 34962)
    a_nrm, blob = accessor(g, blob, nrm, '3f', 5126, 'VEC3', 34962)
    a_uv,  blob = accessor(g, blob, uv,  '2f', 5126, 'VEC2', 34962)
    a_jnt, blob = accessor(g, blob, joints,  '4H', 5123, 'VEC4', 34962)
    a_wgt, blob = accessor(g, blob, weights, '4f', 5126, 'VEC4', 34962)
    a_idx, blob = accessor(g, blob, idx, 'H', 5123, 'SCALAR', 34963)
    # inverse bind matrices: the inverse of each joint's world bind transform, which for a pure
    # translation chain is the negated translation. Serialised the same way as any glTF matrix.
    ibms = []
    for (_n, _p, _t, wt) in SKIN_JOINTS:
        m = mat_translate(-wt[0], -wt[1], -wt[2])
        ibms.append(tuple(m[r][c] for r in range(4) for c in range(4)))
    a_ibm, blob = accessor(g, blob, ibms, '16f', 5126, 'MAT4', None, minmax=False)
    g['bufferViews'][-1].pop('target', None)

    g['skins'].append({'name': 'P1Skin', 'joints': [1, 2, 3],
                       'skeleton': 1, 'inverseBindMatrices': a_ibm})
    p = g['meshes'][0]['primitives'][0]
    p['attributes'] = {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv,
                       'JOINTS_0': a_jnt, 'WEIGHTS_0': a_wgt}
    p['indices'] = a_idx
    g['buffers'].append({'byteLength': len(blob)})
    open(path, 'wb').write(glb(g, blob))
    return {'vertices': len(pos), 'joints': [j[0] for j in SKIN_JOINTS]}

# --- the two-skin source ----------------------------------------------------------------------
# HOUSE-00076: CNA exposes more than one skin per Model only through `getSkinsEXTProperty()`, which
# ADR-0001 forbids. The project's answer is to split offline, one runtime .glb per skin, on a
# SHARED skeleton. This fixture is the thing to split: two ribbons, two skins, one skeleton, and
# a bind pose that skins to the identity so the control render is unambiguous.
TWOSKIN = [
    # part, joint names, root world x
    ('A', ('P1AJ0', 'P1AJ1'), -2.0),
    ('B', ('P1BJ0', 'P1BJ1'),  2.0),
]

def write_twoskin(path):
    g = {'asset': {'version': '2.0', 'generator': 'cna-house p1-make-gltf.py'},
         'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [], 'meshes': [], 'skins': [],
         'materials': [{'name': 'P1TwoMat',
                        'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0]}}],
         'accessors': [], 'bufferViews': [], 'buffers': []}
    blob = b''
    # node 0 is the shared skeleton root; then, per part, two joints and one mesh node
    g['nodes'].append({'name': 'P1TwoRoot', 'children': []})
    for pi, (part, joints, rootx) in enumerate(TWOSKIN):
        pos, nrm, uv, jnt, wgt, idx = [], [], [], [], [], []
        for row in range(5):
            for col in range(2):
                pos.append((rootx - 0.5 + col * 1.0, float(row), 0.0))
                nrm.append((0.0, 0.0, 1.0))
                uv.append((0.25 * col + 0.125, 0.125 * row + 0.0625))
                if row <= 1:
                    j, w = (0, 0, 0, 0), (1.0, 0.0, 0.0, 0.0)
                elif row == 2:
                    j, w = (0, 1, 0, 0), (0.5, 0.5, 0.0, 0.0)
                else:
                    j, w = (1, 0, 0, 0), (1.0, 0.0, 0.0, 0.0)
                jnt.append(j)
                wgt.append(w)
        for row in range(4):
            b = row * 2
            idx += [b, b + 1, b + 3, b, b + 3, b + 2]
        a_pos, blob = accessor(g, blob, pos, '3f', 5126, 'VEC3', 34962)
        a_nrm, blob = accessor(g, blob, nrm, '3f', 5126, 'VEC3', 34962)
        a_uv,  blob = accessor(g, blob, uv,  '2f', 5126, 'VEC2', 34962)
        a_jnt, blob = accessor(g, blob, jnt, '4H', 5123, 'VEC4', 34962)
        a_wgt, blob = accessor(g, blob, wgt, '4f', 5126, 'VEC4', 34962)
        a_idx, blob = accessor(g, blob, idx, 'H', 5123, 'SCALAR', 34963)
        ibm = [tuple(mat_translate(-rootx, 0.0, 0.0)[r][c] for r in range(4) for c in range(4)),
               tuple(mat_translate(-rootx, -2.0, 0.0)[r][c] for r in range(4) for c in range(4))]
        a_ibm, blob = accessor(g, blob, ibm, '16f', 5126, 'MAT4', None, minmax=False)
        g['bufferViews'][-1].pop('target', None)

        base = len(g['nodes'])
        g['nodes'].append({'name': joints[0], 'translation': [rootx, 0.0, 0.0],
                           'children': [base + 1]})
        g['nodes'].append({'name': joints[1], 'translation': [0.0, 2.0, 0.0]})
        g['meshes'].append({'name': 'P1Ribbon' + part, 'primitives': [
            {'attributes': {'POSITION': a_pos, 'NORMAL': a_nrm, 'TEXCOORD_0': a_uv,
                            'JOINTS_0': a_jnt, 'WEIGHTS_0': a_wgt},
             'indices': a_idx, 'mode': 4, 'material': 0}]})
        g['skins'].append({'name': 'P1Skin' + part, 'joints': [base, base + 1],
                           'skeleton': base, 'inverseBindMatrices': a_ibm})
        g['nodes'].append({'name': 'P1RibbonNode' + part, 'mesh': pi, 'skin': pi})
        g['nodes'][0]['children'] += [base, base + 2]
    g['buffers'].append({'byteLength': len(blob)})
    open(path, 'wb').write(glb(g, blob))
    return TWOSKIN

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out, exist_ok=True)
    info = write_static(os.path.join(out, 'P1Static.glb'))
    print('P1Static.glb: %d vertices, %d indices, min=%s max=%s'
          % (info['vertices'], info['indices'], info['min'], info['max']))
    # the analytic expectation the C++ probe is compiled against
    with open(os.path.join(out, 'p1-static-expected.h'), 'w') as f:
        f.write('// generated by p1-make-gltf.py -- do not edit\n')
        f.write('static const float kExpectedMin[3] = {%ff,%ff,%ff};\n' % info['min'])
        f.write('static const float kExpectedMax[3] = {%ff,%ff,%ff};\n' % info['max'])
        f.write('static const int kExpectedVertexCount = %d;\n' % info['vertices'])
        f.write('static const int kExpectedIndexCount = %d;\n' % info['indices'])
        f.write('static const float kExpectedPos[%d][3] = {%s};\n'
                % (len(info['positions']),
                   ','.join('{%ff,%ff,%ff}' % p for p in info['positions'])))
        f.write('static const float kExpectedNrm[%d][3] = {%s};\n'
                % (len(info['normals']),
                   ','.join('{%ff,%ff,%ff}' % p for p in info['normals'])))
        f.write('static const float kExpectedUv[%d][2] = {%s};\n'
                % (len(info['uvs']), ','.join('{%ff,%ff}' % p for p in info['uvs'])))
        f.write('static const unsigned short kExpectedIdx[%d] = {%s};\n'
                % (len(info['index_data']), ','.join(str(i) for i in info['index_data'])))
    q = write_quad(os.path.join(out, 'P1Quad.glb'))
    print('P1Quad.glb: single-sided CCW quad, min=%s max=%s' % (q['min'], q['max']))
    with open(os.path.join(out, 'p1-static-expected.h'), 'a') as f:
        f.write('static const float kQuadMin[3] = {%ff,%ff,%ff};\n' % q['min'])
        f.write('static const float kQuadMax[3] = {%ff,%ff,%ff};\n' % q['max'])
    loc, absl = write_hier(os.path.join(out, 'P1Hier.glb'))
    print('P1Hier.glb: %d nodes, depth 3, one authored as an explicit matrix' % len(HIER))
    with open(os.path.join(out, 'p1-static-expected.h'), 'a') as f:
        f.write('static const int kHierCount = %d;\n' % len(HIER))
        f.write('static const char* const kHierName[%d] = {%s};\n'
                % (len(HIER), ','.join('"%s"' % h[0] for h in HIER)))
        f.write('static const int kHierParent[%d] = {%s};\n'
                % (len(HIER), ','.join(str(h[1]) for h in HIER)))
        f.write('static const float kHierLocal[%d][16] = {%s};\n'
                % (len(loc), ','.join(mat_literal(m) for m in loc)))
        f.write('static const float kHierAbsolute[%d][16] = {%s};\n'
                % (len(absl), ','.join(mat_literal(m) for m in absl)))
    sk = write_skin(os.path.join(out, 'P1Skin.glb'))
    print('P1Skin.glb: %d vertices, joints %s' % (sk['vertices'], sk['joints']))
    with open(os.path.join(out, 'p1-static-expected.h'), 'a') as f:
        f.write('static const int kSkinJointCount = %d;\n' % len(SKIN_JOINTS))
        f.write('static const char* const kSkinJointName[%d] = {%s};\n'
                % (len(SKIN_JOINTS), ','.join('"%s"' % j[0] for j in SKIN_JOINTS)))
        f.write('static const int kSkinJointParent[%d] = {%s};\n'
                % (len(SKIN_JOINTS), ','.join(str(j[1]) for j in SKIN_JOINTS)))
        f.write('static const int kSkinVertexCount = %d;\n' % sk['vertices'])
        # per vertex: the skin-local joint indices and weights the generator authored
        aj, aw = [], []
        for row in range(5):
            for _col in range(2):
                j = [0, 0, 0, 0]; w = [0.0, 0.0, 0.0, 0.0]
                for k, (ji, wi) in enumerate(SKIN_ROWS[row]):
                    j[k], w[k] = ji, wi
                aj.append(j); aw.append(w)
        f.write('static const int kSkinVertexJoint[%d][4] = {%s};\n'
                % (len(aj), ','.join('{%d,%d,%d,%d}' % tuple(v) for v in aj)))
        f.write('static const float kSkinVertexWeight[%d][4] = {%s};\n'
                % (len(aw), ','.join('{' + ','.join(cfloat(x) for x in v) + '}' for v in aw)))
    write_twoskin(os.path.join(out, 'P1TwoSkin.glb'))
    print('P1TwoSkin.glb: 2 skins, 2 meshes, 1 shared skeleton')
    print('wrote p1-static-expected.h')
