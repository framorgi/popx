"""Failure ordering: a failed restore must never invoke historical deletion."""
import asyncio
from pathlib import Path
import sqlite3
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock
from dashboard.checkpoints import Checkpoints
from dashboard.server import Store


class CheckpointFailureTests(unittest.IsolatedAsyncioTestCase):
    async def test_retry_reuses_paused_restored_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            store = SimpleNamespace(path=Path(tmp) / 'history.sqlite')
            receiver = SimpleNamespace(run='restored', status={'state': 'paused'}, command=AsyncMock())
            guard = SimpleNamespace(ensure_capacity=lambda: None)
            app = SimpleNamespace(state=SimpleNamespace(store=store, receiver=receiver, storage_guard=guard))
            service = Checkpoints(app, None)
            service.save = AsyncMock(return_value='b' * 32)
            service.load = AsyncMock(return_value='restored')
            service.reset_history = AsyncMock(side_effect=[RuntimeError('reader'), None])
            with self.assertRaisesRegex(RuntimeError, 'reader'):
                await service.rotate()
            await service.rotate()
            self.assertEqual(service.load.await_count, 1)
            self.assertEqual(service.save.await_count, 1)
            self.assertEqual(service.reset_history.await_count, 2)
            self.assertIsNone(service.recovery)
            service.close()

    async def test_open_reader_retains_history_until_safe_rotation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'history.sqlite'
            store = Store(path)
            with store.db:
                store.db.execute("INSERT INTO runs VALUES('old',0,'{}','finished','',NULL)")
                store.db.execute("INSERT INTO runs VALUES('new',1,'{}','running','',NULL)")
            reader = sqlite3.connect(path)
            reader.execute("INSERT INTO diagnostics(run,data) VALUES('old',?)", ('x' * 10000,))
            reader.commit()

            class Evolution:
                def close(self): pass
                def reopen(self): pass

            receiver = SimpleNamespace(parts={}, partial_bytes=0)
            app = SimpleNamespace(state=SimpleNamespace(store=store, receiver=receiver,
                                  removals=SimpleNamespace(close=AsyncMock())))
            service = Checkpoints(app, Evolution())
            try:
                with self.assertRaisesRegex(RuntimeError, 'Lettori SQLite'):
                    await service.reset_history('new')
                self.assertEqual(store.db.execute('SELECT count(*) FROM runs').fetchone()[0], 2)
                self.assertFalse(any(Path(tmp).glob('history.sqlite.rotation-new*')))
                reader.close()
                await service.reset_history('new')
                self.assertEqual(store.db.execute('SELECT id FROM runs').fetchall(), [('new',)])
                self.assertFalse(any(Path(tmp).glob('history.sqlite.rotation-new*')))
            finally:
                reader.close()
                service.close()
                store.close()

    async def test_failed_restore_preserves_history_and_recovery_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            store=SimpleNamespace(path=Path(tmp)/'history.sqlite')
            db=sqlite3.connect(store.path);db.execute('create table retained(value text)');db.execute("insert into retained values('old history')");db.commit();db.close()
            receiver=SimpleNamespace(status={'state':'paused'},command=AsyncMock())
            app=SimpleNamespace(state=SimpleNamespace(store=store,receiver=receiver))
            service=Checkpoints(app,None)
            service.save=AsyncMock(return_value='a'*32)
            service.load=AsyncMock(side_effect=RuntimeError('invalid checkpoint'))
            service.reset_history=AsyncMock()
            with self.assertRaisesRegex(RuntimeError,'invalid checkpoint'):await service.rotate()
            service.reset_history.assert_not_called();receiver.command.assert_not_called()
            service.close()
            with sqlite3.connect(store.path) as check:self.assertEqual(check.execute('select value from retained').fetchone()[0],'old history')
            reopened=Checkpoints(app,None)
            self.assertEqual(reopened.recovery,('a'*32,));reopened.close()

    async def test_failed_save_pauses_without_deleting(self):
        with tempfile.TemporaryDirectory() as tmp:
            receiver=SimpleNamespace(status={'state':'running'},command=AsyncMock())
            app=SimpleNamespace(state=SimpleNamespace(store=SimpleNamespace(path=Path(tmp)/'history.sqlite'),receiver=receiver))
            service=Checkpoints(app,None)
            service.save=AsyncMock(side_effect=OSError('no space'))
            service.load=AsyncMock();service.reset_history=AsyncMock()
            with self.assertRaises(OSError):await service.rotate()
            service.load.assert_not_called();service.reset_history.assert_not_called()
            receiver.command.assert_awaited_once_with('pause',{})
            service.close()


if __name__=='__main__':unittest.main()
