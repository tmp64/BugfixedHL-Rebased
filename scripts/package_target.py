import argparse
import os
import re
import shutil
from pathlib import Path
from create_metadata import create_metadata

SEMVER_REGEX = re.compile(r'^(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$')

def main():
    parser = argparse.ArgumentParser(description='Prepares CMake install results for zipping')
    parser.add_argument('--build-dir', required=True, help='CMake build directory')
    parser.add_argument('--install-dir', required=True, help='CMake install directory')
    parser.add_argument('--artifact-dir', required=True, help='Output artifact directory. A new directory will be created there')
    parser.add_argument('--target', required=True, choices=('client', 'server'), help='Target type for packaging')
    parser.add_argument('--suffix', required=True, help='Name suffix')
    parser.add_argument('--allow-mod', action=argparse.BooleanOptionalAction, help='Allow version to have .m suffix')
    
    args = parser.parse_args()
    build_dir = Path(args.build_dir)
    install_dir = Path(args.install_dir)
    artifact_dir = Path(args.artifact_dir)

    # Read the version file
    with open(build_dir / 'version.txt', 'r', encoding='utf-8') as f:
        version = f.read().strip()

        # Check that version is valid
        if not SEMVER_REGEX.match(version):
            raise Exception(f'Version is not a valid semver 2.0.0 version (version is {version})')

        # Make sure we don't package binaries with '.m' suffix
        if version.endswith('.m') and not args.allow_mod:
            raise Exception(f'Source tree is modified (version is {version})')
        
        version_main = version.split("+")[0]

    # Assemble the artifact name
    artifact_name = f'BugfixedHL-{version_main}-{args.target}-{args.suffix}'
    print(f'artifact_name = {artifact_name}')

    # Copy install files to artifact dir
    artifact_inner_dir = artifact_dir / artifact_name / 'valve_addon'
    artifact_inner_dir.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(install_dir, artifact_inner_dir)

    # Generate metadata
    create_metadata(version_main, artifact_inner_dir)

    # Pass artifact name to GitHub Actions
    if 'GITHUB_OUTPUT' in os.environ:
        print(f'Passing artifact name to GitHub Actions as artifact_name')
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as f:
            f.write(f'artifact_name={artifact_name}\n')
    else:
        print(f'Not running on GitHub Actions')


main()
