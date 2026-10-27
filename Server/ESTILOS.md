# Estilos de decoracion (paso 5: creatividad)

## El problema
La IA sabe en general que es "glow", "retro" o "espacial", pero **no sabe como se traduce
en Geometry Dash**: que colores usar, que objetos, como ordenar las capas, que triggers.
Sin eso, decora "al azar".

## La solucion: tarjetas de estilo
Cada estilo es una tarjeta corta con:

| Campo | Para que sirve |
|---|---|
| `aliases` | palabras con las que el usuario lo pide ("glow", "nine circles", "espacial"...) |
| `mood` | la idea general en una frase |
| `palette` | fondo / principal / acento / detalle. El servidor calcula el RGB de cada color para que la IA lo use directo en triggers de color (claves 7, 8, 9) |
| `reglas` | reglas de composicion (capas, densidad, tamanos, simetria) |
| `familias` | trozos de nombre de sprite; el servidor busca en el catalogo del juego (`/catalogo`) las familias de objetos deco que encajan y le pasa sus IDs a la IA |
| `composicion` | plantillas relativas ("estrellas cada 60 unidades...") que la IA adapta |
| `triggers` | triggers recomendados (solo se usan si el modo los permite) |
| `evitar` | errores tipicos del estilo |

## Como se aplica
1. El usuario escribe, por ejemplo, "decora con estilo espacial y glow".
2. `styles.detect_styles()` encuentra las palabras clave (sin importar tildes ni mayusculas). Maximo 2 estilos; el que nombras primero manda en la paleta.
3. `styles.style_block()` arma el texto (unos 600-900 caracteres por estilo) y se anade al prompt del sistema.
4. El mismo validador de siempre sigue protegiendo el gameplay: la IA solo puede usar IDs permitidos por el modo.

Solo se aplica en los modos visuales: `deco`, `layout_deco`, `block_design`, `transiciones` y `efectos`.

## Estilos incluidos
`glow` (neon), `retro` (old school), `infierno` (inspirado en el estilo oscuro tipo Nine Circles), `espacial`, `ciudad`.
Son **interpretaciones libres**, no copias de ningun nivel. Ajustalas a tu gusto en `styles.py` (lista `BUILTIN`).

## Anadir tus propios estilos (sin tocar codigo)
1. Copia `data/styles_user/ejemplo.json.txt` como `data/styles_user/mi_estilo.json`.
2. Cambia `id`, `aliases`, colores y reglas. Los campos `id` y `aliases` son obligatorios.
3. Reinicia el servidor. Si usas el mismo `id` que uno incluido, el tuyo lo reemplaza.

## Limites (honestos)
- Es **conocimiento escrito a mano**: mejora mucho la coherencia, pero no garantiza que el resultado quede bonito. Hay que probar y afinar las tarjetas con lo que veas en el editor.
- Las familias de objetos se eligen por el **nombre del sprite**, que es una heuristica. Si un estilo sale sin IDs, revisa `familias` contra tu `catalog_game.txt`.
- Los modelos gratuitos tienen contexto limitado, por eso cada tarjeta es corta.

## Siguiente paso posible: aprender estilos de tus niveles
Cuando la IA pueda leer el nivel (paso 4), se puede **generar una tarjeta automaticamente** a partir de un nivel que te guste: colores usados, familias de objetos mas frecuentes y densidad. Asi "hazlo como mi nivel X" funciona sin escribir nada a mano.

## Pruebas
```
cd Server
python -m unittest discover -s tests -v
```
