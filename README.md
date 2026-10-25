# Manual Oficial de DecoMind AI (v0.3.2-alpha)

**DecoMind AI** es un asistente interactivo de inteligencia artificial diseñado de forma nativa para el editor de niveles de **Geometry Dash 2.2081** sobre **Geode SDK v5.11.2**. Permite generar estructuras, construir layouts jugables, añadir decoración, diseñar bloques personalizados, configurar triggers automáticos, crear transiciones, aplicar efectos avanzados y obtener sugerencias de diseño directamente desde una interfaz sci-fi integrada en el editor.

---

## Índice

1. [Cómo Funciona el Mod (Flujo General)](#1-cómo-funciona-el-mod-flujo-general)
2. [Acceso desde el Editor y Estructura del Menú](#2-acceso-desde-el-editor-y-estructura-del-menú)
3. [Los 9 Modos Especializados de Construcción](#3-los-9-modos-especializados-de-construcción)
4. [Sistema de Chat / Comando y Reubicación Dinámica](#4-sistema-de-chat--comando-y-reubicación-dinámica)
5. [Panel de Uso de la IA, Historial y Herramientas Inferiores](#5-panel-de-uso-de-la-ia-historial-y-herramientas-inferiores)
6. [Configuraciones, Paleta de Colores y Efectos RGB](#6-configuraciones-paleta-de-colores-y-efectos-rgb)
7. [Arquitectura del Servidor y Proveedores de IA (`stub`, `groq` y personalizados)](#7-arquitectura-del-servidor-y-proveedores-de-ia-stub-groq-y-personalizados)
8. [Validación de Seguridad sobre el Nivel (`LevelValidator`)](#8-validación-de-seguridad-sobre-el-nivel-levelvalidator)
9. [Estructura de Archivos del Proyecto](#9-estructura-de-archivos-del-proyecto)

---

## 1. Cómo Funciona el Mod (Flujo General)

DecoMind AI separa su arquitectura en seis capas independientes para garantizar estabilidad, seguridad sobre tu nivel y facilidad para cambiar de proveedor de IA en cualquier momento:

1. **Capa de Interfaz Visual (`src/main.cpp` y `src/UIRenderer.hpp`)**: dibuja de forma nativa en Cocos2d-x (`CCDrawNode`, `CCSprite`, `CCLabelBMFont`, `ScrollLayer`, `TextInput`) toda la interfaz interactiva con proporción `3:2` (`480x320` en unidades de Geometry Dash), animaciones RGB por paleta de colores y transiciones suaves.
2. **Capa de Gestión de Modos (`src/Modes.hpp`)**: mantiene separados para cada uno de los 9 modos tres elementos distintos:
   - Su **identificador interno** (`id`: `"estructurar"`, `"layout"`, `"deco"`, etc.).
   - Su **descripción visible** para el jugador en la interfaz.
   - Sus **instrucciones internas (preprompt)** para el modelo de IA (`internalPrompt`), que jamás se muestran en la pantalla y solo se adjuntan internamente al enviar la solicitud.
3. **Capa de Comunicación (`src/AIClient.hpp`)**: gestiona tanto el modo **Local** (generador de prueba sin conexión a internet) como el modo **Servidor HTTP** (que se comunica con `Server/server.py` mediante peticiones asíncronas `geode::utils::web::WebRequest`).
4. **Capa de Proveedores de IA en Python (`Server/mod_python_api/`)**: recibe la petición del mod, verifica la cuota diaria (`DailyQuota`) y la envía al proveedor configurado (`stub` para pruebas locales o `groq` para generación real con modelos LLM como `llama-3.3-70b-versatile`).
5. **Capa de Validación (`src/LevelValidator.hpp`)**: inspecciona cada objeto devuelto antes de tocar el editor, comprobando IDs válidos (`1..4600`), coordenadas dentro de límites seguros, escalas y restricciones propias del modo (por ejemplo, impidiendo que el modo `ideas` modifique objetos del nivel).
6. **Capa de Ejecución y Respaldos (`src/LevelExecutor.hpp`)**: inserta los objetos validados en `LevelEditorLayer` mediante `createObjectsFromString` y permite crear instantáneas de respaldo del nivel en disco (`backups/`).

---

## 2. Acceso desde el Editor y Estructura del Menú

Al abrir el editor de niveles de Geometry Dash (`EditorUI`), verás en la parte superior el **botón de DecoMind AI** (icono azul con martillo de construcción `resources/editor-btn.png`). Al pulsarlo se despliega el menú interactivo organizado en 5 zonas:

- **Barra Superior**:
  - **Botón volver (flecha verde neón)**: cierra el menú de DecoMind AI y regresa al editor.
  - **Mascota Robot central**: animada con flotación suave; al tocarla avanza a la siguiente frase de consejo del aliado.
  - **Cápsula de progreso superior y botón Diamante**: muestran de un vistazo la disponibilidad de tu cuota diaria y abren la pestaña de `ESTADÍSTICAS`.
- **Barra Lateral Izquierda (5 Pestañas)**:
  - Permite alternar entre **`INICIO`**, **`MODOS`**, **`CHAT`**, **`ESTADÍSTICAS`** y **`AJUSTES`**.
  - Debajo de los botones laterales se ubican los **picos decorativos de Geometry Dash con iluminación RGB dinámica**.
- **Contenedor Central**:
  - Muestra el contenido de la pestaña activa (la cuadrícula deslizante de los 9 modos en `INICIO`, la guía detallada en `MODOS`, el cuadro ampliado de comando en `CHAT`, las métricas en `ESTADÍSTICAS` o el panel de personalización en `AJUSTES`).
- **Columna Derecha**:
  - Agrupa los paneles de **`USO DE LA IA`**, **`CHAT / COMANDO`** e **`HISTORIAL`**.
- **Barra Inferior y Banner del Aliado Robot**:
  - Contiene los 5 accesos rápidos (**`FAVORITOS`**, **`TUTORIALES`**, **`MIS NIVELES`**, **`GUARDAR`**, **`REINICIAR`**) y a la derecha el **banner con contorno RGB, cabeza de robot animada y consejos que aparecen y desaparecen gradualmente (*fade-in / fade-out*)**.

---

## 3. Los 9 Modos Especializados de Construcción

En la pestaña **`INICIO`**, la cuadrícula central es **deslizante hacia abajo (`ScrollLayer`)** y presenta las 9 tarjetas de modos con **degradado interior suave** y **contorno RGB animado por su propia paleta cromática**:

| Modo | ID Interno | Color de Paleta | Función Principal |
| :--- | :--- | :--- | :--- |
| **1. ESTRUCTURAR** | `estructurar` | Verde turquesa (`#01d3ae`) | Organiza grupos, ordena capas (`Z-Order` / `Z-Layer`), limpia triggers redundantes y estructura secciones sin alterar colisiones. |
| **2. LAYOUT** | `layout` | Azul eléctrico (`#02a5fc`) | Crea únicamente la jugabilidad del nivel (bloques, picos, portales, pads y orbes) verificando que cada salto sea superable. |
| **3. DECO** | `deco` | Violeta magenta (`#b648f6`) | Añade y mejora la decoración visual respetando intactos los bloques sólidos y hitboxes existentes. |
| **4. LAYOUT + DECO** | `layout_deco` | Ámbar dorado (`#fbcf50`) | Construye la sección en dos pasadas: primero genera la estructura jugable y luego aplica decoración temática encima. |
| **5. BLOCK DESIGN** | `block_design` | Coral rojizo (`#f75475`) | Diseña bloques personalizados (bordes, esquinas, rellenos geométricos y pilares) manteniendo la forma jugable original. |
| **6. TRIGGERS** | `triggers` | Cian brillante (`#00e3f7`) | Configura automáticamente triggers (`Move`, `Pulse`, `Color`, `Alpha`, `Toggle`, `Spawn`) utilizando IDs de grupo libres. |
| **7. TRANSICIONES** | `transiciones` | Púrpura índigo (`#a94ef8`) | Crea transiciones visuales suaves y creativas entre dos zonas, cambios de color o cambios de portal. |
| **8. EFECTOS AVANZADOS** | `efectos` | Naranja intenso (`#fa9237`) | Añade efectos especiales, movimiento, parallax, partículas y pulsos sincronizados cuidando el límite de objetos. |
| **9. IDEAS Y SUGERENCIAS** | `ideas` | Azul cielo (`#009bf7`) | Modo 100 % consultivo: analiza tu nivel y devuelve ideas de ritmo, contraste y composición sin colocar ni alterar objetos. |

En la pestaña **`MODOS`** puedes consultar la guía ampliada de cada modo, recorrerlos con los botones `< Anterior` / `Siguiente >` y pulsar **`Cargar ejemplo en Chat`** para probar una instrucción recomendada.

---

## 4. Sistema de Chat / Comando y Reubicación Dinámica

El cuadro **`CHAT / COMANDO`** te permite escribir en lenguaje natural lo que deseas que DecoMind AI realice en tu nivel (por ejemplo: *"Haz una transición entre 2 zonas"* o *"Crea un layout de cubo de 20 segundos"*).

### Comportamiento inteligente al seleccionar la pestaña `CHAT`
- **Desde cualquier pestaña (`INICIO`, `MODOS`, `ESTADÍSTICAS`, `AJUSTES`)**: el cuadro `CHAT / COMANDO` se encuentra en la mitad de la columna derecha para que puedas enviar órdenes rápidas mientras ves la cuadrícula de modos.
- **Al pulsar la pestaña `CHAT` en la barra lateral izquierda**:
  1. El cuadro **`CHAT / COMANDO` se traslada y amplía automáticamente hacia el centro del menú**, brindándote un espacio cómodo y protagonista para redactar tus instrucciones y leer debajo el estado y la respuesta detallada de la IA.
  2. Simultáneamente, el cuadro **`HISTORIAL` sube en la columna derecha** para ocupar el espacio donde antes estaba ubicado el chat.
  3. Al regresar a cualquier otra pestaña, ambos paneles vuelven suavemente a su disposición habitual.

---

## 5. Panel de Uso de la IA, Historial y Herramientas Inferiores

### Panel `USO DE LA IA` (Arriba a la derecha)
Muestra en tiempo real:
- **Límite diario (`usadas / límite`)** junto con su barra de progreso verde-turquesa.
- **Peticiones realizadas**: total acumulado de consultas enviadas.
- **Nivel actual**: nombre real del nivel abierto en el editor (`LevelEditorLayer`).
- **Modo activo**: muestra `"Ninguno"` al abrir el menú o el nombre coloreado del modo que hayas seleccionado.

### Panel `HISTORIAL` y Ventana `HISTORIAL DE PETICIONES`
- El panel derecho `HISTORIAL` lista las 3 peticiones más recientes con su tiempo relativo (`Hace X min`).
- **Apertura exclusiva con el botón `>`**: para evitar aperturas accidentales al tocar el panel, la ventana emergente completa (`HISTORIAL DE PETICIONES`) se abre **únicamente presionando el botón `>`** situado a la derecha de cada fila.
- **Lista deslizante de solo lectura**: dentro de la ventana emergente `HISTORIAL DE PETICIONES`, los recuadros muestran el modo utilizado, la hora y el comando completo en modo de **solo lectura** (no clicables) dentro de un contenedor deslizante hacia abajo (`ScrollLayer`), acompañado del botón inferior **`BORRAR HISTORIAL`**.

### Barra Inferior de 5 Herramientas
1. **`FAVORITOS`**: abre la ventana de peticiones favoritas (`favoritos.txt`) para guardar el comando que tengas escrito en `CHAT / COMANDO`, cargar un favorito con un clic o eliminar favoritos antiguos.
2. **`TUTORIALES`**: abre el manual rápido integrado dentro del juego.
3. **`MIS NIVELES`**: muestra las estadísticas en vivo del nivel abierto en el editor (nombre y cantidad exacta de objetos) y la lista de puntos de guardado creados.
4. **`GUARDAR`**: registra inmediatamente una instantánea/punto de guardado del nivel actual en la carpeta `backups/` del mod.
5. **`REINICIAR`**: cancela cualquier petición en vuelo, limpia el cuadro de texto, restablece el modo activo a `Ninguno` y regresa a la vista `INICIO`.

---

## 6. Configuraciones, Paleta de Colores y Efectos RGB

En la pestaña **`AJUSTES`** dispones de 5 bloques organizados y espaciados:

1. **Origen de IA (`CAMBIAR ORIGEN`)**:
   - **Modo `LOCAL (Prueba sin red)`**: genera objetos de demostración directamente desde el mod sin necesidad de levantar el servidor Python ni tener conexión a internet.
   - **Modo `SERVIDOR HTTP (Proveedor IA)`**: envía tus peticiones al servidor (`Server/server.py`) en la URL configurada.
2. **URL del Servidor (`GUARDAR URL`)**:
   - Por defecto `http://127.0.0.1:8765/generate` (ideal si ejecutas el servidor en el mismo dispositivo mediante Termux en Android o en tu PC). Si el servidor corre en otro dispositivo de tu red local, coloca `http://IP_LOCAL:8765/generate`.
3. **Secciones a generar en modo Local (`-` / `+`)**:
   - Ajusta entre `1` y `40` secciones la longitud de prueba generada por el modo local.
4. **Paleta de colores del menú interactivo (`CAMBIAR PALETA`)**:
   - Abre la ventana **`PALETA DEL MENÚ INTERACTIVO`** con el selector de colores estilo Editor de Geometry Dash (`GDColorPickerPopup`).
   - Incluye **18 colores rápidos del editor de GD**, **barras deslizantes R, G, B (`0..255`)** con botones de ajuste fino (`-`, `+`) y rápido (`<<`, `>>`), **vista previa comparativa (`Original` vs `Nuevo`)**, código hexadecimal `#RRGGBB`, botón **`Color por defecto`** y botón **`Aplicar color`**.
5. **Animación y velocidad de bordes RGB (`ON / NORMAL`, `- / +`, `COLOR RGB`)**:
   - **Botón `ON / NORMAL`**: activa el ciclo dinámico de bordes RGB o lo desactiva para dejar todos los recuadros en su color estático normal.
   - **Botones `-` y `+` (`25%` a `300%`)**: regulan en tiempo real la velocidad de las animaciones RGB de las tarjetas de modos, los picos de GD y el banner del robot aliado.
   - **Botón `COLOR RGB`**: abre el selector de colores estilo Geometry Dash para aplicar un tono base personalizado a los efectos RGB (o restaurar los colores por paleta individuales de cada modo pulsando `Color por defecto`).

---

## 7. Arquitectura del Servidor y Proveedores de IA (`stub`, `groq` y personalizados)

El servidor incluido en `Server/server.py` utiliza únicamente la biblioteca estándar de Python 3 (no requiere instalar paquetes pesados con `pip`) y funciona tanto en **PC (Windows/Linux/macOS)** como en **Android (Termux)**.

### 7.1. Modo de prueba sin clave (`stub` - por defecto)
Si ejecutas el servidor directamente:
```bash
cd Server
python3 server.py
```
El servidor arranca en `http://0.0.0.0:8765/generate` usando el proveedor `stub` (`StubProvider`), que responde al instante con layouts de prueba y verifica la comunicación entre Geometry Dash y Python.

### 7.2. Conectar IA Real con Groq (`groq_provider.py` - Gratis y Rápido)
El proyecto ya incluye integrado el proveedor **`groq`** en `Server/mod_python_api/groq_provider.py`, que utiliza la API de Groq (compatible con OpenAI, modelo `llama-3.3-70b-versatile` por defecto):

1. Obtén una API Key gratuita en [console.groq.com](https://console.groq.com).
2. Crea un archivo llamado **`.env`** dentro de la carpeta `Server/` (`Server/.env`) con el siguiente contenido:
   ```env
   DECOMIND_PROVIDER=groq
   GROQ_API_KEY=gsk_tu_clave_aqui
   GROQ_MODEL=llama-3.3-70b-versatile
   GROQ_MAX_OBJS=300
   DAILY_LIMIT=50
   ```
   *(También puedes pasar las variables directamente por terminal: `DECOMIND_PROVIDER=groq GROQ_API_KEY=gsk_... python3 server.py`).*
3. Arranca el servidor:
   ```bash
   cd Server
   python3 server.py
   ```
4. En Geometry Dash, abre **DecoMind AI $\rightarrow$ AJUSTES**, cambia el origen a **`SERVIDOR HTTP`** y envía cualquier instrucción desde `CHAT / COMANDO`.

### 7.2.1. Catálogo de objetos (IDs que la IA puede usar)
La IA solo puede colocar objetos que existan en el catálogo; cualquier otro ID se descarta.

- `Server/data/catalog_curated.json`: IDs de uso común (bloque, picos, portales, velocidades, pads, orbes, triggers). Es editable: si un ID no coincide en tu versión del juego, corrígelo ahí.
- `Server/data/catalog_game.txt`: catálogo **completo** del juego (líneas `id|sprite`). Se genera solo: con el servidor activo y Origen = `server`, escribe **`/catalogo`** en `CHAT / COMANDO` y pulsa enviar. El mod lee la tabla interna de objetos de GD y la sube al servidor (no gasta peticiones de IA).
- Con el catálogo del juego, el servidor clasifica cada objeto por el nombre de su sprite (`solid`, `hazard`, `deco`, ...). Los modos de decoración solo pueden usar `deco`; los modos de layout pueden usar bloques, picos, portales, pads y orbes.
- Sin `catalog_game.txt` los modos de decoración no tienen IDs y la IA te pedirá escribir `/catalogo`.

Propiedades opcionales por objeto (`props`, validadas por rango): color 21/22, z-layer 24, z-order 25, grupo 57, grupo objetivo 51, mover 28/29, duración 10, canal 23, RGB 7/8/9, opacidad 35.

### 7.3. Añadir un Nuevo Proveedor de IA (OpenAI, Gemini, Claude, Ollama, etc.)
En `Server/mod_python_api/providers.py` tienes la clase base `TemplateProvider`:
1. Copia `TemplateProvider` o crea un archivo nuevo en `Server/mod_python_api/`.
2. Implementa el método `generate(self, req: ApiRequest) -> ApiReply`, donde:
   - `req.mode` es el identificador del modo (`layout`, `deco`, etc.).
   - `req.context` es el preprompt interno del modo enviado por el mod.
   - `req.level` es el nombre del nivel actual.
   - `req.prompt` es la instrucción escrita por el usuario.
3. Registra tu proveedor con `register_provider("mi-ia", MiProveedor)` y selecciónalo con `DECOMIND_PROVIDER=mi-ia`.

---

## 8. Validación de Seguridad sobre el Nivel (`LevelValidator`)

Para proteger siempre tu nivel en el editor, existen dos filtros de validación independientes (uno en `groq_provider.py` del lado del servidor y otro obligatorio en `src/LevelValidator.hpp` dentro del mod C++):

- **Rango de IDs de objeto**: solo se aceptan IDs válidos de Geometry Dash (`1 <= id <= 4600`).
- **Coordenadas seguras**: se descartan valores no numéricos (`NaN`/`Inf`) o fuera de los límites del lienzo del editor (`-240 <= x <= 240000`, `-240 <= y <= 6000`).
- **Protección de jugabilidad por modo**: en modos puramente decorativos (`deco`, `transiciones`, `efectos`, `triggers`), el proveedor filtra picos y portales peligrosos para no arruinar tu gameplay, y en el modo `ideas` se bloquea cualquier inserción de objetos.
- **Catálogo cerrado**: el servidor solo acepta IDs del catálogo y solo de las categorías que el modo permite. El mod repite el filtro de picos, portales, velocidades, pads y orbes en los modos que no son de layout.
- **Límite por lote**: máximo `2000` objetos validados por operación en el cliente (`300` por defecto en `GroqProvider`) para evitar congelamientos del editor.

---

## 9. Estructura de Archivos del Proyecto

```text
DecoMind-AI-main/
├── .github/workflows/main.yml      # Compilación automática multiplataforma (Windows, Android32, Android64)
├── CMakeLists.txt                  # Configuración de compilación CMake + Geode SDK
├── mod.json                        # Manifiesto Geode v5.11.2 (versión v0.3.2-alpha, recursos y ajustes)
├── logo.png                        # Logo oficial del mod en formato cuadrado 512x512 RGBA
├── About.md                        # Descripción corta para el visor de mods de Geode
├── CHANGELOG.md                    # Registro histórico detallado de todas las versiones
├── README.md                       # Este manual completo de usuario y arquitectura
├── resources/                      # 26 sprites PNG RGBA en alta resolución empaquetados por Geode
│   ├── editor-btn.png              # Icono del botón de DecoMind AI en la barra superior del editor
│   ├── decomind-mascot.png         # Mascota robot superior con destello
│   ├── decomind-icon.png           # Cabeza del robot aliado para el banner inferior
│   ├── mode-*.png                  # 9 iconos HD (640x512) de los modos de construcción
│   ├── sb-*.png                    # 5 iconos HD de la barra lateral izquierda
│   ├── bb-*.png                    # 5 iconos HD de la barra de herramientas inferior
│   └── hd-*.png / ui-*.png         # Iconos HD de volver, diamante, avión de envío y reloj
├── src/                            # Código fuente C++20 modular del mod
│   ├── main.cpp                    # Menú principal, animaciones, selector de color GD y ventanas modales
│   ├── UIRenderer.hpp              # Motor de dibujo Cocos2d-x, configuración de tema y efectos RGB
│   ├── Modes.hpp                   # Definición de los 9 modos (ID, texto visible y preprompt interno)
│   ├── AIClient.hpp                # Cliente asíncrono HTTP y generador local
│   ├── LevelValidator.hpp          # Validador de seguridad de objetos y coordenadas de GD
│   ├── LevelExecutor.hpp           # Inspector del nivel, ejecutor de objetos y gestor de respaldos
│   └── HistoryManager.hpp          # Persistencia de historial.txt, favoritos.txt y frases.txt
└── Server/                         # Servidor ligero en Python 3 (PC / Termux)
    ├── server.py                   # Servidor HTTP principal (puerto 8765 por defecto)
    └── mod_python_api/             # Paquete modular de integración con IA
        ├── __init__.py             # Exportación de la API del paquete
        ├── protocol.py             # Parseo de peticiones y formato de respuesta para el mod
        ├── providers.py            # Registro de proveedores, StubProvider y TemplateProvider
        ├── groq_provider.py        # Proveedor real conectado a la API de Groq (Llama 3.3 70B)
        └── quota.py                # Control de límite diario de peticiones
```
