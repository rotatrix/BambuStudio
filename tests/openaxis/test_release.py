"""Release guard checks; never call GitHub or create a release."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('release', Path(__file__).resolve().parents[2] / '.github/scripts/openaxis-release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseGuards(unittest.TestCase):
    def test_tags(self):
        self.assertEqual(release.release_tag('v02.08.02.61-rotatrix.1', 'tag'), ('v02.08.02.61', False))
        self.assertEqual(release.release_tag('v02.08.02.61-rotatrix.2-beta.1', 'tag'), ('v02.08.02.61', True))
        for tag, kind in [('rotatrix/work/v02.08.02.61', 'branch'), ('openaxis-preview-abc', 'tag'),
                          ('v02.08.02.61-rotatrix.0', 'tag'), ('v02.08.02.61-rotatrix.1', 'branch'),
                          ('v02.09.00.00-rotatrix.1', 'tag')]:
            with self.subTest(tag=tag, kind=kind), self.assertRaises(ValueError):
                release.release_tag(tag, kind)

    def test_assets_and_provenance(self):
        commit, sdk, run = 'a' * 40, 'b' * 40, '123'
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            for platform in ['windows-x64', 'macos-arm64', 'macos-x86_64', 'macos-arm64-macos26',
                             'linux-x64', 'linux-x64-ubuntu22', 'linux-x64-ubuntu26']:
                ext = 'AppImage' if platform.startswith('linux') else 'zip'
                p = folder / f'BambuStudio-Rotatrix-{platform}-{commit[:12]}.{ext}'
                p.write_bytes(b'package fixture')
                digest = hashlib.sha256(p.read_bytes()).hexdigest()
                p.with_name(p.name + '.sha256').write_text(f'{digest}  {p.name}\n')
                p.with_name(p.name + '.json').write_text(json.dumps(dict(
                    commit=commit, openaxis_commit=sdk, run_id=run, platform=platform, file=p.name, sha256=digest)))
            self.assertEqual(len(release.validate_assets(folder, commit, run)), 21)
            with self.assertRaises(ValueError):
                release.validate_assets(folder, commit, 'other-run')
            p.write_bytes(b'tampered')
            with self.assertRaises(ValueError):
                release.validate_assets(folder, commit, run)
            p.unlink()
            with self.assertRaises(ValueError):
                release.validate_assets(folder, commit, run)


if __name__ == '__main__':
    unittest.main()
