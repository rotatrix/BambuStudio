"""Exercise the macOS transport contract without a compiler or signing identity."""
import hashlib
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[2] / '.github/scripts/openaxis-package.py'


class MacPackage(unittest.TestCase):
    def test_app_only_transport_and_extracted_smoke(self):
        commit = 'a' * 40
        run = subprocess.run
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'LICENSE').write_text('Application license')
            build = root / 'build/arm64'
            app = build / 'BambuStudio/BambuStudio.app'
            resources = app / 'Contents/Resources'
            resources.mkdir(parents=True)
            binary = app / 'Contents/MacOS/BambuStudio'
            binary.parent.mkdir()
            binary.write_text('#!/bin/sh\n[ "$1" = --help ]\n')
            binary.chmod(0o755)
            (resources / 'link').symlink_to('OpenAxis-LICENSE.txt')
            sdk = build / '_deps/openaxis-src'
            (sdk / 'cpp/third_party/ixwebsocket').mkdir(parents=True)
            (sdk / 'LICENSE').write_text('SDK license')
            (sdk / 'cpp/third_party/ixwebsocket/LICENSE.txt').write_text('WebSocket license')
            (build / 'CMakeCache.txt').write_text('SLIC3R_OPENAXIS:BOOL=ON\n')
            signed = []
            smoked = []

            def execute(command, **kwargs):
                if command[0] == 'cmake':
                    # Real Mach-O bundling is exercised on the compiled app.
                    return subprocess.CompletedProcess(command, 0)
                if command[0] == 'codesign':
                    bundle = Path(command[-1])
                    self.assertTrue((bundle / 'Contents/Resources/BambuStudio-LICENSE.txt').is_file())
                    self.assertTrue((bundle / 'Contents/Resources/OpenAxis-LICENSE.txt').is_file())
                    self.assertTrue((bundle / 'Contents/Resources/OpenAxis-IXWebSocket-LICENSE.txt').is_file())
                    signed.append(command)
                    return subprocess.CompletedProcess(command, 0)
                if command[-1] == '--help':
                    self.assertNotEqual(Path(command[0]), binary)
                    smoked.append(command)
                return run(command, **kwargs)

            old = Path.cwd()
            try:
                os.chdir(root)
                with mock.patch.object(sys, 'argv', [str(SCRIPT), 'macos-arm64']), \
                     mock.patch.dict(os.environ, {'GITHUB_SHA': commit}), \
                     mock.patch('subprocess.check_output', return_value=commit + '\n'), \
                     mock.patch('subprocess.run', side_effect=execute):
                    runpy.run_path(str(SCRIPT), run_name='__main__')
            finally:
                os.chdir(old)
            self.assertEqual(len(signed), 3)
            self.assertEqual(len(smoked), 1)
            package = root / f'dist/macos-app-arm64-{commit}.tar.gz'
            with tarfile.open(package) as archive:
                members = archive.getmembers()
                self.assertEqual({m.name.split('/')[0] for m in members}, {'BambuStudio.app'})
                self.assertTrue(archive.getmember('BambuStudio.app/Contents/Resources/link').issym())
                self.assertTrue(archive.getmember('BambuStudio.app/Contents/MacOS/BambuStudio').mode & 0o111)
            digest = hashlib.sha256(package.read_bytes()).hexdigest()
            self.assertEqual(package.with_name(package.name + '.sha256').read_text().split(),
                             [digest, package.name])


if __name__ == '__main__':
    unittest.main()
