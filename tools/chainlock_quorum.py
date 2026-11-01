#!/usr/bin/env python3

ROUND_CONSTANTS = (
    0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
    0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
    0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
)
ROTATIONS = (
    (0, 36, 3, 41, 18), (1, 44, 10, 45, 2),
    (62, 6, 43, 15, 61), (28, 55, 25, 21, 56),
    (27, 20, 39, 8, 14),
)
MASK = (1 << 64) - 1
QUORUM_MAX = 50
MIN_NODES = 10
ED25519_P = (1 << 255) - 19
ED25519_L = (1 << 252) + 27742317777372353535851937790883648493
ED25519_D = -121665 * pow(121666, ED25519_P - 2, ED25519_P) % ED25519_P
ED25519_I = pow(2, (ED25519_P - 1) // 4, ED25519_P)
ED25519_BASE = (15112221349535400772501151409588531511454012693041857206046113283949847762202,
                46316835694926478169428394003475163141307993866256225615783033603165251855960, 1)
PINNED_FIELDS = ("activation_height", "epoch_length", "anchor_depth", "network_id", "genesis_hash", "parameter_set_hash")


def threshold(member_count):
    if member_count < MIN_NODES or member_count > QUORUM_MAX:
        raise ValueError("invalid ChainLock quorum size")
    return (68 * member_count + 99) // 100


def check_pinned(inputs, pinned):
    if pinned is None:
        raise ValueError("locally pinned network parameters are required")
    for field in PINNED_FIELDS:
        if inputs[field] != pinned[field]:
            raise ValueError("quorum inputs differ from pinned " + field)
    length = pinned["epoch_length"]
    if length <= 0 or pinned["activation_height"] % length or not 0 < pinned["anchor_depth"] < length:
        raise ValueError("invalid pinned epoch timing")
    if (len(bytes.fromhex(pinned["network_id"])) != 16
            or len(bytes.fromhex(pinned["genesis_hash"])) != 32
            or len(bytes.fromhex(pinned["parameter_set_hash"])) != 32):
        raise ValueError("invalid pinned network context")
    if inputs["epoch"] < pinned["activation_height"] // length + 2:
        raise ValueError("epoch precedes first lockable epoch")


def lock_epoch(lock, epoch_length):
    if epoch_length <= 0:
        raise ValueError("invalid pinned epoch length")
    blob = bytes.fromhex(lock["lock_blob"])
    if len(blob) < 48:
        raise ValueError("short ChainLock blob")
    height = int.from_bytes(blob[:8], "little")
    epoch = int.from_bytes(blob[40:48], "little")
    expected = height // epoch_length
    if (height != lock["height"] or blob[8:40] != bytes.fromhex(lock["block_id"])
            or epoch != expected or lock["quorum_epoch"] != expected):
        raise ValueError("ChainLock epoch does not match locked height")
    return expected


def check_lock_epoch(lock, inputs, pinned):
    check_pinned(inputs, pinned)
    expected = lock_epoch(lock, pinned["epoch_length"])
    if inputs["epoch"] != expected:
        raise ValueError("ChainLock epoch does not match locked height")
    return expected


def keccak256(message):
    padded = bytearray(message)
    padded.append(1)
    padded.extend(bytes((-len(padded)) % 136))
    padded[-1] ^= 0x80
    lanes = [0] * 25
    for block_start in range(0, len(padded), 136):
        block = padded[block_start:block_start + 136]
        for index in range(17):
            lanes[index] ^= int.from_bytes(block[index * 8:index * 8 + 8], "little")
        for constant in ROUND_CONSTANTS:
            columns = [lanes[x] ^ lanes[x + 5] ^ lanes[x + 10] ^ lanes[x + 15] ^ lanes[x + 20] for x in range(5)]
            for x in range(5):
                delta = columns[(x - 1) % 5] ^ ((columns[(x + 1) % 5] << 1 | columns[(x + 1) % 5] >> 63) & MASK)
                for y in range(5):
                    lanes[x + 5 * y] ^= delta
            moved = [0] * 25
            for x in range(5):
                for y in range(5):
                    value = lanes[x + 5 * y]
                    shift = ROTATIONS[x][y]
                    moved[y + 5 * ((2 * x + 3 * y) % 5)] = ((value << shift) | (value >> (64 - shift))) & MASK if shift else value
            for x in range(5):
                for y in range(5):
                    lanes[x + 5 * y] = moved[x + 5 * y] ^ ((~moved[(x + 1) % 5 + 5 * y]) & moved[(x + 2) % 5 + 5 * y])
            lanes[0] ^= constant
    return b"".join(lane.to_bytes(8, "little") for lane in lanes)[:32]


def decode_ed25519_point(encoded):
    if len(encoded) != 32:
        raise ValueError("invalid service key")
    number = int.from_bytes(encoded, "little")
    y = number & ((1 << 255) - 1)
    if y >= ED25519_P:
        raise ValueError("invalid service key")
    y2 = y * y % ED25519_P
    x2 = (y2 - 1) * pow(ED25519_D * y2 + 1, ED25519_P - 2, ED25519_P) % ED25519_P
    x = pow(x2, (ED25519_P + 3) // 8, ED25519_P)
    if x * x % ED25519_P != x2:
        x = x * ED25519_I % ED25519_P
    if x * x % ED25519_P != x2 or (x == 0 and number >> 255):
        raise ValueError("invalid service key")
    if (x & 1) != number >> 255:
        x = ED25519_P - x
    return x, y, 1, x * y % ED25519_P


def ed25519_add(left, right):
    x1, y1, z1, t1 = left
    x2, y2, z2, t2 = right
    a = (y1 - x1) * (y2 - x2) % ED25519_P
    b = (y1 + x1) * (y2 + x2) % ED25519_P
    c = 2 * ED25519_D * t1 * t2 % ED25519_P
    d = 2 * z1 * z2 % ED25519_P
    e, f, g, h = b - a, d - c, d + c, b + a
    return e * f % ED25519_P, g * h % ED25519_P, f * g % ED25519_P, e * h % ED25519_P


def ed25519_multiply(point, scalar):
    result = (0, 1, 1, 0)
    while scalar:
        if scalar & 1:
            result = ed25519_add(result, point)
        point = ed25519_add(point, point)
        scalar >>= 1
    return result


def ed25519_encode(point):
    x, y, z, _ = point
    inverse = pow(z, ED25519_P - 2, ED25519_P)
    return ((y * inverse % ED25519_P) | ((x * inverse % ED25519_P & 1) << 255)).to_bytes(32, "little")


def check_signature(message, public_key, signature):
    c = int.from_bytes(signature[:32], "little")
    r = int.from_bytes(signature[32:], "little")
    if len(signature) != 64 or c == 0 or c >= ED25519_L or r >= ED25519_L:
        return False
    point = ed25519_add(ed25519_multiply(decode_ed25519_point(public_key), c),
                        ed25519_multiply((*ED25519_BASE, ED25519_BASE[0] * ED25519_BASE[1] % ED25519_P), r))
    commitment = ed25519_encode(point)
    return commitment != b"\x01" + bytes(31) and int.from_bytes(
        keccak256(message + public_key + commitment), "little") % ED25519_L == c


def verify_lock_signatures(lock_blob, quorum_keys, network_id, genesis_hash, parameter_set_hash):
    blob = bytes.fromhex(lock_blob)
    if len(blob) < 55:
        raise ValueError("short ChainLock blob")
    keys = [bytes.fromhex(key) for key in quorum_keys]
    count = len(keys)
    required = threshold(count)
    if any(len(key) != 32 for key in keys):
        raise ValueError("invalid quorum key")
    bitmap = int.from_bytes(blob[48:55], "little")
    if bitmap >> count:
        raise ValueError("ChainLock bitmap exceeds quorum size")
    signers = [index for index in range(count) if bitmap & (1 << index)]
    if len(signers) < required:
        raise ValueError("ChainLock signatures below threshold")
    if len(blob) != 55 + len(signers) * 64:
        raise ValueError("ChainLock signature count does not match bitmap")
    network = bytes.fromhex(network_id)
    genesis = bytes.fromhex(genesis_hash)
    parameters = bytes.fromhex(parameter_set_hash)
    if len(network) != 16 or len(genesis) != 32 or len(parameters) != 32:
        raise ValueError("invalid ChainLock network context")
    message = keccak256(b"QWC_CHAINLOCK_V1" + network + genesis + parameters + blob[40:48] + blob[:8] + blob[8:40])
    for position, index in enumerate(signers):
        if not check_signature(message, keys[index], blob[55 + position * 64:55 + (position + 1) * 64]):
            raise ValueError("invalid ChainLock signature")


def check_locked_header(lock):
    header = bytes.fromhex(lock["block_hashing_blob"])
    miner = bytes.fromhex(lock["miner_tx_blob"])
    position = 0

    def read_varint(data, offset):
        value = 0
        shift = 0
        while offset < len(data) and shift < 70:
            byte = data[offset]
            offset += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value, offset
            shift += 7
        raise ValueError("invalid miner transaction or block header")

    version, position = read_varint(miner, position)
    _, position = read_varint(miner, position)
    inputs, position = read_varint(miner, position)
    if version != 2 or inputs != 1 or miner[position:position + 1] != b"\xff":
        raise ValueError("unsupported miner transaction")
    position += 1
    _, position = read_varint(miner, position)
    outputs, position = read_varint(miner, position)
    if outputs > len(miner) - position:
        raise ValueError("invalid miner transaction")
    for _ in range(outputs):
        _, position = read_varint(miner, position)
        if position >= len(miner):
            raise ValueError("invalid miner transaction")
        target = miner[position]
        position += 1
        if target not in (2, 3):
            raise ValueError("unsupported miner output")
        position += 32 + (target == 3)
    extra_length, position = read_varint(miner, position)
    extra = miner[position:position + extra_length]
    position += extra_length
    if len(extra) != extra_length or miner[position:] != b"\x00":
        raise ValueError("invalid miner transaction")
    root = bytes.fromhex(lock["vm_state_root"])
    roots = []
    extra_position = 0
    while extra_position < len(extra):
        tag, extra_position = read_varint(extra, extra_position)
        if tag == 0:
            if any(extra[extra_position:]):
                raise ValueError("invalid miner transaction extra")
            break
        if tag in (1, 7):
            field = extra[extra_position:extra_position + 32]
            if len(field) != 32:
                raise ValueError("invalid miner transaction extra")
            if tag == 7:
                roots.append(field)
            extra_position += 32
        elif tag == 6:
            _, extra_position = read_varint(extra, extra_position)
            _, extra_position = read_varint(extra, extra_position)
        elif tag in (2, 3, 5, 9, 0xde):
            length, extra_position = read_varint(extra, extra_position)
            extra_position += length
        elif tag == 4:
            count, extra_position = read_varint(extra, extra_position)
            extra_position += count * 32
        else:
            raise ValueError("unsupported miner transaction extra")
        if extra_position > len(extra):
            raise ValueError("invalid miner transaction extra")
    if len(root) != 32 or roots != [root]:
        raise ValueError("VM root differs from miner transaction")
    miner_hash = keccak256(keccak256(miner[:position]) + keccak256(b"\x00") + bytes(32))
    tree_hash = miner_hash
    for sibling_hex in lock["miner_tx_branch"]:
        sibling = bytes.fromhex(sibling_hex)
        if len(sibling) != 32:
            raise ValueError("invalid miner transaction branch")
        tree_hash = keccak256(tree_hash + sibling)

    position = 0
    _, position = read_varint(header, position)
    _, position = read_varint(header, position)
    _, position = read_varint(header, position)
    position += 32 + 4
    if header[position:position + 32] != tree_hash:
        raise ValueError("miner transaction branch differs from block header")
    position += 32
    _, position = read_varint(header, position)
    if position != len(header):
        raise ValueError("invalid block header")
    length = len(header)
    prefix = bytearray()
    while length >= 128:
        prefix.append((length & 127) | 128)
        length >>= 7
    prefix.append(length)
    if keccak256(prefix + header) != bytes.fromhex(lock["block_id"]):
        raise ValueError("block header differs from locked block id")


def derive_quorum(inputs, pinned):
    check_pinned(inputs, pinned)
    epoch = inputs["epoch"]
    if not 0 <= epoch < 1 << 64:
        raise ValueError("epoch outside uint64 range")
    network_id = bytes.fromhex(inputs["network_id"])
    genesis_hash = bytes.fromhex(inputs["genesis_hash"])
    anchor_hash = bytes.fromhex(inputs["snapshot_block_id"])
    snapshot = {bytes.fromhex(key) for key in inputs["snapshot_members"]}
    qualified = {bytes.fromhex(key) for key in inputs["qualified_members"]}
    if len(network_id) != 16 or len(genesis_hash) != 32 or len(anchor_hash) != 32:
        raise ValueError("invalid quorum context length")
    if any(len(key) != 32 for key in snapshot | qualified):
        raise ValueError("invalid service key length")
    if len(snapshot) != inputs["snapshot_total"] or len(qualified) != inputs["qualification_total"]:
        raise ValueError("incomplete quorum input pages")
    prefix = b"QWC_CHAINLOCK_QUORUM_V1" + network_id + genesis_hash + epoch.to_bytes(8, "little") + anchor_hash
    ranked = sorted((keccak256(prefix + key), key) for key in snapshot & qualified)
    if len(ranked) < MIN_NODES:
        raise ValueError("fewer than 10 qualified members: PoW only")
    return [key.hex() for _, key in ranked[:QUORUM_MAX]]


def fetch_quorum_inputs(rpc, daemon_rpc, epoch, pinned):
    offset = 0
    snapshot = []
    qualified = []
    reference = None
    while True:
        page = rpc(daemon_rpc, "get_chainlock_quorum_inputs", {"epoch": epoch, "offset": offset, "limit": 256})
        check_pinned(page, pinned)
        if page["offset"] != offset or page["epoch"] != epoch:
            raise ValueError("quorum input page mismatch")
        metadata = {key: value for key, value in page.items() if key not in ("snapshot_members", "qualified_members", "offset")}
        if reference is None:
            reference = metadata
        elif metadata != reference:
            raise ValueError("quorum inputs changed between pages")
        if len(page["snapshot_members"]) > 256 or len(page["qualified_members"]) > 256:
            raise ValueError("quorum input page exceeds limit")
        snapshot.extend(page["snapshot_members"])
        qualified.extend(page["qualified_members"])
        if len(snapshot) > page["snapshot_total"] or len(qualified) > page["qualification_total"]:
            raise ValueError("quorum input page exceeds reported total")
        if len(snapshot) >= page["snapshot_total"] and len(qualified) >= page["qualification_total"]:
            break
        if not page["snapshot_members"] and not page["qualified_members"]:
            raise ValueError("quorum input pagination made no progress")
        offset += 256
    result = dict(reference)
    result["offset"] = 0
    result["snapshot_members"] = snapshot
    result["qualified_members"] = qualified
    return result
