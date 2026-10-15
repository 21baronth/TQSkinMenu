"""Import only the Player Texture dropdown; never execute CT scripts."""
import argparse
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def extract(path):
	root = ET.parse(path).getroot()
	entries = [e for e in root.iter('CheatEntry') if e.findtext('Description', '').strip('"') == 'Player Texture']
	if len(entries) != 1:
		raise ValueError('Expected exactly one Player Texture entry')
	skins, seen = [], set()
	for line in entries[0].findtext('DropDownList', '').splitlines():
		if not line.strip():
			continue
		path, label = line.split(':', 1)
		path = path.strip()
		if not path.lower().endswith('.tex') or len(path) > 255 or '..' in path or not path.isascii():
			raise ValueError(f'Invalid texture path: {path}')
		key = path.lower().replace('\\', '/')
		if key in seen:
			continue
		seen.add(key)
		# The table's parenthesized lengths are often wrong; the game computes them.
		label = re.sub(r'\(\d+\)\s*$', '', label).strip()
		skins.append({'label': label, 'path': path})
	if not skins:
		raise ValueError('No textures found')
	return skins


if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('table', type=Path)
	parser.add_argument('--output', type=Path, default=Path(__file__).parent / 'src' / 'skins.generated.h')
	args = parser.parse_args()
	skins = extract(args.table)
	args.output.parent.mkdir(parents=True, exist_ok=True)
	lines = ['// Imported texture data only. No CT scripts are executed.', '#pragma once', 'struct Skin { const char* label; const char* path; };', 'inline constexpr Skin kSkins[] = {']
	lines += ['\t{' + json.dumps(s['label']) + ', ' + json.dumps(s['path']) + '},' for s in skins]
	lines += ['};', '']
	args.output.write_text('\n'.join(lines), encoding='utf-8')
	print(f'Imported {len(skins)} unique textures into {args.output}')
