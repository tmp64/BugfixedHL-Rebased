#!/usr/bin/env python3

import argparse
import datetime
import distutils
import distutils.dir_util
import os
import shutil
import subprocess
import sys
import zipfile
from CreateMetadata import create_metadata


# Also needs to be changed in CMakeLists.txt
DEFAULT_VERSION = [1, 9, 1, 'dev', '']


# ---------------------------------------------
# Platform stuff
# ---------------------------------------------
def get_platform_type() -> str:
    if sys.platform.startswith('linux'):
        return 'linux'
    elif sys.platform.startswith('win32'):
        return 'windows'
    else:
        return 'unknown'


class PlatformWindows:
    script = None

    def __init__(self, scr):
        self.script = scr

    def get_cmake_args(self):
        args = []
        if self.script.vs_version == '2017':
            args.extend(['-G', 'Visual Studio 15 2017'])
        elif self.script.vs_version == '2019':
            args.extend(['-G', 'Visual Studio 16 2019'])
            args.extend(['-A', 'Win32'])

        args.extend(['-T', self.script.vs_toolset])

        return args
    
    def get_cmake_build_args(self):
        return ['--', '/p:VcpkgEnabled=false']

    def need_cmake_build_type_var(self):
        return False

    def update_bin_path(self):
        self.script.paths.out_bin = self.script.paths.build + '/' + self.script.build_type + '/'

    def get_dll_ext(self):
        return '.dll'


class PlatformLinux:
    script = None

    def __init__(self, scr):
        self.script = scr

    def get_cmake_args(self):
        args = []
        args.extend(['-G', 'Ninja'])

        toolchain_file = ''

        if self.script.linux_compiler == 'gcc':
            toolchain_file = 'ToolchainLinuxGCC.cmake'
        elif self.script.linux_compiler == 'gcc-8':
            toolchain_file = 'ToolchainLinuxGCC8.cmake'
        elif self.script.linux_compiler == 'gcc-9':
            toolchain_file = 'ToolchainLinuxGCC9.cmake'

        if toolchain_file:
            args.extend(['-DCMAKE_TOOLCHAIN_FILE={}cmake/{}'.format(self.script.repo_root, toolchain_file)])
        return args
    
    def get_cmake_build_args(self):
        return []

    def need_cmake_build_type_var(self):
        return True

    def update_bin_path(self):
        self.script.paths.out_bin = self.script.paths.build + '/'

    def get_dll_ext(self):
        return '.so'


# ---------------------------------------------
# Targets
# ---------------------------------------------
class FileToCopy:
    src = ''
    dst = ''

    def __init__(self, _src, _dst):
        self.src = _src
        self.dst = _dst


COMMON_FILES_TO_COPY = [
    FileToCopy('README.md', 'valve_addon/README_BugfixedHL.md')
]


class TargetClient:
    script = None

    def __init__(self, scr):
        self.script = scr

    def get_build_target_names(self):
        return ['client', 'test_client']

    def get_file_list(self):
        files = COMMON_FILES_TO_COPY
        files.append(FileToCopy(self.script.paths.out_bin + 'client' + self.script.platform.get_dll_ext(),
                                'valve_addon/cl_dlls/client' + self.script.platform.get_dll_ext()))
        files.append(FileToCopy('gamedir/resource', 'valve_addon/resource'))
        files.append(FileToCopy('gamedir/sound', 'valve_addon/sound'))
        files.append(FileToCopy('gamedir/sprites', 'valve_addon/sprites'))
        files.append(FileToCopy('gamedir/ui', 'valve_addon/ui'))
        files.append(FileToCopy('gamedir/commandmenu_default.txt', 'valve_addon/commandmenu_default.txt'))

        if get_platform_type() == 'windows':
            files.append(FileToCopy(self.script.paths.out_bin + 'client.pdb',
                                    'valve_addon/cl_dlls/client.pdb'))

        return files


