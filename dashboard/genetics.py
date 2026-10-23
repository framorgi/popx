"""Versioned, order-independent birth-genome comparison shared with Genome in C++."""
import math

MODEL = 'endpoint_weight_v1'


def canonical(genes):
    return tuple(sorted(tuple(int(v) for v in gene) for gene in genes))


def similarity(a, b):
    """Pair sorted weights at equal endpoints; unmatched alleles contribute zero."""
    if not a and not b:
        return 1.0
    i = j = 0
    score = 0.0
    while i < len(a) and j < len(b):
        left, right = a[i][:4], b[j][:4]
        if left == right:
            score += 1 - abs(a[i][4] - b[j][4]) / 65535
            i += 1
            j += 1
        elif left < right:
            i += 1
        else:
            j += 1
    return score / max(len(a), len(b))


def _hash(values, bits=32):
    value = 2166136261 if bits == 32 else 14695981039346656037
    prime = 16777619 if bits == 32 else 1099511628211
    mask = (1 << bits) - 1
    for number in values:
        for shift in (0, 8, 16, 24):
            value = ((value ^ ((number >> shift) & 255)) * prime) & mask
    return value


def fingerprint(genes):
    return f'{_hash([len(genes), *(v for gene in genes for v in gene)], 64):016x}'


def color(genes):
    """Smooth three-channel projection; similarity is measured from genes, not RGB."""
    if not genes:
        return [0, 0, 0]
    sums = [0, 0, 0]
    for gene in genes:
        h = _hash(gene[:4])
        for channel in range(3):
            sums[channel] += (1 if h & (1 << channel) else -1) * 98304
            sums[channel] += (1 if h & (1 << (channel + 8)) else -1) * gene[4]
    scale = 131072 * math.sqrt(len(genes))
    return [math.floor(144 + 80 * max(-1, min(1, value / scale)) + .5) for value in sums]
