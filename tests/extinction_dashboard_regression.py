"""Exact extinction ticks remain available despite chart downsampling."""
import asyncio
from pathlib import Path
import sqlite3
import tempfile
import unittest

from dashboard.server import ExtinctionIndex


class ExtinctionDashboardRegression(unittest.IsolatedAsyncioTestCase):
    async def test_recovery_terminal_and_incremental_events(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'events.sqlite'
            with sqlite3.connect(path) as db:
                db.executescript('''
                    CREATE TABLE events(id INTEGER PRIMARY KEY, run TEXT, tick INTEGER, type TEXT, data TEXT);
                    CREATE TABLE runs(id TEXT PRIMARY KEY, reason TEXT);
                    CREATE TABLE samples(run TEXT, tick INTEGER);
                    INSERT INTO runs VALUES('r', '');
                    INSERT INTO events(run,tick,type,data) VALUES('r',101,'death','{}');
                    INSERT INTO events(run,tick,type,data) VALUES('r',103,'birth','{"recovery_seed":true}');
                    INSERT INTO events(run,tick,type,data) VALUES('r',103,'birth','{"recovery_seed":true}');
                    INSERT INTO events(run,tick,type,data) VALUES('other',120,'birth','{"recovery_seed":true}');
                ''')
            index = ExtinctionIndex(path)
            self.assertEqual(await index.ticks('r', 0, 200), [103])
            self.assertEqual(await index.ticks('r', 104, 200), [])
            with sqlite3.connect(path) as db:
                db.execute("INSERT INTO events(run,tick,type,data) VALUES('r',211,'birth',?)",
                           ('{"recovery_seed":true}',))
                db.execute("INSERT INTO samples VALUES('r', 300)")
                db.execute("UPDATE runs SET reason='extinction' WHERE id='r'")
            self.assertEqual(await index.ticks('r', 0, 300), [103, 211, 300])
            self.assertEqual(await index.ticks('r', 200, 220), [211])


if __name__ == '__main__':
    unittest.main()
