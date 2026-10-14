"""Install versioned frontend assets locally; called only by --setup."""
from pathlib import Path
import hashlib
from urllib.request import urlopen

root = Path(__file__).resolve().parent.parent / 'dashboard/static/vendor'
assets = {
    'chart.umd.js': 'https://cdn.jsdelivr.net/npm/chart.js@4.4.9/dist/chart.umd.js',
    'chart.LICENSE': 'https://cdn.jsdelivr.net/npm/chart.js@4.4.9/LICENSE.md',
    'cytoscape.min.js': 'https://cdn.jsdelivr.net/npm/cytoscape@3.31.2/dist/cytoscape.min.js',
    'cytoscape.LICENSE': 'https://cdn.jsdelivr.net/npm/cytoscape@3.31.2/LICENSE',
}
checksums = {'chart.umd.js': 'de315773454b6076b63990bfa05ce5155a37f71992c87d7bb5f9a2ea698697ef', 'chart.LICENSE': '41a84aa2caba645f966a18d9c2056b73e6d3a81d80bc0046bc0011a2634d4cce', 'cytoscape.min.js': 'a9e75b12b6dab7b7f5b428636070b38c59eba2a4a7d0a3af36b2afdd67b13499', 'cytoscape.LICENSE': 'e4b62495bacaefaf330f38f226e20d1d60d658d444998ba7406a191f96a2e701'}
root.mkdir(parents=True, exist_ok=True)
for name, url in assets.items():
    with urlopen(url, timeout=30) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != checksums[name]:
        raise RuntimeError('Asset checksum mismatch: '+name)
    if not data:
        raise RuntimeError('Empty asset: '+name)
    target = root/name
    temporary = target.with_suffix('.tmp')
    temporary.write_bytes(data)
    temporary.replace(target)
    print('Installed', name)
