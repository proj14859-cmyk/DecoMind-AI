import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from mod_python_api import styles          # noqa: E402
from mod_python_api.catalog import Catalog  # noqa: E402
from mod_python_api.prompts import build_system  # noqa: E402


class StyleTests(unittest.TestCase):
    def ids(self, prompt):
        return [s["id"] for s in styles.detect_styles(prompt)]

    def test_detecta_cada_estilo(self):
        self.assertEqual(self.ids("decora con estilo GLOW"), ["glow"])
        self.assertEqual(self.ids("algo retro tipo old school"), ["retro"])
        self.assertEqual(self.ids("estilo Nine Circles"), ["infierno"])
        self.assertEqual(self.ids("deco espacial con planetas"), ["espacial"])
        self.assertEqual(self.ids("una ciudad de noche"), ["ciudad"])

    def test_el_primero_nombrado_manda(self):
        self.assertEqual(self.ids("decora con estilo espacial y glow"), ["espacial", "glow"])
        self.assertEqual(self.ids("glow pero espacial"), ["glow", "espacial"])

    def test_sin_acentos_y_mayusculas(self):
        self.assertEqual(self.ids("CÓSMICO"), ["espacial"])
        self.assertEqual(self.ids("Demoníaco"), ["infierno"])

    def test_dos_estilos_maximo(self):
        self.assertEqual(len(self.ids("espacial neon ciudad retro")), 2)

    def test_sin_falsos_positivos_en_los_ejemplos_de_los_modos(self):
        for p in ["Haz una transicion entre 2 zonas pasando de tono oscuro a cian brillante",
                  "Mejora la deco de esta parte con detalles claros",
                  "Contraste: fondo oscuro, detalles claros",
                  "version 2.0 del nivel"]:
            self.assertEqual(self.ids(p), [], p)

    def test_solo_en_modos_visuales(self):
        self.assertEqual(styles.style_block("estilo glow", "layout"), "")
        self.assertEqual(styles.style_block("estilo glow", "ideas"), "")
        self.assertIn("Glow", styles.style_block("estilo glow", "deco"))

    def test_paleta_con_rgb_para_triggers(self):
        txt = styles.style_block("espacial", "deco")
        self.assertIn("R,G,B = 5,8,24", txt)  # #050818

    def test_familias_del_catalogo(self):
        d = tempfile.mkdtemp()
        c = Catalog(d)
        c.save_game_catalog("20|d_star_01_001.png\n21|d_star_02_001.png\n30|d_gear_01_001.png\n40|d_pizza_01_001.png\n")
        txt = styles.style_block("espacial", "deco", c)
        self.assertIn("d_star: 20,21", txt)
        self.assertIn("d_gear: 30", txt)
        self.assertNotIn("pizza", txt)

    def test_limite_de_tamano(self):
        self.assertLessEqual(len(styles.style_block("espacial neon", "deco")), styles.MAX_CHARS)

    def test_estilo_de_usuario(self):
        d = tempfile.mkdtemp()
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, "mio.json"), "w", encoding="utf-8") as f:
            f.write('{"id":"mio","name":"Mio","aliases":["mi estilo"],"palette":{"fondo":"#102030"}}')
        old = styles.USER_STYLES_DIR
        styles.USER_STYLES_DIR = d
        try:
            self.assertEqual(self.ids("hazlo con mi estilo"), ["mio"])
        finally:
            styles.USER_STYLES_DIR = old

    def test_build_system_incluye_el_estilo(self):
        s = build_system("deco", 300, "[deco] x", "", styles.style_block("glow", "deco"))
        self.assertIn("ESTILO PEDIDO", s)
        s2 = build_system("deco", 300, "[deco] x")
        self.assertNotIn("ESTILO PEDIDO", s2)


if __name__ == "__main__":
    unittest.main()
