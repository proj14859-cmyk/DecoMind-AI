import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from mod_python_api import ApiRequest
from mod_python_api.catalog import Catalog, DEFAULT_CURATED
from mod_python_api.groq_provider import build_objects
from mod_python_api.providers import get_provider
from mod_python_api.prompts import build_system


class FixTests(unittest.TestCase):
    def setUp(self):
        self.cat = Catalog(data_dir="/nonexistent")  # fuerza el respaldo embebido

    def test_changelog_ids_in_catalog(self):
        ids = {i for i, _, _ in DEFAULT_CURATED}
        for i in (40, 467, 211, 504, 1705, 1933, 899, 2067, 192):
            self.assertIn(i, ids)

    def test_estructurar_places_deco(self):
        raw = [{"id": 503, "x": 300, "y": 15, "props": {"57": "20.21.22", "21": 10, "30": 2}}]
        s, d = build_objects(raw, "estructurar", 50, self.cat)
        self.assertEqual(d, 0)
        self.assertIn(",57,20.21.22", s)
        self.assertIn(",30,2", s)

    def test_deco_rejects_spike_and_bad_groups(self):
        s, d = build_objects([{"id": 8, "x": 0, "y": 15}], "deco", 50, self.cat)
        self.assertEqual((s, d), ("", 1))
        s, _ = build_objects([{"id": 503, "x": 0, "y": 15, "props": {"57": "1;2"}}], "deco", 50, self.cat)
        self.assertNotIn(",57,", s)

    def test_stub_uses_selection(self):
        req = ApiRequest(mode="deco", prompt="deco neon", selection="count=2|bbox=300,15,390,75",
                         free_groups="next=20|pool=20,21", free_colors="next=10|pool=10,11")
        r = get_provider("stub").generate(req)
        self.assertTrue(r.objects.endswith(";"))
        self.assertIn("57,20", r.objects)

    def test_estructurar_prompt_not_blocked(self):
        self.assertIn("LIBRES", build_system("estructurar", 10, "x"))


if __name__ == "__main__":
    unittest.main()
