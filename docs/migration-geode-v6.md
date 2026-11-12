# DecoMind v0.4.1-alpha: Geode 6 y prueba con Groq

Objetivo fijado el 12/11/2026: **Geode v6.0.0-alpha.1 / Geometry Dash 2.2091**. Es la última alpha numerada publicada al consultar las releases oficiales; nightly es un canal distinto. SDK y manifiesto quedan fijados a esa alpha para reproducir los builds.

- Release oficial: https://github.com/geode-sdk/geode/releases/tag/v6.0.0-alpha.1
- Migración oficial: https://github.com/geode-sdk/docs/blob/main/tutorials/migrate-v6.md

## Cambios

Manifiesto, CI y versión de producto actualizados. Los accesos a directorios de Geode usan el puente UTF-8 `asp::fs` ↔ `std::filesystem`; el núcleo de backups/índice conserva su manejo de errores y renombrado atómico. Las pruebas usan Result 1.5.0 y matjson 4.0.1, igual que el SDK de esta alpha. No se modifican los archivos fuente del SDK.

Esta build requiere Geode 6 y GD 2.2091. El paquete anterior para Geode 5 / GD 2.2081 permanece como artefacto histórico. Cambiar la versión del loader no amplía el catálogo ni resuelve por sí mismo la calidad de generación; esa integración sigue pendiente del ZIP.

## Prueba de Groq dentro del juego

1. Instala GD 2.2091 y Geode 6.0.0-alpha.1, después el paquete nuevo de DecoMind v0.4.1-alpha.
2. En los ajustes del mod de Geode selecciona `ai-provider = groq`. Deja `ai-model` vacío para usar `openai/gpt-oss-120b`, o indica otro modelo disponible en tu cuenta. El viejo valor predeterminado `gpt-4.1-mini` se resuelve al modelo de Groq al elegir ese proveedor; los modelos personalizados se conservan.
3. Abre DecoMind → Ajustes. Pega la clave en el campo enmascarado y pulsa GUARDAR: activa API personal. No introduzcas claves en el chat del mod, archivos del repositorio, issues ni mensajes de ChatGPT.
4. Pulsa PROBAR. Hace GET /models al proveedor elegido, comprueba acceso y presencia del modelo sin enviar geometría del nivel ni solicitar una generación. La disponibilidad de un modelo no garantiza que la siguiente generación vaya a completarse.
5. Selecciona una pequeña estructura y prueba una instrucción decorativa. Revisa la propuesta antes de aceptar. La generación usa la cuota y condiciones de tu cuenta de Groq. Los IDs habilitados siguen limitados al catálogo inicial.
6. Campo vacío + GUARDAR borra la clave. La clave solo dura la sesión del proceso; al reiniciar el juego debes introducirla de nuevo. Cambiar de proveedor no reenvía la clave del anterior.

Se solicita salida JSON en las generaciones. No hay fallback de pago, redirecciones HTTP ni reintentos automáticos. La prueba en vivo con tu clave se realiza en tu dispositivo; la clave no se ha compartido ni usado en CI.

Modelos y acceso vigentes: https://console.groq.com/docs/models

## Complementos posibles más adelante

- **Node IDs** (`geode.node-ids`): localizar controles del editor por nombres y mejorar convivencia con otros mods. Su manifiesto actual declara Geode 6 / GD 2.2091. Candidato para integrar la UI del editor si se usan esos nodos; no hace falta para HTTPS o IA.
- **DevTools** (`geode.devtools`): inspección del árbol de nodos y depuración visual, instalado por el desarrollador, no como requisito del usuario. Verificar una build compatible antes de instalar: el manifiesto consultado todavía declara Geode 5.11.2.
- **GMD API** (`hjfod.gmd-api`): importación/exportación de fixtures .gmd. El manifiesto consultado declara Geode 5 / GD 2.2081: esperar una versión compatible antes de añadirla a esta alpha.
- **Atajos**: evaluar primero la API integrada del SDK 6 y los controles nuevos de GD. No arrastrar Custom Keybinds antiguo sin comprobar su compatibilidad.

No se añaden dependencias externas obligatorias en esta migración. Geode ya suministra HTTPS, JSON, controles y APIs de objetos/undo. Ningún complemento reemplaza el catálogo, la planificación contextual ni las pruebas reales de reversión.

Fuentes de los complementos:
- https://github.com/geode-sdk/NodeIDs
- https://github.com/geode-sdk/DevTools
- https://github.com/HJfod/GMD-API
- https://github.com/geode-sdk/CustomKeybinds
