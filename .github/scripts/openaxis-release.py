"""Stage an immutable version-tag release from verified artifacts; never release branch builds."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def validate_assets(directory, commit, run_id):
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Invalid source commit')
    assets = []
    sdk_commits = set()
    for platform, extension in [('windows-x64', 'zip'), ('macos-arm64', 'zip'), ('macos-x86_64', 'zip'), ('macos-arm64-macos26', 'zip'), ('linux-x64', 'AppImage'), ('linux-x64-ubuntu22', 'AppImage'), ('linux-x64-ubuntu26', 'AppImage')]:
        package = directory / f'BambuStudio-Rotatrix-{platform}-{commit[:12]}.{extension}'
        checksum = package.with_name(package.name + '.sha256')
        manifest = package.with_name(package.name + '.json')
        if not package.is_file() or package.stat().st_size == 0:
            raise ValueError(f'Missing package: {package}')
        with package.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if checksum.read_text().split() != [digest, package.name]:
            raise ValueError(f'Checksum mismatch: {package}')
        data = json.loads(manifest.read_text())
        expected = dict(commit=commit, run_id=run_id, platform=platform, file=package.name, sha256=digest)
        if any(data.get(k) != v for k, v in expected.items()):
            raise ValueError(f'Provenance mismatch: {package}')
        sdk = data.get('openaxis_commit', '')
        if not re.fullmatch(r'[0-9a-f]{40}', sdk):
            raise ValueError('Invalid SDK revision')
        sdk_commits.add(sdk)
        assets.extend([str(package), str(checksum), str(manifest)])
    if len(sdk_commits) != 1:
        raise ValueError('Platforms used different SDK revisions')
    return assets


def release_tag(tag, ref_type):
    match = re.fullmatch(r'(v02\.08\.02\.61)-rotatrix\.([1-9][0-9]*)(-beta\.[1-9][0-9]*)?', tag)
    if ref_type != 'tag' or not match:
        raise ValueError('Expected <upstream-tag>-rotatrix.N or <upstream-tag>-rotatrix.N-beta.N')
    return match.group(1), bool(match.group(3))


def main():
    commit, run_id, repo = (os.environ[k] for k in ['GITHUB_SHA', 'GITHUB_RUN_ID', 'GH_REPO'])
    tag = os.environ['GITHUB_REF_NAME']
    upstream, prerelease = release_tag(tag, os.environ['GITHUB_REF_TYPE'])
    resolved = subprocess.check_output(['git', 'rev-parse', f'{tag}^{{commit}}'], text=True).strip()
    if resolved != commit:
        raise ValueError('Release tag does not identify the built commit')
    subprocess.run(['git', 'merge-base', '--is-ancestor', upstream, commit], check=True)
    if not prerelease:
        # Final versions must come from a maintained branch, never disposable work.
        subprocess.run(['git', 'fetch', 'origin', f'refs/heads/rotatrix/{upstream}'], check=True)
        subprocess.run(['git', 'merge-base', '--is-ancestor', commit, 'FETCH_HEAD'], check=True)
    assets = validate_assets(Path(sys.argv[1]), commit, run_id)
    pages = json.loads(subprocess.check_output(
        ['gh', 'api', '--paginate', '--slurp', f'repos/{repo}/releases'], text=True))
    if any(release['tag_name'] == tag for page in pages for release in page):
        raise ValueError('Release already exists; immutable releases must not be overwritten')
    notes = f"""BambuStudio {upstream} — Rotatrix Build (unofficial)

Source: {commit}
CI: https://github.com/{repo}/actions/runs/{run_id}

Windows x64, macOS Intel/Apple Silicon, and Ubuntu 22.04/24.04/26.04 packages
include SHA256 checksums and source manifests from the same workflow run.

This draft requires release review before publication. These builds are unsigned
on Windows and ad-hoc signed on macOS. Final releases require distribution signing
and notarization where applicable, plus GUI and Rotatrix hardware validation.
Do not publish an unsigned draft as a final release.
"""
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / 'notes.md'
        path.write_text(notes, encoding='utf-8')
        command = ['gh', 'release', 'create', tag, *assets, '--verify-tag', '--draft',
                   '--title', f'BambuStudio {tag}', '--notes-file', str(path)]
        if prerelease:
            command.append('--prerelease')
        subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
