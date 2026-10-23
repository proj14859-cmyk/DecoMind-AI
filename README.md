# DecoMind AI

Asistente interactivo con IA integrado de forma nativa en el editor de **Geometry Dash** (Geode SDK `v5.11.2`).

## Estructura del Proyecto

- `src/main.cpp`: Menú interactivo principal (`DecoMindPopup`), barra superior, menú lateral de 5 secciones (`INICIO`, `MODOS`, `CHAT`, `ESTADÍSTICAS`, `AJUSTES`), cuadrícula central 3x3 de los 9 modos, paneles laterales (`USO DE LA IA`, `CHAT / COMANDO`, `HISTORIAL`), barra inferior de herramientas (`FAVORITOS`, `TUTORIALES`, `MIS NIVELES`, `GUARDAR`, `REINICIAR`), banner del aliado y ventanas secundarias.
- `src/Modes.hpp`: Definición de los 9 modos (`Estructurar`, `Layout`, `Deco`, `Layout + Deco`, `Block Design`, `Triggers`, `Transiciones`, `Efectos Avanzados`, `Ideas y Sugerencias`), separando su identificador interno, descripción visible al usuario y preprompt interno para el modelo de IA.
- `src/UIRenderer.hpp`: Motor de renderizado nativo en Cocos2d-x (`CCDrawNode`, `CCSprite`, `CCLabelBMFont`) con conversión proporcional 1:1 desde el diseño de referencia (`1536x1024` -> `480x320`) y dibujo vectorial de respaldo para todos los iconos y la mascota robot.
- `src/AIClient.hpp`: Capa de comunicación desacoplada para modo local de prueba y servidor HTTP (`Server/server.py`).
- `src/LevelValidator.hpp`: Validación de seguridad de objetos y coordenadas antes de modificar el nivel.
- `src/LevelExecutor.hpp`: Inspección del nivel en `LevelEditorLayer`, creación de copias/puntos de guardado (`backups/`) e inserción validada de objetos.
- `src/HistoryManager.hpp`: Persistencia de `historial.txt`, `favoritos.txt` y `frases.txt`.
- `resources/*.png`: Sprites transparentes del robot aliado y de los 9 modos declarados en `mod.json`.
- `Server/server.py`: Servidor HTTP en Python compatible con Termux y PC.
