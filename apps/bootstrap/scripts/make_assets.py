import argparse
import shutil
from pathlib import Path
import compile_map


def get_asset_root():
    script_path = Path.resolve(Path(__file__))
    return script_path.parents[1] / 'art'


def add_files(pattern, file_set):
    for f in get_asset_root().glob(pattern):
        path = Path(f)
        file_set.add(path)


def process_files(file_set, target_dir):
    for f in file_set:
        file_type = f.suffix.lower()
        target_path = target_dir / f.relative_to(get_asset_root())
        target_path.parents[0].mkdir(parents=True, exist_ok=True)

        if file_type == '.tmx':
            # Tiled map file...
            target_path = target_path.with_suffix('.bin')
            with open(f, 'rt') as input, open(target_path, 'wb') as output:
                compile_map.process(input, get_asset_root(), output)
        else:
            # Default
            shutil.copy2(f, target_path)


def process(asset_list, target_dir):
    inclusions = set()
    exclusions = set()

    for line in asset_list:
        line = line.rstrip('\n')
        if line.startswith('-'):
            add_files(line[1:], exclusions)
        else:
            add_files(line, inclusions)

    if target_dir.exists():
        shutil.rmtree(target_dir)

    target_dir.mkdir(parents=True)
    process_files(inclusions - exclusions, target_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('assets', type=argparse.FileType('r'), help='Source assets list')
    parser.add_argument('-o', dest='outdir', help='Target directory', required=True)
    args = parser.parse_args()

    try:
        target_dir = Path(args.outdir).resolve()
        process(assets, target_dir)
    except Exception as err:
        print(f'Error: {err}')
