import argparse
import os
import re
import shutil
import sys
import uuid
import xml.etree.ElementTree as Xml
from pathlib import Path

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
                    if child.suffix == '.vcxproj':
                        context.project_path = dest_file

                    Xml.register_namespace('', "http://schemas.microsoft.com/developer/msbuild/2003")
                    tree = Xml.parse(child)

                    guidnode = tree.find('.//{http://schemas.microsoft.com/developer/msbuild/2003}ProjectGuid')
                    if guidnode:
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

        app_project_re = re.compile(r"Project\(\"{[^}]+}\"\) = \"(?P<project>[^\"]+)\", \"..\\apps\\\1\\[^\"]+\", \"(?P<project_guid>{[^}]+})\"")
        project_configuration_block_start = re.compile(r"\s*GlobalSection\(ProjectConfigurationPlatforms\) = postSolution")
        project_configuration_re = re.compile(r"\s*(?P<project_guid>{[^}]+})\.[\S]+ = [\S]+")
        inserted_project = False
        added_configuration = False
        next_project_guid = None
        in_projectconfiguration_block = False

        with open(sln_path_bak, 'r') as src, open(sln_path, 'w') as dst:
            for line in src:
                if not inserted_project:
                    if line == "Global":
                        inserted_project = True
                    else:
                        m = app_project_re.match(line)
                        if m:
                            inserted_project = m.group('project') > context.project_name
                            next_project_guid = m.group('project_guid')

                    if inserted_project:
                        if next_project_guid:
                            print(f"Inserting {context.app_name} project before project {next_project_guid}")
                        else:
                            print(f"Inserting {context.app_name} project at end of projects")

                        dst.write(f'Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{context.project_name}", "{os.path.relpath(context.project_path, sln_path.parent)}", "{context.project_guid}"\n')
                        dst.write('EndProject\n')

                if not added_configuration:
                    if not in_projectconfiguration_block:
                        in_projectconfiguration_block = project_configuration_block_start.search(line) is not None
                        if in_projectconfiguration_block:
                            print("Entered project configuration block...")

                    if in_projectconfiguration_block:
                        if line == "\tEndGlobalSection":
                            added_configuration = True
                        elif next_project_guid is not None:
                            m = project_configuration_re.match(line)

                            if m:
                                added_configuration = m.group('project_guid') == next_project_guid

                        if added_configuration:
                            dst.write(f'\t\t{context.project_guid}.Debug|x64.ActiveCfg = Debug|x64\n')
                            dst.write(f'\t\t{context.project_guid}.Debug|x64.Build.0 = Debug|x64\n')
                            dst.write(f'\t\t{context.project_guid}.Development|x64.ActiveCfg = Development|x64\n')
                            dst.write(f'\t\t{context.project_guid}.Development|x64.Build.0 = Development|x64\n')
                            dst.write(f'\t\t{context.project_guid}.Release|x64.ActiveCfg = Release|x64\n')
                            dst.write(f'\t\t{context.project_guid}.Release|x64.Build.0 = Release|x64\n')

                dst.write(line)
    except:
        shutil.copy(sln_path_bak, sln_path)
        raise
    finally:
        sln_path_bak.unlink()


def process(app_name):
    inept_root = get_inept_root()
    template_dir = inept_root / 'apps/template'
    target_dir = inept_root / 'apps' / app_name.lower()
    print(f"Template dir: {template_dir}")
    print(f"Target dir: {target_dir}")
    target_dir.mkdir()

    try:
        context = ProcessingContext(app_name)
        process_template(context, target_dir, template_dir)
        add_project_to_solution(context, target_dir)
    except:
        rmtree(target_dir)
        raise
    # finally:
    #     if target_dir.exists():
    #         rmtree(target_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("name", help="Name of new app to add")
    args = parser.parse_args()
    print(f"Adding app {args.name}...")

    try:
        process(args.name)
    except Exception as err:
        print(f"Error: {err}")
