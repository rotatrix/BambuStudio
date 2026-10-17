"""Exercise the deployment gate with actual Mach-O load commands."""
from pathlib import Path
import platform
import plistlib
import subprocess
import tempfile
import unittest

AUDIT = Path(__file__).resolve().parents[2] / '.github/scripts/macos-audit.cmake'


@unittest.skipUnless(platform.system() == 'Darwin', 'requires Apple tools')
class MacDeployment(unittest.TestCase):
    def test_rejects_a_newer_bundled_library(self):
        arch = platform.machine()
        target = '11.3' if arch == 'arm64' else '10.15'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app = root / 'Fixture.app'
            binary = app / 'Contents/MacOS/Fixture'
            library = app / 'Contents/Frameworks/libfixture.dylib'
            binary.parent.mkdir(parents=True)
            library.parent.mkdir()
            (app / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'Fixture'}))
            source = root / 'fixture.c'
            source.write_text('int main(void) { return 0; }\n')
            subprocess.run(['clang', f'-mmacosx-version-min={target}', str(source), '-o', str(binary)], check=True)
            audit = ['cmake', f'-DAPP={app}', f'-DARCH={arch}', f'-DMIN_MACOS={target}', '-P', str(AUDIT)]
            for minimum, success in [(target, True), ('15.0', False)]:
                subprocess.run(['clang', '-dynamiclib', f'-mmacosx-version-min={minimum}',
                                str(source), '-o', str(library)], check=True)
                result = subprocess.run(audit, capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
                if not success:
                    self.assertIn('requires macOS 15.0', result.stderr)


if __name__ == '__main__':
    unittest.main()
