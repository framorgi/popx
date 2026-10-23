"""Threshold behaviour without allocating a large database; legacy stop fallback; current checkpoint rotation tested separately."""
import asyncio
from pathlib import Path
import tempfile
import unittest
import json
import os
import socket
import sqlite3
import subprocess
import time
from types import SimpleNamespace
from unittest.mock import patch
from fastapi import HTTPException
from dashboard.storage import StorageGuard
from dashboard.server import ROOT, create_app
from fastapi.testclient import TestClient


class Receiver:
    def __init__(self):
        self.run='run';self.status={'state':'running','metadata':{'config':{'database_limit_gb':50}}}
        self.commands=[];self.fail=False
    async def command(self,name,args,run):
        self.commands.append((name,args,run))
        if self.fail:raise RuntimeError('No acknowledgment')
        return {'ok':True}


class LimitTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp=tempfile.TemporaryDirectory();self.path=Path(self.temp.name)/'config.ini'
        self.path.write_text('database_limit_gb = 75 # GB\n');self.receiver=Receiver()
        self.guard=StorageGuard(SimpleNamespace(path=Path(self.temp.name)/'db'),self.receiver,self.path)
    async def asyncTearDown(self):self.temp.cleanup()

    async def test_total_threshold_config_and_once_per_run(self):
        with patch('dashboard.storage.database_files',return_value={'database':49_000_000_000,'journal':999_999_999,'shared_index':0}):
            await self.guard.check();self.assertEqual(self.receiver.commands,[])
        with patch('dashboard.storage.database_files',return_value={'database':49_000_000_000,'journal':1_000_000_000,'shared_index':0}):
            await self.guard.check();await self.guard.check()
            self.assertEqual(self.receiver.commands,[('stop',{'reason':'storage_limit'},'run')])
            with self.assertRaises(HTTPException):self.guard.ensure_capacity()
            self.guard.ensure_capacity(configured=True) # Next run uses updated 75 GB from the INI.
            self.receiver.run='new';await self.guard.check();self.assertEqual(len(self.receiver.commands),2)
        self.assertEqual(self.guard.state['limit_gb'],50)

    async def test_retry_and_measurement_failure(self):
        with patch('dashboard.storage.database_files',return_value={'database':50_000_000_000}):
            self.receiver.fail=True;await self.guard.check();self.assertTrue(self.guard.state['error'])
            self.receiver.fail=False;self.guard.retry_at=0;await self.guard.check();self.assertTrue(self.guard.state['stop_requested'])
        self.receiver.run='unmeasured'
        with patch('dashboard.storage.database_files',side_effect=OSError('unreadable')):
            await self.guard.check();self.assertEqual(len(self.receiver.commands),2)
            self.assertIn('unreadable',self.guard.state['error'])

    async def test_invalid_config_never_disables_limit_silently(self):
        self.receiver.status['metadata']['config']={}
        for value in ['0','-1','nan','inf','bad']:
            self.path.write_text('database_limit_gb='+value)
            await self.guard.check();self.assertTrue(self.guard.state['error'])
            with self.assertRaises(HTTPException):self.guard.ensure_capacity(configured=True)
        self.assertEqual(self.receiver.commands,[])


# Real rotation/save/load coverage lives in checkpoint_integration.py.

if __name__ == '__main__': unittest.main()
