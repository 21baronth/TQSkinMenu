import sys
import io
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from import_table import extract


class ImportTests(unittest.TestCase):
	def parse(self, body):
		return extract(io.StringIO(body))

	def test_import_ignores_scripts_and_deduplicates_paths(self):
		result = self.parse('''<CheatTable><CheatEntries><CheatEntry>
		<Description>"Player Texture"</Description><DropDownList>Creatures\\PC\\skin.tex:Skin(999)
		creatures/pc/SKIN.tex:Duplicate(1)</DropDownList>
		<AssemblerScript>raise Exception("must never run")</AssemblerScript>
		</CheatEntry></CheatEntries></CheatTable>''')
		self.assertEqual(result, [{'path': 'Creatures\\PC\\skin.tex', 'label': 'Skin'}])

	def test_invalid_path_rejected(self):
		with self.assertRaises(ValueError):
			self.parse('<CheatTable><CheatEntry><Description>Player Texture</Description><DropDownList>../skin.tex:Bad</DropDownList></CheatEntry></CheatTable>')

	def test_missing_dropdown_rejected(self):
		with self.assertRaises(ValueError):
			self.parse('<CheatTable><CheatEntry><Description>Player Texture</Description></CheatEntry></CheatTable>')


if __name__ == '__main__':
	unittest.main()
