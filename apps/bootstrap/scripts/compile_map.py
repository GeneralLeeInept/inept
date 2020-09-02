'''
Quick and dirty tiled map / tileset 'compiler' - pull out the bits I'm interested in and write them into a structured binary file

Map file format:
----------------
32-bits magic (FOURCC('MAP '))
16-bits tile size
16-bits map width (in tiles)
16-bits map height (in tiles)
map data (16 bits per tile)

16-bits minimum tile x
16-bits minimum tile y
16-bits maximum tile y
16-bits maximum tile y

32-bits player spawn x
32-bits player spawn y

16-bits ai spawn point count
ai spawn points (2 x 32-bits each)

16-bits number of zones
zone data (see below)

16-bits tilesheet path length
tilesheet path (utf-8)
16-bits number of tiles
tile data (see below)


Zone data format:
-------------------
32-bits top left x
32-bits top left y
32-bits width
32-bits height
16-bit zone name length
zone name (utf-8)


Tile data format:
-----------------
16-bit tile id
8-bits flags:
    7 6 5 4 3 2 1 0
    | | | | | | | +- Solid v movables
    | | | | | | +--- Blocks LOS
    | | | | | +----- Solid v bullet
    | | | | +------- Solid v AI
    | | | +--------- ??
    | | +----------- ??
    | +------------- ??
    +--------------- ??
8-bits padding
'''
import argparse
import struct
import xml.etree.ElementTree as Xml
from pathlib import Path


def get_asset_root():
    script_path = Path.resolve(Path(__file__))
    return script_path.parents[1] / 'art'


def write_string(s, output):
    chars = s.encode('utf-8')
    output.write(struct.pack('H', len(chars)))
    output.write(chars)


def process_tileset(tsx_path, asset_root, output):
    tsx = Xml.parse(tsx_path)
    root = tsx.getroot()
    image = root.find('image')
    tilesheet_path = (Path.resolve(tsx_path.parents[0]) / image.attrib['source']).relative_to(asset_root)
    write_string(tilesheet_path.as_posix(), output)

    output.write(struct.pack('H', int(root.attrib['tilecount'])))

    flag_dict = { 'blocks_movables' : 1, 'blocks_los' : 2, 'blocks_bullets' : 4, 'blocks_ai' : 8 }

    for tile_node in root.findall('tile'):
        tile_id = int(tile_node.attrib['id'])
        flags = 0
        properties = tile_node.findall('properties/property[@value="true"]')

        for p in properties:
            flags |= flag_dict[p.attrib['name']]

        output.write(struct.pack('H', tile_id))
        output.write(struct.pack('=B', flags))
        output.write(struct.pack('x'))


def write_spawn(spawn_point, tile_size, output):
    x = float(spawn_point.attrib['x']) / float(tile_size)
    y = float(spawn_point.attrib['y']) / float(tile_size)
    output.write(struct.pack('f', x))
    output.write(struct.pack('f', y))


def write_zone(zone, tile_size, output):
    x = float(zone.attrib['x']) / float(tile_size)
    y = float(zone.attrib['y']) / float(tile_size)
    w = float(zone.attrib['width']) / float(tile_size)
    h = float(zone.attrib['height']) / float(tile_size)
    output.write(struct.pack('f', x))
    output.write(struct.pack('f', y))
    output.write(struct.pack('f', w))
    output.write(struct.pack('f', h))
    write_string(zone.attrib['name'], output)


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

    data_node = root.find('layer/data[@encoding="csv"]')
    tile_data = data_node.text.split(',')

    for t in tile_data:
        output.write(struct.pack('H', int(t)))

    map_min_x = root.find('./properties/property[@name="map_min_x"]')
    map_min_y = root.find('./properties/property[@name="map_min_y"]')
    map_max_x = root.find('./properties/property[@name="map_max_x"]')
    map_max_y = root.find('./properties/property[@name="map_max_y"]')
    output.write(struct.pack('H', int(map_min_x.attrib['value'])))
    output.write(struct.pack('H', int(map_min_y.attrib['value'])))
    output.write(struct.pack('H', int(map_max_x.attrib['value'])))
    output.write(struct.pack('H', int(map_max_y.attrib['value'])))

    spawn_points = root.findall('./objectgroup[@name="Spawns"]/object[@type="spawn"]')
    player_spawn = next(filter(lambda x: x.attrib['name'] == 'player', spawn_points))
    write_spawn(player_spawn, tile_size, output)
    ai_spawns = list(filter(lambda x: x.attrib['name'] == 'ai', spawn_points))
    output.write(struct.pack('H', int(len(ai_spawns))))

    for spawn in ai_spawns:
        write_spawn(spawn, tile_size, output)

    zones = root.findall('./objectgroup[@name="Zones"]/object')
    output.write(struct.pack('H', int(len(zones))))

    for zone in zones:
        write_zone(zone, tile_size, output)

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
