import argparse
import json
import os
import re
import shutil
from pathlib import Path
from create_metadata import METADATA_FILE_NAME, create_metadata


def _read_metadata_version(meta_path: Path) -> str:
    with open(meta_path, 'r', encoding='utf-8') as f:
        j = json.load(f)
        return j['version']


def main():
    parser = argparse.ArgumentParser(description='Merges multiple artifacts into one')
    parser.add_argument('--artifact-dir', required=True, help='Where artifacts are located')
    parser.add_argument('--out-dir', required=True, help='Where to put the merged artifact')
    parser.add_argument('--target', required=True, help='Target name')
    parser.add_argument('suffixes', nargs='+', help='Artifact suffixes to combine. Priority is first to last.')

    args = parser.parse_args()
    artifact_dir = Path(args.artifact_dir)
    out_dir = Path(args.out_dir)

    # Build file list
    files: dict[str, tuple[Path, Path]] = {} # lower_case_path -> rel path, root path

    for suffix in args.suffixes:
        # Find the artifact for this suffix
        name_suffix = f'-{args.target}-{suffix}'
        artifacts_with_suffix = list(artifact_dir.glob(f'*{name_suffix}', case_sensitive=True))

        if len(artifacts_with_suffix) == 0:
            raise Exception(f'No artifacts found for suffix = {suffix}')
        
        if len(artifacts_with_suffix) > 1:
            print('Found too many artifacts:')
            for i in artifacts_with_suffix:
                print(f'- {i}')
            raise Exception(f'Too many artifacts found for suffix = {suffix}')
        
        artifact_with_suffix = artifacts_with_suffix[0]

        # Build the file list
        for dirpath, _, filenames in os.walk(artifact_with_suffix):
            for filename in filenames:
                full_path = Path(dirpath) / filename
                rel_path = full_path.relative_to(artifact_with_suffix)
                key = rel_path.as_posix().lower()

                if key not in files:
                    files[key] = (rel_path, artifact_with_suffix)
                    print(f'{rel_path} -> {artifact_with_suffix.name}')

    # Read version from old metadata
    meta_rel_path, meta_root_dir = files[f'valve_addon/{METADATA_FILE_NAME}'.lower()]
    version = _read_metadata_version(meta_root_dir / meta_rel_path)
    out_artifact_name = f'BugfixedHL-{version}-{args.target}'

    # Pass artifact name to GitHub Actions
    if 'GITHUB_OUTPUT' in os.environ:
        print(f'Passing artifact name to GitHub Actions as artifact_name')
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as f:
            f.write(f'artifact_name={out_artifact_name}\n')
    else:
        print(f'Not running on GitHub Actions')

    # Copy the files
    artifact_inner_dir = out_dir / out_artifact_name
    artifact_inner_dir.mkdir(parents=True, exist_ok=True)
    
    for rel_path, root_path in files.values():
        abs_path = artifact_inner_dir / rel_path
        abs_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root_path / rel_path, abs_path)

    # Create new metadata
    create_metadata(version, artifact_inner_dir / 'valve_addon')

main()
