"""Lossless SQLite genome encoding and legacy-event compatibility."""

import json
from pathlib import Path
import tempfile
import unittest

from dashboard.genome_codec import decode_genes, encode_genes, pack_event, unpack_event
from dashboard.server import Store


class GenomeCodecRegression(unittest.TestCase):
    def test_roundtrip_full_gene_domain_and_corruption(self):
        genes = [[0, 0, 7, 21, -32768], [6, 0xffffffff, 7, 0xffffffff, 32767],
                 [2, 128, 5, 16384, 0]]
        self.assertEqual(decode_genes(encode_genes(genes)), genes)
        self.assertEqual(decode_genes(encode_genes([])), [])
        for broken in ('v2:AA==', 'v1:!', 'v1:AQ==', encode_genes(genes) + 'AA'):
            with self.assertRaises(ValueError):
                decode_genes(broken)
        with self.assertRaises(ValueError):
            encode_genes([[0, 0x100000000, 7, 0, 1]])

    def test_store_preserves_public_events_and_legacy_rows(self):
        genes = [[0, i, 7, i % 22, i - 50] for i in range(100)]
        agent = {'pop_id': 'p', 'genes': genes, 'age': 0}
        with tempfile.TemporaryDirectory() as tmp:
            store = Store(Path(tmp) / 'events.sqlite')
            try:
                message = {'run_id': 'r', 'tick': 1, 'type': 'event',
                           'data': {'event_type': 'birth', 'agent': agent}}
                self.assertTrue(store.ingest('process', 1, message))
                raw = store.db.execute('SELECT data FROM events WHERE run=?', ('r',)).fetchone()[0]
                stored = json.loads(raw)
                self.assertNotIn('genes', stored)
                self.assertLess(len(raw), len(json.dumps(agent, separators=(',', ':'))))
                self.assertEqual(unpack_event(stored), agent)
                self.assertEqual(agent['genes'], genes)
                self.assertIs(unpack_event(agent), agent)
            finally:
                store.close()


if __name__ == '__main__':
    unittest.main()
