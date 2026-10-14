"""Read current SQLite or preserved historical JSON without creating data files."""
import json
import sqlite3
from pathlib import Path

def read_report(path, kind=None):
    # Historical input is accepted without modifying or converting existing artifacts.
    if str(path).endswith('.json'):
        return json.loads(Path(path).read_text())
    with sqlite3.connect(f'file:{Path(path).resolve()}?mode=ro',uri=True) as db:
        if kind:
            row=db.execute('SELECT d.data FROM diagnostics d JOIN runs r ON r.id=d.run WHERE json_extract(r.metadata,\'$.kind\')=? ORDER BY d.id DESC LIMIT 1',(kind,)).fetchone()
        else:
            row=db.execute('SELECT data FROM diagnostics ORDER BY id DESC LIMIT 1').fetchone()
    if not row:
        raise RuntimeError('No diagnostic report in '+str(path))
    return json.loads(row[0])
