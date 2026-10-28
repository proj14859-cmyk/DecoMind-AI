# Registro de Cambios (Changelog) - DecoMind AI

Todas las modificaciones notables realizadas en este proyecto están documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/) y este proyecto se adhiere a [Semantic Versioning](https://semver.org/lang/es/).

---

## [0.3.4-alpha] - 2026-10-26 — Selector Oficial de Color de GD y Botón Flotante Movible del Editor

### 1. Selector de Color Oficial Nativo de Geometry Dash (`src/OfficialColorPickerPopup.hpp`)
- **Integración de `cocos2d::extension::CCControlColourPicker` (corroborado contra `libcocos2dcpp.so` ARM64)**:
  - Reemplazo directo de `GDColorPickerPopup` en la pestaña `AJUSTES` (conservando `GDColorPickerPopup` como respaldo).
  - Utiliza la rueda nativa oficial del juego (`CCControlColourPicker::colourPicker()` en `0xa9cf2c`), compuesta por `CCSpriteBatchNode("CCControlColourPickerSpriteSheet.png")`, el anillo exterior `CCControlHuePicker` (`0xa9d14c`) y el disco central `CCControlSaturationBrightnessPicker` (`0xa9d920`).
- **Correcciones sobre el borrador `button-fix/base`**:
  - **Calificación explícita de `cocos2d::extension::ColorPickerDelegate`** y ubicación de `#include "OfficialColorPickerPopup.hpp"` en ámbito global fuera del `namespace` anónimo de `main.cpp` (evitando incluir cabeceras estándar y del SDK dentro de `namespace {}`).
  - **Corrección de pivote al escalar (`setAnchorPoint({0.f, 0.f})`)**: en `CCControlColourPicker::init()` (`0xa9cb8c`), los sprites hijos se centran en `(0, 0)` mientras `contentSize` es `(168, 168)` e `ignoreAnchorPointForPosition` es `true`. Fijar `anchorPoint` en `{0.f, 0.f}` evita que `CCNode::nodeToParentTransform` (`0xacb544`) desplace la rueda al aplicar `setScale(0.84f)`.
  - **Prevención de bucle recursivo en entradas de texto (`m_updatingInputs`)**: como `CCControlColourPicker::setColorValue` (`0xa9cab0`) invoca automáticamente a `m_delegate->colorValueChanged` (`0xa9cb60`), se implementó la guarda `m_updatingInputs` (equivalente al flag `m_disableTextDelegate` en `ColorSelectPopup::updateColorLabels`, `0x74046c`) y se asigna `setDelegate(this)` después de inicializar todos los controles.
  - **Interfaz completa estilo `ColorSelectPopup` (`0x73d208`)**: doble muestra de color actual y color original con `whiteSquare60_001.png` (`0x73db48`), casillas editables `R:`, `G:`, `B:` (`0..255`) y `HEX:` (`RRGGBB`) sincronizadas en vivo en ambas direcciones, botones `COPY`, `PASTE`, `DEFAULT` y botón `OK`.

### 2. Botón Flotante Circular y Movible del Editor (`src/EditorButton.hpp`)
- **Comportamiento dual (toque corto vs. mantener y arrastrar) y transparencia del 50%-60%**:
  - **Toque corto (picar una vez)**: abre inmediatamente el menú interactivo de DecoMind AI.
  - **Mantener pulsado y mover el dedo o el ratón**: al superar el umbral de arrastre (`6.5 px`), el botón se mueve libremente por toda la pantalla limitado a los bordes visibles (`std::clamp`) y guarda su posición persistente en `btn-x` y `btn-y` (`Mod::get()->setSavedValue`).
  - **Transparencia del ~55% en reposo (`opacidad 140/255`)**: cumple el rango de 50%-60% de transparencia en reposo y se ilumina suavemente (`225/255`) con respuesta de escala al mantenerlo pulsado o arrastrarlo.
- **Correcciones críticas sobre el borrador `button-fix/button`**:
  - **Activación real de toques (`setTouchEnabled(true)` y `setTouchMode(kCCTouchesOneByOne)`)**: en `CCLayer::init()` (`0xaea00c`), `m_bTouchEnabled` (`[this + 360]`) se inicializa en `false`, por lo que `CCLayer::onEnter()` (`0xaec3ec`) jamás llamaba a `registerWithTouchDispatcher()` y el botón no respondía a ningún toque.
  - **Corrección de compilación `decomind::rgba4F`**: corregidas las llamadas `ui::rgba4F(...)` a `rgba4F(...)` (definida en `namespace decomind`, no en `decomind::ui`).
  - **Corrección de punto de anclaje (`ignoreAnchorPointForPosition(false)` + `setAnchorPoint({0.5f, 0.5f})`)**: asegura que `getPosition()` y `setPosition()` representen el centro geométrico real del botón (`34x34`), evitando que se desplace `(+17, +17)` o se corte en los bordes superior y derecho.
  - **Prioridad táctil `-250` y bloqueo ante modales / Playtest**: reemplazada la prioridad `-600` por `-250` (por encima de `EditorUI` en `0` y sus `CCMenu` en `-128`, pero por debajo de cualquier popup `FLAlertLayer` / `Popup` que usa `ForcePrio` en `-501` o menor), además de comprobar visibilidad de toda la jerarquía de padres, ausencia de `FLAlertLayer` activos en la escena y que el editor no esté en `PlaybackMode::Playing`.
  - **Sincronización en vivo del borde con el tema**: añadido `DragButton::refreshTheme()` para que el anillo del botón flotante actualice su color al instante cuando se cambia el color del menú en `AJUSTES`.
  - **Corrección de compatibilidad en Clang / NDK (`getParent()` e `isVisible()`)**: en Cocos2d-x 2.2 las funciones virtuales de `CCNode` no están marcadas como `const`, por lo que declarar `CCNode const*` en `isHierarchyVisible()` provocaba error de compilación estricta de C++23 en Windows y Android. Ajustado a puntero no-const `CCNode*`.

### 3. Corrección de Entrada de Texto Multilínea y Soporte de Letra `ñ`/`Ñ` en el Chat (`src/main.cpp` y `src/Modes.hpp`)
- **Corrección del borrado del texto previo al autocompletar o escribir palabras largas (`PromptTextFieldDelegate`)**:
  - En `libcocos2dcpp.so` (`CCTextInputNode::onTextFieldInsertText` en `0x5a4498`), cuando `strlen(text) > 1` (por ejemplo al autocompletar `"estructura"` -> `"tura"` en el teclado de Android, escribir rápido o insertar caracteres UTF-8 de 2 bytes como la `ñ`), el código nativo ejecutaba `this->setString(filtered)` (`0x5a4688`) sobrescribiendo todo el texto escrito anteriormente (`"genera una estruc"`) y llamaba a `onClickTrackNode(false)` (`0x5a46ac`) cerrando el teclado.
  - Se implementó `PromptTextFieldDelegate` (`cocos2d::CCTextFieldDelegate`) para interceptar `onTextFieldInsertText` y `onTextFieldDeleteBackward`, concatenando siempre los nuevos caracteres al texto existente sin borrar lo anterior ni cerrar el teclado.
- ** Salto de línea inteligente por palabras completas en 4 filas (`m_typedPreviewLines[4]`)**:
  - Se ocultó el `CCLabelBMFont` interno de una sola línea de `CCTextInputNode` (que cortaba visualmente en ~17 caracteres, ancho `ps(336.f)`) y se amplió la vista previa a **4 líneas completas** con ajuste por límites de palabra (`rfind(' ')`) para que ninguna palabra quede partida a la mitad.
- **Soporte completo de `ñ`, `Ñ` y tildes en español (`normalizePromptInputWithEnye` + `m_tildeOverlayNode`)**:
  - Acepta `ñ`/`Ñ` y vocales acentuadas (`á, é, í, ó, ú, ü, ¿, ¡`) tanto en UTF-8 (`0xC3 0xB1`) como en Latin-1/Windows-1252 (`0xF1`), dibujando visualmente la virgulilla (`~`) sobre la `n`/`N` en el cuadro de chat.

### 4. Versión Global y Documentación
- Versión global actualizada a **`0.3.4-alpha`** (`v0.3.4-alpha` en `mod.json`, `0.3.4` en `CMakeLists.txt`, cabeceras de `src/`, `Server/`, `README.md` y `About.md`).
- Añadido `COMO-FUNCIONA.md` con el desglose técnico verificado contra `libcocos2dcpp.so` (ARM64).

---

## [0.3.3-alpha fix] - 2026-10-26 — Correcciones tras auditar el zip 0.3.3-alpha

> En `mod.json` la version es `v0.3.3-alpha.1` (Geode no admite el texto "fix" en la version).

### Servidor (`Server/`)
- **`providers.py` completo**: el archivo del zip estaba cortado a mitad de `_contextual_deco_and_triggers`. Reescrito: `StubProvider`, `TemplateProvider`, `register_provider`, `get_provider` y la deco local contextual (usa `SELECTION` / `VIEWPORT`, los grupos y colores libres, o los rangos que pida el usuario).
- **`quota.py` restaurado** (faltaba en el zip; el servidor no arrancaba sin el).
- **Catalogo**: `Server/data/catalog_curated.json` vuelve a existir (93 IDs) y `catalog.py` recupera la copia de respaldo `DEFAULT_CURATED`, ahora con los IDs que decia el changelog (bordes 40/41/467/468, bloques estructurales, rellenos 211/503/918, halos 504/505/1001/1002, detalles 579..1888, portal Swing 1933). Estos IDs vienen del changelog anterior y no estan verificados en el juego: `/catalogo` manda sobre ellos.
- **Groq ve el nivel**: el mensaje a la IA incluye ahora `SELECTION`, `VIEWPORT`, grupos y colores ocupados/libres (`format_editor_perception`). Antes se parseaban pero no se enviaban.
- **Modo `estructurar` habilitado** tambien en el servidor (`NO_OBJECT_MODES` solo contiene `ideas`) y sus reglas en `prompts.py` actualizadas; los modos ya no dicen "aun no podemos leer el nivel".
- **Propiedades validadas** en `groq_provider.py` alineadas con `LevelValidator.hpp`: 4/5, 11, 17, 20, 30, 45-47, 52, 56, 61, 62, 64, 67, 68/69, 71, 85, 87, 150/151 y rangos de 21-25. La clave `57` acepta varios grupos (`"20.21.22"`, maximo 10).
- Pruebas nuevas `Server/tests/test_fix.py`.

### Cliente y documentacion
- Version renombrada a `0.3.3-alpha fix` en `README.md`, `About.md`, cabeceras de `src/` y `server.py`.
- Las dos entradas `0.3.3-alpha` del 25 de octubre quedan unidas en una sola.

---

## [0.3.3-alpha] - 2026-10-25

### Paso 5: estilos de decoracion y creatividad

### Estilos de decoracion ("tarjetas de estilo")
- **Nuevo `Server/mod_python_api/styles.py`**: la IA ahora recibe guias concretas de estilo cuando el usuario lo pide en su peticion (por ejemplo `decora con estilo espacial y glow`).
  - Detecta el estilo por palabras clave, sin importar tildes ni mayusculas. Soporta hasta 2 estilos a la vez; el que se nombra primero manda en la paleta.
  - Cada tarjeta incluye: idea general, paleta (fondo, principal, acento, detalle) con su **RGB ya calculado** para usarlo directamente en triggers de color, reglas de composicion y capas, plantillas de composicion, triggers recomendados y errores a evitar.
  - Con el catalogo del juego cargado (`/catalogo`), añade las **familias de objetos deco** que encajan con el estilo (por nombre de sprite) y sus IDs.
  - Se aplica solo en los modos visuales: `deco`, `layout_deco`, `block_design`, `transiciones` y `efectos`.
- **5 estilos incluidos** (interpretaciones libres, no copias de niveles concretos): `glow` (neon), `retro` (old school), `infierno` (inspirado en el estilo oscuro tipo Nine Circles), `espacial` y `ciudad`.
- **Estilos propios sin tocar codigo**: archivos JSON en `Server/data/styles_user/` (ejemplo incluido: `ejemplo.json.txt`). Un estilo con el mismo `id` reemplaza al incluido.
- Deteccion sin falsos positivos con los textos de ejemplo de los modos (por ejemplo "cian brillante" o "fondo oscuro" no activan ningun estilo).

### Cambios en archivos existentes
- `prompts.py`: `build_system(...)` acepta el parametro opcional `style_text`.
- `groq_provider.py`: llama a `style_block(...)` y muestra `[groq] estilo detectado en el pedido` en la consola.

### Documentacion y pruebas
- Nueva guia `Server/ESTILOS.md` (como funciona, como anadir estilos, limites).
- Nuevas pruebas `Server/tests/test_styles.py` (11 casos): `python -m unittest discover -s tests -v`.

### Percepción Contextual del Nivel, Selección, Grupos/Colores Libres y Animaciones GD 2.2

### 1. Percepción en Vivo del Nivel y Estructuras Seleccionadas (`src/LevelExecutor.hpp` y `src/AIClient.hpp`)
- **Lectura real de objetos seleccionados (`EditorUI::get()->getSelectedObjects()`)**:
  - Escaneo instantáneo de todas las piezas seleccionadas por el usuario en el editor: cálculo de su caja delimitadora exacta (`bbox = minX, minY, maxX, maxY`), dimensiones en bloques (`grid = WxH`) e inspección individual de cada objeto (`ID`, `X`, `Y`, rotación, escala, `Z-Layer`, `Z-Order`, `Color 1`, `Color 2` y `Group IDs`).
  - Detección geométrica automática de bordes superiores/inferiores/laterales, esquinas y relleno interior para que los modos **`Deco`**, **`Block Design`**, **`Estructurar`**, **`Triggers`** y **`Efectos Avanzados`** trabajen directamente sobre la estructura seleccionada.
- **Lectura de la vista de cámara activa (`VIEWPORT`)**:
  - Cuando el usuario no tiene objetos seleccionados, el mod calcula la posición y el zoom de la cámara (`LevelEditorLayer::m_objectLayer`) y envía los objetos visibles alrededor del área de trabajo para construir o decorar en la zona exacta donde está mirando el creador.

### 2. Escáner de `Group IDs` y `Color IDs` Ocupados vs. Libres
- **Detección completa de `Group IDs` (`1..9999`)**:
  - Recorre todos los `GameObject*` del nivel (`m_groups`) y todos los triggers `EffectGameObject*` (`m_targetGroupID` y `m_centerGroupID`) para construir el mapa compacto de grupos ocupados (`USED_GROUPS`), el siguiente grupo libre (`nextFreeGroup`) y la reserva de grupos libres recomendados (`FREE_GROUPS`).
- **Detección completa de `Color IDs` (`1..999`) y Paleta Activa**:
  - Recorre los canales de color asignados a objetos (`m_baseColor`, `m_detailColor`), los canales objetivo de triggers (`m_targetColor`) y los canales configurados en `GJEffectManager` (`m_fromColor` RGB, `m_blending` y `m_fromOpacity`) para enviar a la IA qué canales están ocupados (`USED_COLORS`), su paleta cromática actual y qué canales están libres (`FREE_COLORS`).
- **Respeto de rangos pedidos por el usuario o asignación libre automática**:
  - Si el usuario especifica rangos en su instrucción (por ejemplo: *"genera una deco estilo neón para esta estructura seleccionada usando estos group ID de colores del 10 al 14 así como el group ID de objetos del 20 al 25"*), tanto el servidor como el generador local utilizan exactamente esos rangos repartidos por capas (estructura base, bordes, detalles y glow). Si no los especifica, se asignan automáticamente IDs de `FREE_GROUPS` y `FREE_COLORS` sin colisionar con el resto del nivel.

### 3. Soporte de Propiedades Oficiales `LevelString` de GD 2.2 y Animaciones de Entrada/Salida
- **Ampliación de `LevelValidator.hpp` y `groq_provider.py`**:
  - Soporte y validación estricta para todas las propiedades clave de objetos y triggers de Geometry Dash 2.2:
    - **Objetos**: `1` (ID), `2/3` (X/Y), `4/5` (Flip X/Y), `6` (Rotación), `20/61` (Editor Layers), `21/22` (Color 1 / Color 2), `24/25` (Z-Layer / Z-Order), `32` (Escala), `57` (Group IDs simples o múltiples separados por punto como `"20.21.22"`, hasta 10 grupos por objeto), `64/67` (Don't Fade / Don't Enter).
    - **Triggers (`899` Color, `901` Move, `1006` Pulse, `1007` Alpha, `1049` Toggle, `1268` Spawn, `1346` Rotate, `1347` Follow, `1616` Stop, `2067` Scale)**: `7/8/9` (R/G/B), `10` (Duración), `11/62/87` (Touch/Spawn/Multi-Trigger), `17` (Blending), `23` (Target Color ID), `28/29` (Move X/Y), `30` (Easing `0..18`), `35` (Opacidad `0..1`), `45/46/47` (Fade In / Hold / Fade Out), `51` (Target Group ID), `52` (Pulse Target Type), `56` (Activate Group), `68/69` (Rotate Degrees / Times 360), `71` (Center Group ID), `85` (Easing Rate) y `150/151` (Scale X/Y).
- **Animaciones automáticas de Entrada, Pulso, Rotación y Salida**:
  - Generación coordinada de triggers de inicialización (`Move` + `Alpha 0` instantáneos previos a la estructura), **animación de entrada** (`Move` con `EaseOut` + `Alpha 1`), **pulso/rotación** sobre los detalles de la estructura y **animación de salida** (`Move` con `EaseIn` + `Alpha 0` al superar la estructura).

### 4. Ampliación del Catálogo Curado y Servidor Python (`Server/`)
- **Catálogo curado oficial ampliado (`Server/data/catalog_curated.json` y `catalog.py`)**:
  - Incorporados IDs oficiales de GD 2.2 para bordes (`40`, `41`, `467`, `468`), bloques estructurales (`1..7`, `62..66`, `83`, `90`, `91`, `144`, `145`, `160`, `161`, `192`), rellenos sin colisión (`211`, `503`, `918`), halos y esquinas de luz (`504`, `505`, `1001`, `1002`), detalles geométricos y tecnológicos (`579`, `580`, `725`, `919`, `1140`, `1249`, `1250`, `1585`, `1587`, `1705..1707`, `1755`, `1888`), portal Swing (`1933`) y triggers completos.
  - Habilitado el modo **`Estructurar`** (`estructurar`) tanto en el cliente como en el servidor ahora que el mod lee las estructuras seleccionadas, los grupos y los colores del nivel.
- **Nuevas métricas en vivo en la pestaña `ESTADÍSTICAS` (`src/main.cpp`)**:
  - Muestra en tiempo real la cantidad de objetos seleccionados, los rangos de `Group IDs` ocupados junto con el siguiente grupo libre (`G#`) y los `Color IDs` ocupados junto con el siguiente canal libre (`C#`).

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
