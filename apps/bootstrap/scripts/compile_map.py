'''
Quick and dirty tiled map / tileset 'compiler' - pull out the bits I'm interested in and write them into a structured binary file

Map file format:

32-bits magic (FOURCC('MAP '))
16-bits tile size
16-bits map width (in tiles)
16-bits map height (in tiles)
16-bits tilesheet path length
map data (16 bits per tile)
tilesheet path (utf-8)
'''
import argparse
import struct
import xml.etree.ElementTree as Xml
from pathlib import Path


def get_asset_root():
    script_path = Path.resolve(Path(__file__))
    return script_path.parents[1] / 'art'


def process(input, asset_root, output):
    tmx = Xml.parse(input)
    root = tmx.getroot()
    tile_size = int(root.attrib['tilewidth'])
    width = int(root.attrib['width'])
    height = int(root.attrib['height'])
    output.write(struct.pack('L', 0x4d415020))
    output.write(struct.pack('H', tile_size))
    output.write(struct.pack('H', width))
    output.write(struct.pack('H', height))

    data_node = root.find('layer/data')
    tile_data = data_node.text.split(',')

    tsx_node = root.find('tileset')
    tsx_path = Path.resolve(Path(input.name).parents[0] / tsx_node.attrib['source'])
    tsx = Xml.parse(tsx_path)
    root = tsx.getroot()
    image = root.find('image')
    tilesheet_path = Path.resolve(tsx_path.parents[0]) / image.attrib['source']
    tilesheet_path = tilesheet_path.relative_to(asset_root)
    tilesheet_path = tilesheet_path.as_posix().encode('utf-8')
    output.write(struct.pack('H', len(tilesheet_path)))

    for t in tile_data:
        output.write(struct.pack('H', int(t)))

    output.write(tilesheet_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('map', type=argparse.FileType('r'), help='Map file to compile')
    parser.add_argument('out', type=argparse.FileType('wb'), help='Output file')
    args = parser.parse_args()

    try:
        process(src, get_asset_root(), output)

    except Exception as err:
        print(f'Error: {err}')
