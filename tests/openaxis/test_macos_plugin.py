"""Native Security.framework verification; optional authentic installed fixtures.

Set BAMBU_TEST_PLUGIN_DIR to a directory containing the upstream signed dylibs.
Fixtures are only read/copied, never loaded or modified in place.
"""
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(platform.system() == 'Darwin', 'requires Apple Security.framework')
class PluginSignature(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        main = cls.root / 'verify.cpp'
        main.write_text('''#include "CertificateVerify.hpp"
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    return Slic3r::VerifyBambuModule(argv[1], std::string(argv[2]) == "network"
        ? Slic3r::BambuModule::Networking : Slic3r::BambuModule::Source) ? 0 : 1;
}
''')
        cls.checker = cls.root / 'verify'
        subprocess.run(['clang++', '-std=c++17', '-I', str(ROOT / 'src/slic3r/Utils'),
                        str(main), str(ROOT / 'src/slic3r/Utils/MacPluginVerify.cpp'),
                        '-framework', 'Security', '-framework', 'CoreFoundation',
                        '-o', str(cls.checker)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def verify(self, path, module, accepted):
        result = subprocess.run([str(self.checker), str(path), module])
        self.assertEqual(result.returncode, 0 if accepted else 1)

    def test_missing_unsigned_and_wrong_publisher(self):
        self.verify(self.root / 'missing.dylib', 'network', False)
        source = self.root / 'fixture.c'
        source.write_text('int fixture(void) { return 42; }\n')
        library = self.root / 'fixture.dylib'
        subprocess.run(['clang', '-dynamiclib', str(source), '-o', str(library)], check=True)
        subprocess.run(['codesign', '--remove-signature', str(library)], check=True)
        self.verify(library, 'network', False)
        subprocess.run(['codesign', '--force', '--sign', '-', '--identifier',
                        'libbambu_networking', str(library)], check=True)
        self.verify(library, 'network', False)

    @unittest.skipUnless(os.environ.get('BAMBU_TEST_PLUGIN_DIR'), 'authentic plugin fixtures not supplied')
    def test_authentic_wrong_identifier_and_tampered(self):
        for filename, module, wrong in [('libbambu_networking.dylib', 'network', 'source'),
                                        ('libBambuSource.dylib', 'source', 'network')]:
            original = Path(os.environ['BAMBU_TEST_PLUGIN_DIR']) / filename
            self.verify(original, module, True)
            self.verify(original, wrong, False)
            altered = self.root / filename
            shutil.copy2(original, altered)
            with altered.open('r+b') as handle:
                handle.seek(16384)
                value = handle.read(1)
                self.assertTrue(value)
                handle.seek(16384)
                handle.write(bytes([value[0] ^ 1]))
            self.verify(altered, module, False)


if __name__ == '__main__':
    unittest.main()
