import re

__all__ = ['Solution', 'parse']


class ProjectRef:
    parse_re = re.compile(r'Project\(\"(?P<type>[^\"]+)\"\)\s*=\s*\"(?P<name>[^\"]+)\",\s*\"(?P<rel_path>[^\"]+)\",\s*\"(?P<uuid>[^\"]+)\"\s*EndProject', re.MULTILINE)

    def __init__(self, source=None):
        if source:
            m = self.parse_re.match(source)
            self.type_uuid = m.group('type')
            self.name = m.group('name')
            self.rel_path = m.group('rel_path')
            self.uuid = m.group('uuid')
        else:
            self.type_uuid = None
            self.name = None
            self.rel_path = None
            self.uuid = None

    def __str__(self):
        return f'''Project("{self.type_uuid}") = "{self.name}", "{self.rel_path}", "{self.uuid}"
EndProject'''


class GlobalSection:
    parse_re = re.compile(r"\bGlobalSection\((?P<name>[^\)]+)\)\s*=\s*(?P<position>.*)\s*(?P<body>(?:.|\n|\r)*?)\s*EndGlobalSection", re.MULTILINE)
    kvp_re = re.compile(r"^\s*(?P<key>[^\s]+)\s*=\s*(?P<value>.*)$", re.MULTILINE)

    def __init__(self, source):
        m = self.parse_re.match(source)
        self.name = m.group('name')
        self.position = m.group('position')
        self.dict = {}

        for kvp in self.kvp_re.finditer(m.group('body')):
            self.dict[kvp.group('key')] = kvp.group('value')

    def __str__(self):
        body = '\n'.join([f'\t\t{key} = {value}' for key, value in self.dict.items()])
        return  f"""\tGlobalSection({self.name}) = {self.position}
{body}
\tEndGlobalSection
"""

class Solution:
    header_re = re.compile(r"""
Microsoft Visual Studio Solution File, Format Version (?P<format_version>[\d]+\.[\d]+)
# Visual Studio Version 16
VisualStudioVersion = (?P<vs_version>[\d]+(?:\.[\d]+)*)
MinimumVisualStudioVersion = (?P<minimum_vs_version>[\d]+(?:\.[\d]+)*)
""", re.MULTILINE)

    project_defs_re = re.compile(r"\bProject\b(?:(?:.|\n|\r)*?)\bEndProject\b", re.MULTILINE)
    global_sections_re = re.compile(r"\bGlobalSection\b(?:(?:.|\n|\r)*?)\bEndGlobalSection\b", re.MULTILINE)

    def __init__(self):
        self.format_version = "12.00"
        self.vs_version = "16.0.30204.135"          # 16.6.2
        self.minimum_vs_version = "10.0.40219.1"    # 2010 SP1?
        self.projects = []

    def parse(self, source):
        close_source = False

        if not hasattr(source, 'read'):
            source = open(source, 'rt')
            close_source = True

        try:
            data = source.read()

            header = self.header_re.search(data)
            self.format_version = header.group('format_version')
            self.vs_version = header.group('vs_version')
            self.minimum_vs_version = header.group('minimum_vs_version')
            self.projects = [ProjectRef(m.group(0)) for m in self.project_defs_re.finditer(data)]
            self.global_sections = [GlobalSection(m.group(0)) for m in self.global_sections_re.finditer(data)]

        finally:
            if close_source:
                source.close()

    def write(self, dest):
        close_dest = False

        if not hasattr(dest, 'write'):
            dest = open(dest, 'wt', encoding='utf-8')
            close_dest = True

        try:
            dest.writelines(f"""
Microsoft Visual Studio Solution File, Format Version {self.format_version}
# Visual Studio Version {self.vs_version.partition('.')[0]}
VisualStudioVersion = {self.vs_version}
MinimumVisualStudioVersion = {self.minimum_vs_version}
""")

            dest.writelines('\n'.join(str(project) for project in self.projects))

            sections = ''.join(str(gs) for gs in self.global_sections)
            dest.writelines(f"""
Global
{sections}EndGlobal
"""
            )

        finally:
            if close_dest:
                dest.close()

    def add_project(self, name, rel_path, uuid, parent=None):
        p = ProjectRef()
        p.name = name
        p.rel_path = rel_path
        p.uuid = uuid
        p.type_uuid = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'  # vcxproj
        # TODO: validate uniqueness
        self.projects.append(p)

        # TODO: Sort projects

        solution_configs = next((gs for gs in self.global_sections if gs.name == 'SolutionConfigurationPlatforms'), None)
        project_configs = next((gs for gs in self.global_sections if gs.name == 'ProjectConfigurationPlatforms'), None)

        for sc in solution_configs.dict.keys():
            key = f'{uuid}.{sc}.ActiveCfg'
            project_configs.dict[key] = sc
            key = f'{uuid}.{sc}.Build.0'
            project_configs.dict[key] = sc

        if parent:
            parent_project = next((p for p in self.projects if p.name == parent), None)
            nested_projects = next((gs for gs in self.global_sections if gs.name == 'NestedProjects'), None)
            nested_projects.dict[uuid] = parent_project.uuid


def parse(source):
    solution = Solution()
    solution.parse(source)
    return solution
