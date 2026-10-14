"""Read-only provenance shared by production sessions and diagnostic fixtures."""
import hashlib
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parent.parent

def collect(binary=None):
    def output(command):
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=10)
        return result.stdout.strip() if result.returncode==0 else 'unavailable'
    untracked = output(['git','ls-files','--others','--exclude-standard','--','src','tools','scripts','dashboard','config','tests']).splitlines()
    return {'binary_sha256':hashlib.sha256(Path(binary).read_bytes()).hexdigest() if binary and Path(binary).is_file() else None,
            'untracked_sources':{name:(ROOT/name).read_text() for name in untracked if (ROOT/name).is_file() and Path(name).suffix in ('.py','.cpp','.h','.sh','.html','.css','.js','.txt','.ini')},
            'commit':output(['git','rev-parse','HEAD']),
            'diff':output(['git','diff','--','src','tools','scripts','dashboard','config','tests']),
            'compiler':output(['clang++','--version']),
            'constants':(ROOT/'src/common.h').read_text(),
            'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                             for folder in ('src','tools','scripts','dashboard','tests')
                             for p in (ROOT/folder).glob('*.*') if p.is_file()},
            'rng':'random_device and rand are not globally seeded; fixture replay is not global reproducibility'}
