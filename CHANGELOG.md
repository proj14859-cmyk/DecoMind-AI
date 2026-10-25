# Registro de Cambios (Changelog) - DecoMind AI

Todas las modificaciones notables realizadas en este proyecto están documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/) y este proyecto se adhiere a [Semantic Versioning](https://semver.org/lang/es/).

---

## [0.3.12] - 2026-10-25

### Rediseño Interactivo, Efectos RGB y Nuevos Recursos Visuales
- **Nuevo botón del editor (`resources/editor-btn.png`) y nuevo logo oficial (`logo.png`)**:
  - Conversión de ambos recursos a formato PNG `RGBA` de 32 bits con canal alfa limpio y dimensiones estándar (`256x256` para `editor-btn.png` y `512x512` para `logo.png`).
  - Reemplazo del antiguo botón `"IA"` en `EditorUI` por el nuevo botón gráfico `editor-btn.png`.
- **Cuadrícula central de modos deslizante hacia abajo (`INICIO`)**:
  - Integración de la cuadrícula `3x3` de los 9 modos dentro de un `ScrollLayer` vertical fluido con espacio inferior reservado para futuras funciones y herramientas.
- **Contornos RGB por paleta de colores y degradado interior en tarjetas de modos**:
  - Cada una de las 9 tarjetas anima su contorno dinámicamente dentro de su propia familia cromática (`shiftPaletteColor`: tonos ámbar/naranja en tarjetas naranjas, tonos celestes/azules en tarjetas azules, tonos violeta/magenta en tarjetas moradas, etc.).
  - Añadido degradado interior escalonado hacia un tono más claro de la paleta de cada tarjeta (`drawRoundedRectWithSoftGradient`) para aportar mayor profundidad visual.
- **Decoración RGB en picos de Geometry Dash y banner del robot aliado**:
  - Los picos decorativos y líneas de la esquina inferior izquierda cuentan ahora con animación dinámica RGB (`redrawSpikesRgb`).
  - El banner inferior derecho del aliado incorpora contorno RGB dinámico, animación continua de flotación y pulso en el icono de la cabeza del robot (`m_allyRobotNode`) y transición gradual de aparición y desaparición (*fade-in / fade-out*) para las frases de `frases.txt`.
- **Reubicación dinámica de paneles al seleccionar `CHAT`**:
  - Eliminada la sección antigua estática de conversación en el centro.
  - Al pulsar la pestaña `CHAT` en la barra lateral, el cuadro `CHAT / COMANDO` se traslada y amplía hacia el contenedor central, mientras el cuadro `HISTORIAL` sube automáticamente en la columna derecha para ocupar el espacio que dejó libre el chat. Al cambiar a cualquier otra pestaña, ambos paneles regresan a su ubicación original.
- **Mejoras en el panel y ventana emergente de `HISTORIAL`**:
  - La ventana emergente `HistoryPopup` se abre ahora **únicamente presionando el botón `>`** situado a la derecha de las filas del panel `HISTORIAL`.
  - Dentro de `HistoryPopup` (`HISTORIAL DE PETICIONES`), se eliminó la interacción clicable sobre los recuadros individuales para que sean tarjetas informativas de solo lectura dentro de un menú deslizante (`ScrollLayer`) fluido hacia abajo.
- **Rediseño completo de la pestaña `AJUSTES` y selector de color estilo Editor de Geometry Dash (`GDColorPickerPopup`)**:
  - Reemplazo de los botones grandes por defecto de GD que se solapaban con el texto por botones estilizados nativos (`ui::makeCustomMenuBtn`) distribuidos en 5 tarjetas horizontales limpias.
  - Eliminado el botón de recarga manual de `frases.txt`.
  - Añadida opción para cambiar la **paleta de colores del menú interactivo** en tiempo real.
  - Añadidos controles para activar/desactivar el efecto de los **recuadros RGB** (`ACTIVO` / `NORMAL`), regular su **velocidad** (`25%` a `300%`) y elegir su **color base**.
  - Implementada la ventana `GDColorPickerPopup` inspirada en el selector de colores oficial del editor de Geometry Dash (18 presets rápidos de GD, barras deslizantes R/G/B `0..255`, comparador `Original` vs `Nuevo`, código hexadecimal `#RRGGBB` y botón de restauración por defecto).
- **Documentación completa**:
  - `README.md` reescrito íntegramente como manual detallado de arquitectura, modos, chat, personalización y proveedores de IA (`stub`, `groq` y plantilla para nuevos proveedores).

---

## [0.3.11] - 2026-10-24
- Soporte para proveedores de IA mediante API (`Server/mod_python_api/groq_provider.py` con soporte para `GROQ_API_KEY`, `GROQ_MODEL`, `GROQ_MAX_OBJS` y carga automática de `Server/.env`).
- Paquete modular `Server/mod_python_api/` (`protocol.py`, `providers.py`, `quota.py`) e iconos HD (`resources/*.png`).

---

## [0.3.0] - 2026-10-23

### Rediseño Visual Completo (Fidelidad 1:1 a la Referencia)
- **Marco Sci-Fi y Lienzo Nativo**:
  - Implementación de mapeo proporcional exacto `1536x1024` $\rightarrow$ `480x320` (`SCALE_K = 0.3125f` en X e Y) para garantizar proporciones 1:1 en el sistema de coordenadas de Cocos2d-x / Geometry Dash.
  - Marco exterior con esquinas biseladas sci-fi, fondo azul marino profundo (`#00093d` / `#000b32`), líneas arquitectónicas y picos decorativos de Geometry Dash con líneas cian en la esquina inferior izquierda.
- **Header Superior**:
  - **Botón volver**: botón verde neón con doble triángulo y borde luminoso menta para cerrar el menú.
  - **Mascota robot DecoMind**: integrada en el centro superior sobre el borde del marco, con ojos cian `^ ^`, auriculares, destello de 4 puntas (`✦`) y animación suave de flotación.
  - **Cápsula superior de progreso**: cápsula horizontal con destello y barra cian-turquesa sincronizada con la disponibilidad de la cuota diaria.
  - **Botón de diamante**: gema facetada azul/cian en la esquina superior derecha con acceso directo a estadísticas.
- **Cuadrícula 3x3 de los 9 Modos (Pestaña INICIO)**:
  - Reemplazo de la antigua lista deslizante vertical por una cuadrícula uniforme de 3 filas $\times$ 3 columnas con tarjetas redondeadas.
- **Columna Derecha (3 Paneles Apilados)**:
  - Paneles `USO DE LA IA`, `CHAT / COMANDO` e `HISTORIAL`.
- **Barra Inferior y Banner del Aliado**:
  - Barra de 5 compartimentos (`FAVORITOS`, `TUTORIALES`, `MIS NIVELES`, `GUARDAR`, `REINICIAR`) y banner inferior derecho del robot aliado.
- **Arquitectura Modular en C++**:
  - `src/Modes.hpp`, `src/UIRenderer.hpp`, `src/AIClient.hpp`, `src/LevelValidator.hpp`, `src/LevelExecutor.hpp` y `src/HistoryManager.hpp`.

---

## [0.2.0] - Versión Anterior
- Prototipo inicial de menú con lista deslizante vertical de modos.
- Servidor Python básico de prueba con cuota diaria.
- Botón "IA" en `EditorUI`.
