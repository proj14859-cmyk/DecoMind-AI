# Integrar los estilos en tus archivos (si ya modificaste prompts.py o groq_provider.py)

Los archivos completos van incluidos, pero si en los pasos 3 y 4 ya cambiaste `prompts.py`
o `groq_provider.py`, no los sobrescribas: haz solo estos 3 cambios pequenos.

## 1) `Server/mod_python_api/prompts.py`
Cambia la firma de `build_system` y anade el estilo al final:

```python
def build_system(mode: str, max_objs: int, catalog_text: str, catalog_note: str = "",
                 style_text: str = "") -> str:
    ...
        if catalog_note:
            parts.append(catalog_note)
        if style_text:                    # <- NUEVO
            parts.append(style_text)      # <- NUEVO
```

## 2) `Server/mod_python_api/groq_provider.py`
Arriba, junto a los otros imports:
```python
from .styles import style_block
```
Y donde se llama a `build_system(...)` dentro de `generate()`:
```python
style = style_block(req.prompt, req.mode, self.catalog)
if style:
    print("[groq] estilo detectado en el pedido")
system = build_system(req.mode, self.max_objs, self.catalog.describe(req.mode), note, style)
```

## 3) Archivo nuevo
Copia `Server/mod_python_api/styles.py` (no depende de nada tuyo, solo de `catalog.py` para `DATA_DIR`).
Si renombraste `DATA_DIR` o cambiaste `catalog.py`, ajusta el import de la linea `from .catalog import DATA_DIR`.
