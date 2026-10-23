"""Failure ordering: a failed restore must never invoke historical deletion."""
import asyncio
from pathlib import Path
import sqlite3
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock
from dashboard.checkpoints import Checkpoints


class CheckpointFailureTests(unittest.IsolatedAsyncioTestCase):
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
