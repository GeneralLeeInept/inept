'''
Quick and dirty tiled map / tileset 'compiler' - pull out the bits I'm interested in and write them into a structured binary file

Map file format:
----------------
32-bits magic (FOURCC('MAP '))
16-bits tile size
16-bits map width (in tiles)
16-bits map height (in tiles)
map data (16 bits per tile)

16-bits tilesheet path length
tilesheet path (utf-8)
16-bits number of tiles
tile data (see below)

Tile data format:
-----------------
16-bit tile id
8-bits walkable flag
8-bits padding
'''
import argparse
import struct
import xml.etree.ElementTree as Xml
from pathlib import Path


def get_asset_root():
    script_path = Path.resolve(Path(__file__))
    return script_path.parents[1] / 'art'


def process_tileset(tsx_path, asset_root, output):
    tsx = Xml.parse(tsx_path)
    root = tsx.getroot()
    image = root.find('image')
    tilesheet_path = Path.resolve(tsx_path.parents[0]) / image.attrib['source']
    tilesheet_path = tilesheet_path.relative_to(asset_root)
    tilesheet_path = tilesheet_path.as_posix().encode('utf-8')
    output.write(struct.pack('H', len(tilesheet_path)))

    output.write(tilesheet_path)
    output.write(struct.pack('H', int(root.attrib['tilecount'])))

    for tile_node in root.findall('tile'):
        tile_id = int(tile_node.attrib['id'])
        walkable_flag = 2

        properties = tile_node.find('properties')

        if properties:
            for p in properties:
                if p.attrib['name'] == 'walkable':
                    walkable_flag = 1 if p.attrib['value'] == 'true' else 0

        output.write(struct.pack('H', tile_id))
        output.write(struct.pack('=B', walkable_flag))
        output.write(struct.pack('x'))


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

    for t in tile_data:
        output.write(struct.pack('H', int(t)))

    tsx_node = root.find('tileset')
    tsx_path = Path.resolve(Path(input.name).parents[0] / tsx_node.attrib['source'])
    process_tileset(tsx_path, asset_root, output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('map', type=argparse.FileType('r'), help='Map file to compile')
    parser.add_argument('out', type=argparse.FileType('wb'), help='Output file')
    args = parser.parse_args()

    try:
        process(src, get_asset_root(), output)

    except Exception as err:
        print(f'Error: {err}')
