import argparse
import os
import re
import shutil
import sys
import uuid
import xml.etree.ElementTree as Xml
from pathlib import Path
import visualstudio.solution as VS

def rmtree(dir : Path):
    for child in dir.iterdir():
        if child.is_file():
            child.unlink()
        else:
            rmtree(child)
    dir.rmdir()


def get_inept_root():
    script_path = Path(os.path.abspath(__file__))
    return script_path.parents[1]


class ProcessingContext:
    def __init__(self, app_name):
        self.app_name = app_name
        self.project_guid = f"{{{uuid.uuid4()}}}".upper()
        self.project_name = app_name.lower()
        self.project_path = None


def process_template(context, target_dir, template_dir):
    print(f"Entering directory {template_dir} -> {target_dir}...")

    for child in template_dir.iterdir():
        if  child.is_file():
            # Process template file
            src_filename = str(child.name)
            if src_filename.startswith('template'):
                dst_filename = src_filename.replace('template', context.project_name)
                dest_file = target_dir / dst_filename
                print(f"Processing file {child} -> {dest_file}...")

                if str(child.name).startswith('template.vcxproj'):
                    ns = { 'msbuild' :  'http://schemas.microsoft.com/developer/msbuild/2003' }

                    Xml.register_namespace('', 'http://schemas.microsoft.com/developer/msbuild/2003')
                    tree = Xml.parse(child)

                    if child.suffix == '.vcxproj':
                        context.project_path = dest_file

                        guidnode = tree.find('msbuild:PropertyGroup[@Label="Globals"]/msbuild:ProjectGuid', ns)

                        if guidnode is None:
                            raise RuntimeError("Failed to locate ProjectGuid node in template.")

                        guidnode.text = context.project_guid

                    filenodes = tree.findall('.//*[@Include]')

                    for n in filenodes:
                        filename = Path(n.attrib['Include'])
                        if filename.name.startswith('template'):
                            filename = filename.parent / Path(context.project_name + filename.suffix)
                            n.attrib['Include'] = str(filename)

                    with open(dest_file, 'wb') as output:
                        tree.write(output, encoding='UTF-8')
                elif child.suffix == '.ico':
                    dest_file = target_dir / child.name
                    shutil.copy(child, dest_file)
                else:
                    with open(child, 'rt') as src, open(dest_file, 'wt') as dest:
                        for line in src:
                            line = line.replace('${Template}', context.app_name)
                            dest.write(line)
            else:
                dest_file = target_dir / child.name
                print(f"Copying file {child} -> {dest_file}...")
                shutil.copy(child, dest_file)
        else:
            target_child_dir = target_dir / child.name
            template_child_dir = child
            target_child_dir.mkdir()
            process_template(context, target_child_dir, template_child_dir)


def add_project_to_solution(context, target_dir):
    sln_path = get_inept_root() / 'project/inept.sln'
    sln_path_bak = sln_path.with_suffix(sln_path.suffix + '.bak')
    shutil.copy(sln_path, sln_path_bak)

    try:
        print(f"Adding {context.app_name} to {sln_path}...")
        solution = VS.parse(sln_path)
        solution.add_project(context.project_name, os.path.relpath(context.project_path, sln_path.parent), context.project_guid, 'apps')
        solution.write(sln_path)
    except:
        shutil.copy(sln_path_bak, sln_path)
        raise
    finally:
        sln_path_bak.unlink()


def process(app_name, add_to_sln):
    inept_root = get_inept_root()
    template_dir = inept_root / 'scripts/app_template'
    target_dir = inept_root / 'apps' / app_name.lower()
    print(f"Template dir: {template_dir}")
    print(f"Target dir: {target_dir}")
    target_dir.mkdir()

    try:
        context = ProcessingContext(app_name)
        process_template(context, target_dir, template_dir)

        if add_to_sln:
            add_project_to_solution(context, target_dir)
    except:
        rmtree(target_dir)
        raise
    # finally:
    #     if target_dir.exists():
    #         rmtree(target_dir)


if __name__ == "__main__":
    print (sys.version)
    parser = argparse.ArgumentParser()
    parser.add_argument("name", help="Name of new app to add")
    parser.add_argument("--no-solution", action='store_false', dest='add_to_sln', help="Do not add the app to the inept solution")
    args = parser.parse_args()
    print(f"Adding app {args.name}...")

    try:
        process(args.name, args.add_to_sln)
    except Exception as err:
        print(f"Error: {err}")
