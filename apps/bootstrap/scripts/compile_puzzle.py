import argparse
import struct
import xml.etree.ElementTree as Xml
from pathlib import Path


class RamFragment:
    def __init__(self, xml):
        self.address = int(xml.attrib['addr'], 16)
        self.data = [int(x, 16) for x in xml.text.split(',')]

    def write(self, output):
        output.write(struct.pack('H', self.address))
        output.write(struct.pack('H', len(self.data)))
        binary_data = bytearray(self.data)
        output.write(binary_data)


class CpuState:
    def __init__(self, xml):
        self.program_counter = int(xml.attrib.get('pc', '0'), 16)
        self.status_register = int(xml.attrib.get('p', '0'), 16)
        self.accumulator = int(xml.attrib.get('a', '0'), 16)
        self.index_register = int(xml.attrib.get('x', '0'), 16)
        self.ram = [RamFragment(x) for x in xml.findall('ram')]

    def write(self, output):
        output.write(struct.pack('H', self.program_counter))
        output.write(struct.pack('=B', self.status_register))
        output.write(struct.pack('=B', self.accumulator))
        output.write(struct.pack('=B', self.index_register))
        output.write(struct.pack('H', len(self.ram)))

        for fragment in self.ram:
            fragment.write(output)


class Test:
    def __init__(self, xml):
        self.initial_state = CpuState(xml.find('initial_state'))
        self.pass_state = CpuState(xml.find('pass_state'))

    def write(self, output):
        self.initial_state.write(output)
        self.pass_state.write(output)


def write_string(s, output):
    if s:
        chars = s.encode('utf-8')
        output.write(struct.pack('H', len(chars)))
        output.write(chars)
    else:
        output.write(struct.pack('H', 0))


def process(input, output):
    # process input
    xml = Xml.parse(input)
    puzzle = xml.getroot()
    title = puzzle.find('title')
    description = puzzle.find('description')
    tests = [Test(x) for x in puzzle.findall('tests/test')]

    # write output
    output.write(struct.pack('L', 0x50555A5A))  # 'PUZZ' FOURCC
    write_string(title.text, output)
    write_string(description.text, output)
    output.write(struct.pack('H', len(tests)))

    for test in tests:
        test.write(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('puzzle', type=argparse.FileType('r'), help='Puzzle file to compile')
    parser.add_argument('out', type=argparse.FileType('wb'), help='Output file')
    args = parser.parse_args()

    #try:
    process(args.puzzle, args.out)

    #except Exception as err:
    #print(f'Error: {err}')
