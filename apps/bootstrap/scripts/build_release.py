import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from git import *

tag_re = re.compile(r'bootstrap-(?P<ver>\d+\.\d+.\d+)')

def get_inept_root():
    script_path = Path(os.path.abspath(__file__))
    return script_path.parents[3]

def get_app_root():
    script_path = Path(os.path.abspath(__file__))
    return script_path.parents[1]

def get_app_tags(repo):
    tags = sorted(list(filter(lambda x: tag_re.match(str(x)), repo.tags)), key=lambda t: t.tag.tagged_date)
    return tags

def get_prev_version(repo):
    tags = get_app_tags(repo)
    if tags:
        m = tag_re.match(str(tags[-1]))
        return m.group('ver')
    return '0.0.0'

def format_version(major, minor, patch):
    return f'{major}.{minor}.{patch}'

def get_next_version(repo):
    prev_version = get_prev_version(repo)
    major, minor, patch = prev_version.split('.')
    return format_version(major, minor, int(patch) + 1)

if __name__ == "__main__":
    try:
        inept_root = get_inept_root()
        repo = Repo(inept_root)
        if repo.is_dirty():
            raise RuntimeError("Repository is not up to date, cannot continue.")

        # Build ID
        head = repo.head.commit
        sha_head = head.hexsha
        build_id = repo.git.rev_parse(sha_head, short=7).upper()

        # Release tag
        release_version = get_next_version(repo)
        release_tag = f'bootstrap-{release_version}'
        print(f"Building version '{release_version} using commit '{build_id}'")

        # Build
        msbuild =  r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
        if not os.path.isfile(msbuild):
            raise RuntimeError("MsBuild not found.")

        solution = str(inept_root / 'project/inept.sln') # don't love this, should generate solutions for the apps
        print(f"Building {solution}...")
        target = '-t:bootstrap:Rebuild'
        configuration = '-p:Configuration=Release'
        platform = '-p:Platform=x64'
        appversion = f'-p:GliAppVersion=\\"Version {release_version} Build {build_id}\\"'
        shipping = '-p:GliShipping=1'
        subprocess.check_call([msbuild, solution, target, configuration, platform, appversion, shipping])

        # Create release folder
        release_dir = get_app_root() / 'releases' / release_version
        release_dir.mkdir(parents=True)

        # Build assets
        with tempfile.TemporaryDirectory() as tempdir:
            src_dir = get_app_root() / 'art'
            assets_dir = Path(tempdir) / 'assets'
            shutil.copytree(src_dir, assets_dir, ignore=shutil.ignore_patterns('*.pdn'))
            shutil.make_archive(Path(tempdir) / 'assets', 'zip', assets_dir)
            shutil.copy2(Path(tempdir) / 'assets.zip', release_dir / 'assets.glp')

        # Copy build to release
        shutil.copy2(get_inept_root() / 'project/_builds/bootstrap/Release/bin/bootstrap.exe', release_dir / 'bootstrap.exe')

        # Create zip
        shutil.make_archive(get_app_root() / f'releases/bootstrap-{release_version}', 'zip', release_dir)

        # Add tag to git
        new_tag = repo.create_tag(release_tag, ref=head, message=f'Bootstrap release {release_version}')

    except Exception as err:
        print(f"Error: {err}")
