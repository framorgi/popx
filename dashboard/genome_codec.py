"""Lossless, versioned storage encoding for birth-genome event payloads.

The simulator protocol and public API keep JSON gene arrays. Only SQLite event
rows use this compact representation; old rows remain readable unchanged.
"""

import base64
import binascii
import struct


_PREFIX = 'v1:'
_MAX_GENES = 100_000


def _put_uint(value, output):
    if not isinstance(value, int) or not 0 <= value <= 0xffffffff:
        raise ValueError('Indice genetico fuori da uint32')
    while value >= 128:
        output.append((value & 127) | 128)
        value >>= 7
    output.append(value)


def _get_uint(data, offset):
    value = 0
    for shift in range(0, 35, 7):
        if offset >= len(data):
            raise ValueError('Genoma codificato troncato')
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            if value > 0xffffffff:
                raise ValueError('Indice genetico fuori da uint32')
            return value, offset
    raise ValueError('Varint genetico troppo lungo')


def encode_genes(genes):
    if len(genes) > _MAX_GENES:
        raise ValueError('Genoma troppo lungo')
    output = bytearray()
    _put_uint(len(genes), output)
    for gene in genes:
        if len(gene) != 5:
            raise ValueError('Gene non valido')
        source_type, source_index, sink_type, sink_index, weight = gene
        if not all(isinstance(value, int) for value in gene) or not 0 <= source_type <= 7 or not 0 <= sink_type <= 7:
            raise ValueError('Tipo genetico non valido')
        if not -32768 <= weight <= 32767:
            raise ValueError('Peso genetico fuori da int16')
        output.append((source_type << 3) | sink_type)
        _put_uint(source_index, output)
        _put_uint(sink_index, output)
        output.extend(struct.pack('<h', weight))
    return _PREFIX + base64.b64encode(output).decode('ascii')


def decode_genes(encoded):
    if not isinstance(encoded, str) or not encoded.startswith(_PREFIX):
        raise ValueError('Versione genoma non supportata')
    try:
        data = base64.b64decode(encoded[len(_PREFIX):], validate=True)
    except binascii.Error as error:
        raise ValueError('Genoma codificato corrotto') from error
    count, offset = _get_uint(data, 0)
    if count > _MAX_GENES:
        raise ValueError('Genoma troppo lungo')
    genes = []
    for _ in range(count):
        if offset >= len(data):
            raise ValueError('Genoma codificato troncato')
        types = data[offset]
        offset += 1
        source_index, offset = _get_uint(data, offset)
        sink_index, offset = _get_uint(data, offset)
        if offset + 2 > len(data):
            raise ValueError('Genoma codificato troncato')
        weight = struct.unpack_from('<h', data, offset)[0]
        offset += 2
        genes.append([types >> 3, source_index, types & 7, sink_index, weight])
    if offset != len(data):
        raise ValueError('Byte aggiuntivi nel genoma codificato')
    return genes


def pack_event(agent):
    if not isinstance(agent.get('genes'), list):
        return agent
    packed = dict(agent)
    packed['genes_packed'] = encode_genes(packed.pop('genes'))
    return packed


def unpack_event(agent):
    if 'genes_packed' not in agent:
        return agent
    unpacked = dict(agent)
    unpacked['genes'] = decode_genes(unpacked.pop('genes_packed'))
    return unpacked
