#!/usr/bin/env python3

# A function for creating metadata for BHL update file.
# This file can be called as a script:
#
#   ./CreateMetadata.py "1.0.0-dev+version_string" "/path/to/files"
#
# It will create file
#   /path/to/files/bugfixedhl_install_metadata.dat
import argparse
import hashlib
import json
import os

# List of files that don't need to be updated
USER_MODIFIABLE_FILES = [
    'ui/resource/ChatScheme.res',
    'ui/resource/ClientScheme.res',
    'ui/resource/ClientSourceScheme.res',
    'ui/scripts/HudLayout.res',
]


def create_metadata(version, startpath):
    meta = {
        'version': version,
        'files': {}
    }

    for root, dirs, files in os.walk(startpath):
        for file in files:
            fullpath = os.path.join(root, file)
            path = os.path.relpath(fullpath, startpath).replace('\\', '/')

            # Skip files outside of path (impossible?)
            if path.startswith('..'):
                continue

            file_data = {
                'size': os.path.getsize(fullpath),
                'hash_sha1': '',
                'user_modifiable': False,
            }

            hasher = hashlib.sha1()
            with open(fullpath, 'rb') as infile:
                buf = infile.read()
                hasher.update(buf)

            file_data['hash_sha1'] = hasher.hexdigest()

            for userFile in USER_MODIFIABLE_FILES:
                if userFile.lower() == path.lower():
                    file_data['user_modifiable'] = True
                    break

            meta['files'][path] = file_data

    with open(startpath + '/bugfixedhl_install_metadata.dat', "a") as f:
        f.write(json.dumps(meta, sort_keys=True, indent=4))


# Script entry point
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Creates update metadata file for BugfixedHL')
    parser.add_argument('--version', action='store', required=True, help='version of the update')
    parser.add_argument('--path', action='store', required=True, help='path to update files')
    args = parser.parse_args()

    create_metadata(args.version, args.path)
