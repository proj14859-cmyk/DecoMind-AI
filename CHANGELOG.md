# Registro de Cambios (Changelog) - DecoMind AI

Todas las modificaciones notables realizadas en este proyecto están documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/) y este proyecto se adhiere a [Semantic Versioning](https://semver.org/lang/es/).

---

## [0.3.2-alpha] - 2026-10-25 — Parche y Modificación del Diseño Visual

### Selector de Colores Circular Estilo Editor Oficial de Geometry Dash
- **Reemplazo completo del selector de color (`GDColorPickerPopup`)**:
  - Eliminado el título `"PALETA DEL MENU INTERACTIVO"` y los controles rectangulares anteriores.
  - Implementado el diseño circular oficial del editor de Geometry Dash: rueda circular exterior de matiz (360°), disco central de saturación y brillo con cursores circulares en vivo, recuadro cuadrado de vista previa en la esquina superior izquierda, casillas `R:`, `G:`, `B:` y campo `HEX:` editable a la izquierda, botones `COPY`, `PASTE` y `DEFAULT` a la derecha, y botón verde `OK` en la parte inferior central.

### Rediseño Moderno y Simplificado de `AJUSTES`
- **Interfaz limpia sin bordes excesivos ni opciones repetidas**:
  - Sustitución de las cajas recargadas y botones de borde grueso por una disposición moderna de 4 filas separadas por divisores delgados (`0.8f`) y botones planos de borde sutil (`0.9f`):
    1. **Modo de conexión** (`Local` / `Servidor HTTP` + botón `Cambiar`).
    2. **Dirección del servidor (HTTP)** (campo `TextInput` + botón `Guardar`).
    3. **Color principal del menú** (muestra de color + botón `Seleccionar` que abre la rueda circular).
    4. **Flujo RGB en contornos** (botón `Alternar` + controles `-` / `+` de velocidad del flujo de `25%` a `300%`).

### Tarjetas de los 9 Modos y Flujo RGB Tipo "Gusano" (`INICIO`)
- **Color entero limpio y espaciado tipográfico rediseñado**:
  - Eliminado el degradado escalonado interior de las tarjetas, reemplazándolo por su color entero original uniforme y limpio (`m.bgColor`).
  - Desactivada la animación brusca de zoom al tocar/pasar sobre las tarjetas (`m_scaleMultiplier = 1.015f`) y añadido un marco interior complementario suave al seleccionarlas.
  - Rediseñada la distribución vertical de icono, título, línea separadora sutil y las dos líneas de descripción (conservando su texto íntegro) con márgenes holgados para que sean claramente visibles y nunca toquen el borde inferior.
- **Contornos RGB con flujo tipo "gusano" (traslado de punto A hacia punto B)**:
  - Implementado `ui::drawWormPerimeterBorder()`: en lugar de cambiar todo el borde de color a la vez, una estela luminosa recorre continuamente el perímetro del recuadro trasladándose del punto A al punto B con velocidad regulable desde `AJUSTES` (aplicado tanto a las 9 tarjetas en su propia paleta como al banner del aliado y a los picos decorativos de GD).

### Encuadre del Chat, Barra Superior, Botones e Historial
- **Encuadre exacto de `CHAT / COMANDO` en el centro (Pestaña `CHAT`)**:
  - Reconstruido `m_chatPanelNode` en coordenadas locales con punto de anclaje central `{0.5, 0.5}` para que, al seleccionar la pestaña `CHAT`, se ubique exactamente en el centro horizontal del cuadrado central (`x = 699` ref) y ligeramente pegado a su borde superior, con el cuadro de estado/respuesta debajo y sin pisar la columna derecha.
- **Limpieza de la esquina superior derecha**:
  - Eliminados el icono del rombo/diamante y la cápsula de barra superior ubicados en la esquina superior derecha del menú.
- **Botones en color entero con iluminación media**:
  - Eliminado el degradado de todos los botones laterales y de acción; al seleccionarlos se iluminan en color entero con intensidad media equilibrada (`#0d4fa8`).
- **Panel derecho `HISTORIAL` y ventana emergente `HISTORIAL DE PETICIONES`**:
  - En el recuadro derecho `HISTORIAL`: eliminados los botones `>` al lado de cada oración y sus contornos gruesos; se conserva **un único botón compacto de borde delgado en la esquina superior derecha del recuadro** que despliega la ventana de historial.
  - En la ventana emergente `HISTORIAL DE PETICIONES`: eliminado el contorno de color de cada oración, dejando únicamente el texto visible limpio separado oración por oración dentro de la lista deslizante.

---

## [Catalogo de IDs y prompts] - 2026-10-25 (antes numerado 0.3.13)

### Prompts de los modos (punto 2)
- Reescritos los 9 `internalPrompt` de `Modes.hpp` (objetivo claro, restricciones de jugabilidad y honestidad cuando falta información del nivel).
- Nuevas reglas por modo en el servidor (`Server/mod_python_api/prompts.py`): qué puede y qué no puede hacer cada modo, criterios de creatividad, grupos 9000-9100 y canales 900-950 para triggers mientras la IA no lee el nivel, y formato exacto de la respuesta.
- `estructurar` e `ideas` no colocan objetos (estructurar necesita leer el nivel, aún no disponible).

### Catálogo de IDs de objetos (punto 3)
- Nuevo `Server/mod_python_api/catalog.py` + `Server/data/catalog_curated.json`: la IA solo puede usar IDs del catálogo y de las categorías que permite su modo.
- Nuevo comando **`/catalogo`** en el chat del mod: lee todos los objetos del juego (`ObjectToolbox`) y los sube al servidor (`POST /catalog` -> `Server/data/catalog_game.txt`). Nuevo `src/ObjectCatalog.hpp` y `AIClient::uploadCatalog`.
- Los objetos aceptan `rot`, `scale` y `props` (claves de GD validadas por rango) para triggers, colores, grupos y capas.
- `LevelValidator`: segunda barrera de seguridad; en modos que no son layout se rechazan picos, portales, velocidades, pads y orbes.
- El generador local de prueba ya no coloca picos en `triggers`/`transiciones` ni objetos en `estructurar`.
- `groq_provider.py` incluye la elección automática de modelo disponible y el detalle de los errores HTTP.

---

## [0.3.12] - 2026-10-25
- Nuevo botón del editor (`resources/editor-btn.png`) y nuevo logo oficial (`logo.png`) en formato RGBA de 32 bits.
- Cuadrícula central de modos deslizante hacia abajo (`ScrollLayer`), reubicación dinámica del panel de chat y manual completo en `README.md`.

---

## [0.3.11] - 2026-10-24
- Soporte para proveedores de IA mediante API (`Server/mod_python_api/groq_provider.py` con soporte para `GROQ_API_KEY`, `GROQ_MODEL`, `GROQ_MAX_OBJS` y carga automática de `Server/.env`).
- Paquete modular `Server/mod_python_api/` (`protocol.py`, `providers.py`, `quota.py`) e iconos HD (`resources/*.png`).

---

## [0.3.0] - 2026-10-23
- Rediseño visual base fiel a la referencia (`1536x1024` $\rightarrow$ `480x320`) y arquitectura modular en C++ (`Modes.hpp`, `UIRenderer.hpp`, `AIClient.hpp`, `LevelValidator.hpp`, `LevelExecutor.hpp`, `HistoryManager.hpp`).

---

## [0.2.0] - Versión Anterior
- Prototipo inicial de menú con lista deslizante vertical de modos y servidor Python básico.
