'''
Quick and dirty tiled map / tileset 'compiler' - pull out the bits I'm interested in and write them into a structured binary file

Map file format:
----------------
32-bits magic (FOURCC('MAP '))
16-bits tile size
16-bits map width (in tiles)
16-bits map height (in tiles)
16-bits layer count

(layer count) map data (16 bits per tile)

16-bits tileset count
(tileset count) tilesets

Tileset format:
---------------
16-bits first tile id
16-bits tile count
16-bits texture path length
(texture path length) texture path
'''
import argparse
import struct
import xml.etree.ElementTree as Xml
from pathlib import Path


def write_string(output_file, string):
    chars = string.encode('utf-8')
    output_file.write(struct.pack('H', len(chars)))
    output_file.write(chars)


def process_tileset(firstgid, path, output_file, asset_root):
    with open(path, 'r') as input:
        tsx = Xml.parse(input)
        root = tsx.getroot()
        output_file.write(struct.pack('H', firstgid))
        tile_count = int(root.attrib['tilecount'])
        output_file.write(struct.pack('H', tile_count))
        image = root.find('image')
        image_path = (path.parent / Path(image.attrib['source'])).relative_to(asset_root)
        write_string(output_file, str(image_path))


def process(input, output, asset_root):
    outputDirectory = output.parent
    outputDirectory.mkdir(parents=True, exist_ok=True)

    with open(output, 'wb') as output_file:
        output_file.write(b'MAP ')

        tmx = Xml.parse(input)
        root = tmx.getroot()
        tile_size = int(root.attrib['tilewidth'])
        width = int(root.attrib['width'])
        height = int(root.attrib['height'])
        output_file.write(struct.pack('H', tile_size))
        output_file.write(struct.pack('H', width))
        output_file.write(struct.pack('H', height))

        layers = root.findall('layer/data[@encoding="csv"]')
        layer_count = len(layers)
        output_file.write(struct.pack('H', layer_count))

        for l in layers:
            tile_data = l.text.split(',')

            for t in tile_data:
                output_file.write(struct.pack('H', int(t)))

        tile_sets = root.findall('tileset')
        tile_set_count = len(tile_sets)
        output_file.write(struct.pack('H', tile_set_count))

        for ts in tile_sets:
            firstgid = int(ts.attrib['firstgid'])
            source = ts.attrib['source']
            path = (input.parent / source).absolute().resolve()
            process_tileset(firstgid, path, output_file, asset_root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--assetroot', '-a', type=Path, default='.', help='Asset root path')
    parser.add_argument('map', type=Path, help='Map file to compile')
    parser.add_argument('out', type=Path, help='Output file')
    args = parser.parse_args()
    asset_root = Path.resolve(args.assetroot)
    process(args.map, args.out, asset_root)