class TargetServer:
    script = None

    def __init__(self, scr):
        self.script = scr

    def get_build_target_names(self):
        return ['hl', 'test_server', 'bugfixedapi_amxx']

    def get_file_list(self):
        files = COMMON_FILES_TO_COPY
        files.append(FileToCopy(self.script.paths.out_bin + 'hl' + self.script.platform.get_dll_ext(),
                                'valve_addon/dlls/hl' + self.script.platform.get_dll_ext()))

        if get_platform_type() == 'windows':
            files.append(FileToCopy(self.script.paths.out_bin + 'hl.pdb',
                                    'valve_addon/dlls/hl.pdb'))
            files.append(FileToCopy(self.script.paths.out_bin + 'bugfixedapi_amxx.dll',
                                    'valve_addon/addons/amxmodx/modules/bugfixedapi_amxx.dll'))
            files.append(FileToCopy(self.script.paths.out_bin + 'bugfixedapi_amxx.pdb',
                                    'valve_addon/addons/amxmodx/modules/bugfixedapi_amxx.pdb'))
        elif get_platform_type() == 'linux':
            files.append(FileToCopy(self.script.paths.out_bin + 'bugfixedapi_amxx_i386.so',
                                    'valve_addon/addons/amxmodx/modules/bugfixedapi_amxx_i386.so'))

        return files


