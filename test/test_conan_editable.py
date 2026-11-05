"""Check real editable metadata using cached dependencies, without building.

Run explicitly with ``python3 test/test_conan_editable.py -q`` after registering
this checkout as krisp/0.1.0. This check never changes the editable mapping.
"""

import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def conan(*args):
    result = subprocess.run(
        ['conan', *args], cwd=ROOT, capture_output=True, text=True,
    )
    if result.returncode:
        raise AssertionError(result.stderr)
    return json.loads(result.stdout)


class EditableMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        entries = conan('editable', 'list', '--format=json')
        entry = entries.get('krisp/0.1.0')
        if not entry or Path(entry['path']).resolve().parent != ROOT:
            raise unittest.SkipTest('Register this checkout as krisp/0.1.0 first')

    def test_debug_and_release_use_matching_checkout_paths(self):
        for mode, profile in (
            ('Debug', 'conan_clang_profile'),
            ('Release', 'conan_clang_release_profile'),
        ):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as output:
                graph = conan(
                    'install', '--requires=krisp/0.1.0', '-g', 'PkgConfigDeps',
                    '-pr:h', str(ROOT / profile), '-of', output,
                    '--build=never', '--no-remote', '--format=json',
                )['graph']['nodes'].values()
                engines = [n for n in graph if (n.get('ref') or '').startswith('krisp/0.1.0')]
                self.assertEqual(len(engines), 1)
                self.assertEqual(engines[0]['binary'], 'Editable')
                self.assertEqual(engines[0]['settings']['build_type'], mode)
                dependencies = [n for n in graph if n.get('context') == 'host'
                                and n.get('ref') and n not in engines]
                self.assertTrue(dependencies)
                for node in dependencies:
                    # Header-only dependencies may omit build_type.
                    build_type = node['settings'].get('build_type')
                    if build_type is not None:
                        self.assertEqual(build_type, 'Release', node['ref'])

                pc = (Path(output) / 'krisp.pc').read_text()
                variables = dict(re.findall(r'^([\w]+)=(.*)$', pc, re.MULTILINE))

                def resolve(value):
                    for _ in range(len(variables)):
                        value = re.sub(r'\$\{(\w+)\}', lambda m: variables[m[1]], value)
                    return value

                build = ROOT / 'build' / mode.lower()
                expected = {
                    'krisp_resources': ROOT / 'resources/default',
                    'krisp_default_config': ROOT / 'configs/default.yaml',
                    'krisp_shaders': build / 'shaders',
                    'krisp_environment_lighting': build / 'tools/default_environment.krisp-ibl',
                }
                for key, path in expected.items():
                    self.assertEqual(Path(resolve(variables[key])), path)
                for key, path in (('libdir', build / 'src'), ('libdir1', build / 'third_party')):
                    self.assertEqual(Path(resolve(variables[key])), path)
                self.assertEqual('-D_DEBUG' in pc, mode == 'Debug')


if __name__ == '__main__':
    unittest.main()
