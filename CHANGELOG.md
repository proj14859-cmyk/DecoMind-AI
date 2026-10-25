# Registro de Cambios (Changelog) - DecoMind AI

Todas las modificaciones notables realizadas en este proyecto están documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/) y este proyecto se adhiere a [Semantic Versioning](https://semver.org/lang/es/).

---
## [0.3.11] - 2026-10-24
- soporte para proveedores de AI mediante Api

## [0.3.0] - 2026-10-23

### Rediseño Visual Completo (Fidelidad 1:1 a la Referencia)
- **Marco Sci-Fi y Lienzo Nativo**:
  - Implementación de mapeo proporcional exacto `1536x1024` $\rightarrow$ `480x320` (`SCALE_K = 0.3125f` en X e Y) para garantizar proporciones 1:1 en el sistema de coordenadas de Cocos2d-x / Geometry Dash.
  - Marco exterior con esquinas biseladas sci-fi, fondo azul marino profundo (`#00093d` / `#000b32`), líneas arquitectónicas y picos decorativos de Geometry Dash con líneas cian en la esquina inferior izquierda.
- **Header Superior**:
  - **Botón volver**: botón verde neón con doble triángulo y borde luminoso menta para cerrar el menú.
  - **Botón de ajustes superior**: engranaje cian dentro de una caja redondeada azul marino que conmuta directamente a la vista de ajustes.
  - **Mascota robot DecoMind**: integrada en el centro superior sobre el borde del marco, con ojos cian `^ ^`, auriculares, destello de 4 puntas (`✦`) y animación suave de flotación.
  - **Cápsula superior de progreso**: cápsula horizontal con destello y barra cian-turquesa sincronizada con la disponibilidad de la cuota diaria.
  - **Botón de diamante**: gema facetada azul/cian en la esquina superior derecha con acceso directo a estadísticas.
- **Cuadrícula 3x3 de los 9 Modos (Pestaña INICIO)**:
  - Reemplazo de la antigua lista deslizante vertical por una cuadrícula uniforme de 3 filas $\times$ 3 columnas con tarjetas redondeadas.
  - Colores de fondo y bordes neón idénticos a la referencia:
    - `ESTRUCTURAR`: Verde azulado / turquesa (`#012e3c` / `#01d3ae`).
    - `LAYOUT`: Azul eléctrico (`#031d64` / `#02a5fc`).
    - `DECO`: Violeta / magenta (`#280c66` / `#b648f6`).
    - `LAYOUT + DECO`: Ámbar dorado (`#30200e` / `#fbcf50`).
    - `BLOCK DESIGN`: Coral rojizo (`#3f1132` / `#f75475`).
    - `TRIGGERS`: Cian brillante (`#013457` / `#00e3f7`).
    - `TRANSICIONES`: Índigo / púrpura (`#190e5b` / `#a94ef8`).
    - `EFECTOS AVANZADOS`: Naranja intenso (`#3b1b17` / `#fa9237`).
    - `IDEAS Y SUGERENCIAS`: Azul cielo / zafiro (`#011451` / `#009bf7`).
  - Cada tarjeta incluye su icono correspondiente (sprite empaquetado + respaldo vectorial `CCDrawNode`), título en mayúsculas en blanco y descripción visible de 2 líneas.
  - Resaltado visual activo con borde luminoso doble e indicador al seleccionar un modo.
- **Columna Derecha (3 Paneles Apilados)**:
  - **Uso de la IA**: estrella cian, indicador de límite diario (`used / limit`), barra de progreso redondeada verde-cian (`#28f2ba`), contador de peticiones realizadas, nombre del nivel actual leído de `LevelEditorLayer` y modo activo.
  - **Chat / Comando**: caja de texto con placeholder multilínea exacto de la referencia (`Escribe lo que quieres que haga en tu nivel...`), campo interactivo con soporte de texto multilínea y botón de envío redondeado azul con icono de avión de papel (`➤`).
  - **Historial**: encabezado con icono de reloj y 3 filas interactivas con puntos azules (`●`), resumen de la acción, tiempo relativo (`Hace X min`) y flecha `>`, pulsables para recargar la petición.
- **Barra Inferior y Banner del Aliado**:
  - Barra de 5 compartimentos con iconos y etiquetas: `FAVORITOS`, `TUTORIALES`, `MIS NIVELES`, `GUARDAR` y `REINICIAR`.
  - Banner inferior derecho del robot aliado con cabeza del robot, texto rotatorio y estrella cian exterior.

### Arquitectura Modular en C++ (Separación de Responsabilidades)
- **`src/Modes.hpp`**:
  - Separación estricta de los 3 componentes por modo:
    1. Identificador interno (`id`).
    2. Descripción visible para el usuario (`visibleLine1`, `visibleLine2`, `userGuide`).
    3. Preprompt interno para el modelo (`internalPrompt`), que nunca se expone en la interfaz visual.
  - Función `sanitizeForGD()` que translitera tildes y caracteres especiales a caracteres compatibles con las fuentes bitmap de Geometry Dash (`bigFont.fnt` y `chatFont.fnt`) evitando letras omitidas.
- **`src/UIRenderer.hpp`**:
  - Motor de renderizado vectorial nativo sobre `CCDrawNode`, `CCSprite` y `CCLabelBMFont`.
  - Conversores de coordenadas de referencia `px()`, `py()`, `ps()`.
  - Funciones de dibujo de rectángulos redondeados con bordes y resplandores suaves (`drawRoundedRect`, `addRefRoundedBox`).
  - Dibujo procedural de respaldo para todos los iconos de la interfaz (modos, barra lateral, barra inferior, encabezado y mascota).
- **`src/AIClient.hpp`**:
  - Desacoplamiento de la red y el proveedor de IA.
  - Soporte de cancelación (`cancel()`) y control de estado ocupado (`isBusy()`).
  - Inyección del preprompt interno (`internalPrompt`) exclusivamente en la carga útil de la petición HTTP.
  - Manejo de respuestas honestas cuando el servidor o proveedor externo está pendiente de configuración.
- **`src/LevelValidator.hpp`**:
  - Validación de seguridad de objetos y parámetros generados antes de aplicarlos al nivel.
  - Comprobación de rangos de ID de objeto (`1..4600`), límites de coordenadas `X/Y`, escalas válidas y tope de objetos por lote (`MAX_OBJECTS_PER_BATCH = 2000`).
  - Restricción estricta en el modo `ideas` (bloqueo total de modificaciones en el nivel).
- **`src/LevelExecutor.hpp`**:
  - Consulta segura de metadatos del nivel en `LevelEditorLayer` (título y número de objetos).
  - Sistema de puntos de guardado y respaldos en la carpeta `backups/` del mod.
  - Inserción validada de objetos mediante `createObjectsFromString`.
- **`src/HistoryManager.hpp`**:
  - Persistencia y carga de `historial.txt` con marcas de tiempo UNIX para calcular tiempos relativos reales.
  - Persistencia de `favoritos.txt` con comandos frecuentes.
  - Carga y rotación de frases para el aliado desde `frases.txt`.

### Nuevas Ventanas Secundarias (Modales)
- **`HistoryPopup`**: ventana modal para explorar el historial completo de peticiones, reutilizar comandos anteriores con un clic o vaciar el historial.
- **`FavoritesPopup`**: permite guardar el comando actual como favorito, listar favoritos guardados con acceso rápido y eliminarlos.
- **`TutorialsPopup`**: guía interactiva sobre el flujo de trabajo en el editor, descripción de los 9 modos y configuración del servidor.
- **`LevelsBackupPopup`**: visualización de metadatos del nivel activo y gestión de instantáneas de respaldo guardadas.

### Servidor Backend (`Server/server.py`)
- Compatibilidad dual en `parse_request_body`: soporta tanto el formato estructurado con cabeceras `MODE:`, `CONTEXT:`, `LEVEL:`, `PROMPT:` como el formato legado de 2 líneas.
- Soporte para peticiones `GET` para verificar el estado de salud y cuota del servidor.

### Recursos Gráficos y Configuración
- Creación y registro de 11 sprites transparentes en `resources/`:
  - `decomind-mascot.png`: mascota robot superior con destello.
  - `decomind-icon.png`: cabeza del robot para el banner inferior.
  - 9 iconos de modo: `mode-estructurar.png`, `mode-layout.png`, `mode-deco.png`, `mode-layout-deco.png`, `mode-block-design.png`, `mode-triggers.png`, `mode-transiciones.png`, `mode-efectos.png`, `mode-ideas.png`.
- Actualización de `mod.json` para empaquetar `"resources": { "sprites": ["resources/*.png"] }`.

---

## [0.2.0] - Versión Anterior
- Prototipo inicial de menú con lista deslizante vertical de modos.
- Servidor Python básico de prueba con cuota diaria.
- Botón "IA" en `EditorUI`.