# ---------------------------------------------
# Build script class
# ---------------------------------------------
class BuildScript:
    class Paths:
        base = ''
        build = ''
        out_bin = ''
        archive_root = ''
        archive_files = ''

    allowed_targets = ['client', 'server']
    allowed_build_types = ['debug', 'release']

    allowed_vs_versions = ['2017', '2019']
    allowed_vs_toolsets = ['v141', 'v141_xp', 'v142']

    allowed_linux_compilers = ['gcc', 'gcc-8', 'gcc-9']

    platform = None
    build_target = None
    build_target_name = ''
    build_type = ''  # Values are Debug or RelWithDebInfo
    vs_version = ''
    vs_toolset = ''
    linux_compiler = ''
    release_version = ''
    release_version_array = []

    cmake_binary = ''
    cmake_args = []

    repo_root = ''
    git_hash = ''
    date_code = ''
    out_dir_name = ''
    paths = Paths()

    def run(self):
        if get_platform_type() == 'unknown':
            print("Unknown platform. This script only supports Windows and Linux.")
            exit(1)
        elif get_platform_type() == 'windows':
            self.platform = PlatformWindows(self)
        elif get_platform_type() == 'linux':
            self.platform = PlatformLinux(self)

        try:
            self.repo_root = \
                subprocess.Popen(['git', 'rev-parse', '--show-toplevel'], stdout=subprocess.PIPE).communicate()[
                    0].rstrip().decode('utf-8').replace('\\', '/') + '/'
            self.git_hash = \
                subprocess.Popen(['git', 'rev-parse', '--short', 'HEAD'], stdout=subprocess.PIPE).communicate()[
                    0].rstrip().decode('utf-8')
        except Exception as e:
            print('Failed to get info from Git repo: ' + str(e))
            exit(1)

        self.date_code = datetime.datetime.now().strftime('%Y%m%d')

        self.parse_arguments()

        self.run_cmake()
        self.build_targets()
        self.copy_files()
        self.create_install_metadata()
        self.create_zip()

    def parse_arguments(self):
        parser = argparse.ArgumentParser(description='Builds production build of BugfixedHL, ready for release on '
                                                     'GitHub.')

        parser.add_argument('--target', action='store', required=True, choices=self.allowed_targets,
                            help='(required) target to build')
        parser.add_argument('--build-type', action='store', required=True, choices=self.allowed_build_types,
                            help='(required) build type')
        parser.add_argument('--vs', action='store', required=(get_platform_type() == 'windows'),
                            choices=self.allowed_vs_versions, help='(windows only, required) Visual Studio version')
        parser.add_argument('--toolset', action='store', required=(get_platform_type() == 'windows'),
                            choices=self.allowed_vs_toolsets, help='(windows only, required) Visual Studio toolset')
        parser.add_argument('--linux-compiler', action='store', default='gcc',
                            choices=self.allowed_linux_compilers, help='(linux only) CMake toolchain file to use')
        # parser.add_argument('--updater', action='store_true',
        #                    help='enable updater')
        parser.add_argument('--out-dir', action='store',
                            help='output directory (if not set, uses <workdir>/BugfixedHL-<version>)')
        parser.add_argument('--cmake-bin', action='store', default=shutil.which("cmake"),
                            help='path to cmake binary (PATH used instead)')
        parser.add_argument('--cmake-args', action='store',
                            help='additional CMake arguments')
        parser.add_argument('--github-actions', dest='ci', action='store_true',
                            help='build for GitHub Actions')

        # Version override
        parser.add_argument('--v-major', action='store', type=int, default=DEFAULT_VERSION[0],
                            help='sets major version number')
        parser.add_argument('--v-minor', action='store', type=int, default=DEFAULT_VERSION[1],
                            help='sets minor version number')
        parser.add_argument('--v-patch', action='store', type=int, default=DEFAULT_VERSION[2],
                            help='sets patch version number')
        parser.add_argument('--v-tag', action='store', default=DEFAULT_VERSION[3],
                            help='sets version tag')
        parser.add_argument('--v-meta', action='store', default=DEFAULT_VERSION[4],
                            help='sets version metadata')

        # Parse arguments
        args = parser.parse_args()
        self.vs_version = args.vs
        self.vs_toolset = args.toolset
        self.linux_compiler = args.linux_compiler
        self.cmake_binary = args.cmake_bin
        if args.cmake_args:
            self.cmake_args = [i.strip() for i in args.cmake_args.split(' ')]

        # Set target
        self.build_target_name = args.target
        if args.target == 'client':
            self.build_target = TargetClient(self)
        elif args.target == 'server':
            self.build_target = TargetServer(self)

        # Set build type
        if args.build_type == 'debug':
            self.build_type = 'Debug'
        elif args.build_type == 'release':
            self.build_type = 'RelWithDebInfo'

        # Parse version
        self.release_version = "{}.{}.{}".format(args.v_major, args.v_minor, args.v_patch)
        if args.v_tag:
            self.release_version = "{}-{}".format(self.release_version, args.v_tag)
        if args.v_meta:
            self.release_version = "{}+{}".format(self.release_version, args.v_meta)

        self.release_version_array = [args.v_major, args.v_minor, args.v_patch, args.v_tag]

        # Set output directory
        self.out_dir_name = 'BugfixedHL-{}-{}-{}-{}-{}'.format(
            self.release_version.replace('+', '-'),
            self.build_target_name, 
            get_platform_type(), 
            self.git_hash, 
            self.date_code
        )

        # Set artifact name
        if args.ci:
            with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
                f.write('artifact_name=BugfixedHL-{}-{}-{}-{}'.format(
                    self.release_version.replace('+', '-'),
                    self.build_target_name,
                    get_platform_type(),
                    self.git_hash
                ))

        out_dir = args.out_dir
        if out_dir:
            self.paths.base = out_dir
        else:
            work_dir = os.getcwd()
            out_dir = self.out_dir_name
            if os.path.exists(os.path.realpath(work_dir + '/' + out_dir)):
                count = 1
                while True:
                    out_dir = '{}-{:03}'.format(self.out_dir_name, count)
                    if os.path.exists(os.path.realpath(work_dir + '/' + out_dir)):
                        count += 1
                    else:
                        break

                    if count > 999:
                        print('Failed to create out path. Try setting it manually')
                        exit(1)
                        break
            self.paths.base = os.path.realpath(work_dir + '/' + out_dir)

        self.paths.base = os.path.realpath(self.paths.base) + '/'
        self.paths.build = self.paths.base + 'build'
        self.paths.archive_root = self.paths.base + 'archive' + '/'
        self.paths.archive_files = self.paths.archive_root + self.out_dir_name + '/'

        try:
            os.mkdir(self.paths.base)
            os.mkdir(self.paths.build)
            os.mkdir(self.paths.archive_root)
            os.mkdir(self.paths.archive_files)
            self.platform.update_bin_path()
        except OSError as e:
            print("Failed to create out paths: {}. Try setting it manually.".format(str(e)))
            exit(1)

    def run_cmake(self):
        try:
            args = [self.cmake_binary]
            args.extend(['-S', self.repo_root])
            args.extend(['-B', self.paths.build])
            args.extend(self.platform.get_cmake_args())
            args.extend(['-DUSE_UPDATER=TRUE'])
            args.extend(['-DBHL_VERSION_MAJOR=' + str(self.release_version_array[0])])
            args.extend(['-DBHL_VERSION_MINOR=' + str(self.release_version_array[1])])
            args.extend(['-DBHL_VERSION_PATCH=' + str(self.release_version_array[2])])

            if self.release_version_array[3] == '':
                args.extend(['-DBHL_VERSION_TAG=no_tag'])
            else:
                args.extend(['-DBHL_VERSION_TAG=' + self.release_version_array[3]])

            args.extend(self.cmake_args)

            if self.platform.need_cmake_build_type_var():
                args.extend(['-DCMAKE_BUILD_TYPE=' + self.build_type])

            print("---------------- Running CMake with arguments:")
            for i in args:
                print('\'', i, '\' ', sep='', end='')
            print()
            print()

            subprocess.run(args, check=True)

            print()
            print()
        except subprocess.CalledProcessError:
            print('CMake finished with non-zero status code.')
            exit(1)
        except Exception as e:
            print('Failed to run CMake: {}.'.format(str(e)))
            exit(1)

    def build_targets(self):
        try:
            targets = ''

            for target in self.build_target.get_build_target_names():
                targets += target + ' '

            targets = targets.strip()

            print("---------------- Building targets", targets)
            args = [self.cmake_binary]
            args.extend(['--build', self.paths.build])
            args.append('--target')
            args.extend(self.build_target.get_build_target_names())
            args.extend(['--config', self.build_type])
            args.extend(self.platform.get_cmake_build_args())

            for i in args:
                print('\'', i, '\' ', sep='', end='')
            print()
            print()

            subprocess.run(args, check=True)

            print()
            print()
        except subprocess.CalledProcessError:
            print('Build finished with non-zero status code.')
            exit(1)
        except Exception as e:
            print('Failed to run build: {}.'.format(str(e)))
            exit(1)

    def copy_files(self):
        print("---------------- Copying files")
        files = self.build_target.get_file_list()

        for i in files:
            print(i.src, '->', i.dst)

            if os.path.isabs(i.src):
                src = i.src
            else:
                src = self.repo_root + i.src
            dst = self.paths.archive_files + i.dst

            try:
                if not os.path.exists(src):
                    print('Error: file', src, "doesn't exist.")
                    exit(1)

                if os.path.exists(dst):
                    print('Error: file', dst, 'already exists.')
                    exit(1)

                if os.path.isdir(src):
                    distutils.dir_util.copy_tree(src, dst)
                else:
                    # Create parent dirs
                    distutils.dir_util.mkpath(os.path.dirname(dst))

                    # Copy file
                    shutil.copyfile(src, dst)

            except Exception as e:
                print('Failed to copy files: {}.'.format(str(e)))
                exit(1)

        print()
        print()

    def create_install_metadata(self):
        print("---------------- Creating metadata")
        try:
            create_metadata(self.release_version, self.paths.archive_files + 'valve_addon')
        except Exception as e:
            print('Failed to create metadata file: {}.'.format(str(e)))
            exit(1)

    def create_zip(self):
        print("---------------- Creating ZIP archive")
        try:
            zipf = zipfile.ZipFile('{}{}.zip'.format(self.paths.base, self.out_dir_name), 'w',
                                   zipfile.ZIP_DEFLATED)

            for root, dirs, files in os.walk(self.paths.archive_root):
                for file in files:
                    path = os.path.join(root, file)
                    zipf.write(path, os.path.relpath(path, self.paths.archive_root))

            zipf.close()
        except Exception as e:
            print('Failed to create ZIP archive: {}.'.format(str(e)))
            exit(1)


# Script entry point
if __name__ == '__main__':
    script = BuildScript()
    script.run()
