# DecoMind-AI — Plan maestro de rediseño estructural e integración híbrida

**Documento:** Especificación técnica, arquitectura, migración y pruebas  
**Versión del plan:** 1.1 — 12/11/2026  
**Objetivo de producto:** DecoMind-AI v0.4.0-alpha y evolución posterior  
**Proyecto principal:** [proj14859-cmyk/DecoMind-AI](https://github.com/proj14859-cmyk/DecoMind-AI)  
**Referencia de integración:** [entity12208/EditorAI](https://github.com/entity12208/EditorAI)  
**Base de contenido:** `GD_Editor_Catalogo.zip`, suministrado por el propietario del mod  
**Estado:** propuesta de implementación; no equivale a código integrado, compilado o probado.

---

## 0. Resumen ejecutivo

DecoMind-AI debe pasar de un prototipo alpha con interfaz funcional, ejecución limitada y servidor Python/Termux opcional a un **asistente de creación y decoración dentro de Geometry Dash**, con ejecución nativa desde Geode, previsualización reversible, catálogo verificado y acceso a IA sin pasos obligatorios de configuración.

**Principios no negociables:**

1. **Conservar la identidad visual.** No reemplazar la UI existente por ImGui ni copiar la apariencia de EditorAI. Mantener fuentes, colores, tema, páginas, botón flotante, mascota y comportamiento de navegación salvo cambios funcionales expresamente especificados.
2. **Termux no es requisito.** El mod deberá poder hacer solicitudes HTTPS desde C++/Geode directamente al proveedor seleccionado. El servidor `Server/` anterior será legado, no dependencia de ejecución.
3. **Sin clave obligatoria al iniciar.** Ofrecer una ruta gratuita sin cuenta *si el servicio comunitario la permite y está disponible*. El modo de API personal será opcional, configurable en Ajustes.
4. **No confundir gratuito con offline.** Una red comunitaria de IA sigue siendo remota; requiere Internet y tiene posibles límites, colas o indisponibilidad. El modo sin Internet será un generador por reglas, identificado como tal.
5. **Nunca editar a ciegas.** Separar propuesta, validación, previsualización, aceptación y reversión. Respetar el gameplay existente salvo autorización explícita.
6. **No depender de `.gmd` para ejecutar.** El mecanismo principal será crear objetos mediante el editor abierto. Los `.gmd` servirán para pruebas, respaldo/intercambio y diagnósticos, no como único motor.
7. **Catálogo con confianza explícita.** Las ~4.083 entradas catalogadas no implican que 4.083 configuraciones estén verificadas. Distinguir ID conocido, sprite encontrado, parámetros comprobados, ejemplar `.gmd` y validación en juego.
8. **Diseño gradual.** El primer hito funcional es insertar objetos, previsualizarlos, rechazarlos y dejar el nivel exactamente como estaba; luego integrar IA, decoración y sincronización.
9. **Ejemplos JSONC como conocimiento visual.** Aprovechar la colección de EditorAI como referencias seleccionables de estructuras y estilos, sin confundir ejemplos parciales con niveles completos o copiar indiscriminadamente diseños ajenos.

### 0.1 Alcance decidido

**Incluido en el rediseño:** arquitectura modular C++/Geode, editor bridge, catálogo, validación, operaciones estructuradas, gestor de previsualización y transacciones, proveedores HTTPS, configuración de API keys, agente coordinador/roles, sesiones, audio y sincronización en fases, telemetría opt-in y pruebas Android.

**No incluido como requisito de v0.4 inicial:** garantizar proveedores gratuitos ilimitados; funcionar con un gran LLM offline dentro del móvil; soportar todos los triggers/cámaras al primer lanzamiento; obtener automáticamente una clave de un servicio de terceros; garantizar niveles verificados por juego sin playtest.

---

## 1. Estado actual y evaluación técnica

### 1.1 DecoMind-AI v0.3.8-alpha

Archivos revisados en el repositorio principal:

| Archivo/carpeta | Responsabilidad existente | Tratamiento |
|---|---|---|
| `src/main.cpp` | Popups y UI principal; coordinación del mod | Conservar UX; extraer orquestación en adaptadores nuevos |
| `src/UIRenderer.hpp` | Componentes, estilos y render visual | Conservar |
| `src/EditorButton.hpp` | Botón flotante y eventos táctiles | Conservar |
| `src/OfficialColorPickerPopup.hpp` | Selector de color oficial | Conservar |
| `src/Modes.hpp` | Nueve modos y prompts | Conservar, revisar contrato de salida |
| `src/AIClient.hpp` | Modo local, servidor Python HTTP, contexto/editor | Refactor a gestor de proveedores sin romper API de la UI |
| `src/LevelExecutor.hpp` | Escaneo de selección/vista/grupos/colores y generación por reglas | Dividir en percepción, operaciones y ejecución |
| `src/LevelValidator.hpp` | Validación de claves GD, límites y objetos | Rehacer a nivel de esquemas por tipo de objeto |
| `src/ObjectCatalog.hpp` | Catálogo interno de IDs y sprites (`ObjectToolbox`) | Fusionar con catálogo verificado |
| `src/HistoryManager.hpp` | Historial y estadísticas | Conservar; enlazar con sesiones/operaciones |
| `Server/` | Servidor Python y lógica de Groq/estilos | Dejar como legado opcional, sin necesidad de Termux |
| `resources/` | Sprites, iconos, tipografías usadas | Conservar |
| `CMakeLists.txt` | Compilación C++20 | Ajustar solo según dependencias realmente importadas |
| `mod.json` | Identidad, GD 2.2081, Geode 5.11.2, ajustes | Mantener ID `thedaek.decomind-ai`; migrar settings |

**Hallazgo crítico:** en la versión revisada, `saveLevelSnapshot()` escribe un `.txt` con nombre, fecha y número de objetos, **sin datos restaurables**. La función no debe anunciarse como copia recuperable. Es prioritario crear transacciones reversibles y un respaldo completo validado antes de habilitar edición masiva.

**Hallazgo de calidad:** el generador contextual usa varios IDs y cuadrículas fijas. Debe transformarse en un generador consciente de la geometría real, colisiones, estilo, grupos, capas, música y selección.

### 1.2 EditorAI — componentes de referencia

Repositorio público: `entity12208/EditorAI` (al revisar, manifiesto `v2.1.12`). La licencia del repositorio es **MIT**. Se verificaron la licencia, README, manifiesto, estructura de fuentes y fragmentos de código.

| Recurso | Uso recomendado | Precaución |
|---|---|---|
| `src/main.cpp` | Estudiar ejecución nativa, herramientas, generación por lotes, audio y preview | Archivo monolítico ~1,14 MB; extraer funciones aisladas, no copiarlo entero |
| `src/sessions.hpp` | Modelo de sesión, historial, estado, persistencia y acciones de seguimiento | Adaptar al historial y UI de DecoMind |
| `src/design_knowledge.hpp` | Guías de composición, gameplay, sync y uso responsable de IDs | Reutilizar como conocimiento de prompts, no como validador |
| `src/overlay.cpp` | Referencia de UX y configuración de proveedores | **No importar la UI ImGui**: la UI de DecoMind es la oficial del proyecto |
| `resources/object_ids.json` | Nombres semánticos de IDs de objeto | Contrastar con catálogo del usuario y versión de GD |
| `resources/example_levels/*.jsonc` y `example_sections.json` | Ejemplos de generaciones/estilos | No asumir que todos los ejemplos son compatibles con todas las versiones |
| `mod.json` | Configuración de proveedores y opciones de generación | Seleccionar subconjunto necesario, evitar copiar credenciales o valores internos |
| `CMakeLists.txt` | Referencia de dependencias y Android NDK | Comparar toolchains; evitar ImGui si no se usa |

EditorAI documenta: generación en nivel actual/nuevo/seleccionado, colocación progresiva, capa de previsualización (aceptar/editar/rechazar), sesiones, múltiples proveedores, BPM y waveform, herramientas y roles auxiliares. Estos elementos son **capacidades declaradas** del proyecto, no evidencia de que hayan sido probados en DecoMind.

### 1.3 Compatibilidad declarada

- DecoMind: C++20, Geode 5.11.2, GD 2.2081 según `mod.json`.
- EditorAI: C++23, Geode 5.10.1, GD 2.2081 según `mod.json`.
- **Decisión:** mantener C++20 por defecto. Adaptar el código reutilizado si es posible; escalar el estándar solo ante dependencia imprescindible documentada.
- **Objetivo prioritario:** Android/arm64 en el juego y Geode concretos que usa el proyecto, sin excluir otras plataformas por arquitectura.
- Ninguna coincidencia de versión declarada sustituye compilación real, verificación de hooks, pruebas en dispositivo y validación de API.

---

## 2. Experiencia de producto y UI — preservar el diseño

### 2.1 Páginas que no deben rediseñarse visualmente

Conservar el sistema actual de Inicio, Información/Modos, Chat, Estadísticas, Ajustes, paleta cromática y componentes auxiliares. Mantener el botón flotante, iconografía y animaciones ya establecidas. Si se añade una opción nueva, deberá integrarse en el lenguaje visual existente y no introducir una segunda UI paralela.

### 2.2 Ajustes de IA: reemplazo funcional del antiguo Local/Servidor

**Opciones del selector:**

| Modo | Comportamiento | Requiere configuración | Internet |
|---|---|---|---|
| **Automático — predeterminado** | Selecciona primero un proveedor gratuito integrado y disponible | No, si hay un servicio comunitario autorizado y operativo | Sí para IA remota |
| **Gratuito** | Permite elegir manualmente entre servicios sin clave compatibles | No para proveedores sin cuenta; otros pueden requerir inicio de sesión | Sí |
| **API personal** | Selecciona proveedor, modelo y clave personal | Sí, la clave del usuario | Sí |
| **Sin conexión (opcional)** | Generador por reglas/catálogo; no fingir que es LLM | No | No |

**Configuración visible propuesta:**

- Modo de IA y proveedor activo; estado real (conectado, ocupado, sin servicio, límite, error) sin promesas de disponibilidad.
- En automático: orden de proveedores **solo autorizados**, con fallback seguro; nunca enviar silenciosamente una petición a un servicio de pago.
- En gratuito: lista de proveedores habilitados con condiciones de uso, privacidad, límites y conexión.
- En API personal: proveedor (OpenAI, Groq, Gemini, OpenRouter, DeepSeek, otros compatibles), modelo, campo clave oculto, probar conexión, guardar y borrar credencial.
- Ajustes avanzados plegables: timeout, máximo de tokens, límite por tarea, número de reintentos, análisis musical, confirmación antes de aplicar, densidad decorativa.
- Opción clara para desactivar cualquier uso de red comunitaria/telemetría y para no compartir información sensible del nivel.

### 2.3 UX de generación

1. Usuario selecciona modo y objeto/estructura o zona de trabajo.
2. Escribe «decora estos bloques estilo espacio, conservando gameplay».
3. DecoMind muestra objetivo/alcance, proveedor y estimación del volumen de objetos **sin garantizar duración**.
4. Se construye un plan por etapas, se valida y aparece una previsualización progresiva.
5. El usuario elige **Aceptar / Editar / Rechazar**. Hasta aceptar, el nivel original sigue recuperable.
6. En la sesión se presenta un resumen: añadidos, modificados, eliminados, grupos, colores, triggers, errores/rechazos.
7. Se admite «reduce glow», «sube los triggers», «sin tocar los pinchos», etc., sin perder contexto.

---

## 3. Proveedores de IA sin Termux

### 3.1 Modelo de conexión

**Geode C++ → HTTPS directo → proveedor de IA → respuesta estructurada → validador → preview → editor.**

No es necesario mantener un proceso en Termux ni exponer `127.0.0.1:8765`. Los servicios externos necesitan conectividad, pero la app gestiona sus solicitudes por sí misma.

### 3.2 Platinum comunitario (sin cuenta) y límites

EditorAI documenta el modo **Use Platinum** combinado con Ollama, sobre computadoras de voluntarios. **No es inferencia local en Android**. Los prompts se envían a equipos de terceros; la latencia, continuidad y límites dependen de la red.

**Condiciones antes de integrarlo como predeterminado:**

- Confirmar que el servicio y sus términos permiten clientes derivados o externos a EditorAI, y que no requieren autorización, identificador, token o cabecera reservados.
- Verificar endpoint, protocolo, versiones, formato, límites, gestión de errores y política de privacidad en el código y documentación vigentes.
- No copiar ni reutilizar credenciales privadas, endpoints ocultos o tokens de otro producto.
- Añadir disclosure en primer uso: «Las instrucciones pueden procesarse en equipos comunitarios».
- Si el servicio no permite integración o deja de estar disponible, no simular disponibilidad: mostrar alternativa gratuita legítima o configuración personal.

**Decisión de diseño:** la experiencia predeterminada será *sin configuración obligatoria cuando exista un servicio público permitido*, no una promesa de IA gratis permanente.

### 3.3 Otros proveedores

- Adaptadores HTTPS para modelos de API compatibles o con formato propio.
- Priorizar proveedores que permiten cuota gratuita o inicio de sesión explícito, distinguiendo **gratis sin cuenta**, **gratis con cuenta** y **API de pago**.
- No usar automáticamente cuentas, claves o cuotas de terceros.
- El administrador podrá añadir modelos sin modificar la UI principal, mediante un registro de proveedores y esquemas.
- Fallback condicionado a consentimiento, privacidad y coste. Nunca reintentar en cadena sin límite.

### 3.4 API key personal guardada en el teléfono

- Clave introducida en **Ajustes → IA → API personal** y nunca incluida en el binario/distribución.
- Requerimiento preferido: Android Keystore o almacenamiento protegido del SO mediante adaptador seguro; evaluar viabilidad con Geode/NDK.
- Si solo está disponible almacenamiento ordinario de Geode, **no presentarlo como cifrado**: documentar riesgos y buscar mecanismo más seguro antes del lanzamiento público.
- El campo visual siempre aparece enmascarado; ofrecer **Borrar** y **Probar conexión**.
- No imprimir API keys, tokens de respuesta ni cabeceras Authorization en logs, crash reports, historial, prompts, backups ni estadísticas.
- HTTPS y validación de TLS; no aceptar endpoints HTTP remotos por defecto.
- Las claves personales se envían exclusivamente al proveedor elegido. Se debe impedir que una clave de OpenAI termine en endpoints de terceros por fallback.

### 3.5 Gestión de fallos y costes

Implementar máquina de estados: `Ready`, `Connecting`, `Running`, `RateLimited`, `Unavailable`, `AuthFailed`, `Canceled`, `Finished`, `Failed`.

- Timeouts controlados; cancelación de peticiones y de colocación.
- Retries con backoff y máximo de intentos, solo para errores reintentables.
- Tratamiento de 401/403, 429, 5xx, problemas de red, JSON inválido, respuestas demasiado grandes y contenido incompleto.
- Límite de tokens/operaciones por sesión y límite de coste configurado por usuario cuando hay API personal.
- Si la IA gratuita cae, presentar decisión al usuario; no cambiar a una API de pago automáticamente.

---

## 4. Roles/agentes sin multiplicar costes

El término «agente» será un **rol especializado** que puede ejecutarse con el mismo modelo/proveedor. No requiere necesariamente cuatro LLM distintos.

| Rol | Entrada | Salida |
|---|---|---|
| **Coordinador** | Prompt, modo, selección, contexto, presupuesto | Plan de secciones, dependencias, pasos y criterios |
| **Diseñador** | Geometría, dificultad, estilo | Grandes masas, rellenos y contornos |
| **Decorador** | Plan, catálogo, colores, geometría | Objetos glow, detalles, fondo, efectos por capas |
| **Sincronizador** | Canción, BPM/transitorios, speed portals | Timeline de beats y triggers propuestos |
| **Validador** | Operaciones estructuradas, estado real del nivel | Errores, advertencias, operaciones aceptadas/rechazadas |

**Validador preferentemente determinista**, implementado en C++: no depender de una segunda IA para comprobar rangos, IDs, conflictos, colisiones o grupos. El coordinador puede recurrir a un subagente LLM solo si el proveedor lo permite, si es útil y dentro de los límites seleccionados.

**Política de privacidad:** cada rol recibe solo la información necesaria. No transmitir todo el historial o nivel completo si basta la selección y contexto relevante.

---

## 5. Arquitectura objetivo

```text
DecoMind UI existente (Cocos2d/Geode)
    │ UIAdapter: opciones, chat, progreso, Preview/Undo
    ▼
AppController / SessionManager
    │
    ├── ProviderManager
    │     ├── PlatinumProvider (solo con autorización y protocolo confirmado)
    │     ├── OpenAICompatibleProvider
    │     ├── GeminiProvider / otros adaptadores
    │     └── OfflineRuleProvider (no LLM)
    │
    ├── AgentOrchestrator + TaskPlanner
    │     ├── GD Catalog Service
    │     ├── Editor Perception Snapshot
    │     ├── Music Analysis / Sync Planner
    │     └── Operation Schema + Validator
    │
    ▼
Transactional Placement Engine
    ├── Preview Sandbox / Temp Objects
    ├── Batch Placement / Main Thread Queue
    ├── Apply / Reject / Undo
    └── Full Backup / Restore
    │
    ▼
EditorBridge (Geode native, LevelEditorLayer/EditorUI)
    ▼
Geometry Dash — nivel en memoria
```

### 5.1 Estructura modular propuesta

```text
DecoMind-AI/
├── CMakeLists.txt
├── mod.json
├── LICENSES/
│   ├── THIRD_PARTY_NOTICES.md
│   └── EditorAI-MIT.txt
├── docs/
│   ├── architecture.md
│   ├── provider-security.md
│   ├── operations-schema.md
│   ├── catalog-validation.md
│   ├── style-examples-jsonc.md
│   ├── migration-v038-v040.md
│   └── testing-android.md
├── src/
│   ├── main.cpp                       # Entradas/hook y arranque, no lógica monolítica
│   ├── ui/
│   │   ├── UIAdapter.hpp
│   │   └── AISettingsPanel.hpp        # Extiende UI actual, sin cambiar estilo
│   ├── core/
│   │   ├── AppController.hpp
│   │   ├── SessionManager.hpp
│   │   ├── AgentOrchestrator.hpp
│   │   ├── TaskPlanner.hpp
│   │   └── OperationSchema.hpp
│   ├── providers/
│   │   ├── IAIProvider.hpp
│   │   ├── ProviderManager.hpp
│   │   ├── PlatinumProvider.hpp
│   │   ├── OpenAICompatibleProvider.hpp
│   │   ├── ProviderCredentials.hpp
│   │   └── OfflineRuleProvider.hpp
│   ├── editor/
│   │   ├── EditorBridge.hpp
│   │   ├── EditorPerception.hpp
│   │   ├── NativePlacementEngine.hpp
│   │   ├── PreviewManager.hpp
│   │   ├── TransactionManager.hpp
│   │   └── BackupManager.hpp
│   ├── catalog/
│   │   ├── GDObjectCatalog.hpp
│   │   ├── TriggerSchemaRegistry.hpp
│   │   └── CatalogVerifier.hpp
│   ├── examples/
│   │   ├── ExampleLibrary.hpp
│   │   ├── JsoncLoader.hpp
│   │   ├── ExampleNormalizer.hpp
│   │   ├── StyleReferenceManager.hpp
│   │   └── ExampleRetriever.hpp
│   ├── validation/
│   │   ├── OperationValidator.hpp
│   │   ├── GameplaySafety.hpp
│   │   └── BudgetLimits.hpp
│   └── music/
│       ├── AudioAnalyzer.hpp
│       ├── TimingMap.hpp
│       └── TriggerSyncPlanner.hpp
├── resources/
│   ├── ...                            # Mantener recursos visuales existentes
│   ├── catalog/verified_ids.json
│   └── examples/
│       ├── manifest.json             # Índice de ejemplos, metadatos y licencias
│       ├── sections/                 # Secciones aprobadas y normalizadas
│       └── originals-jsonc/          # Solo ejemplos redistribuibles, con atribución
├── tests/
│   ├── unit/
│   ├── fixtures/
│   └── integration/
└── legacy/
    └── README.md                      # Referencia al antiguo Server/, sin ejecución requerida
```

**Nota:** rutas propuestas, no estructura ya creada. Evitar duplicar `UIRenderer.hpp` y archivos actuales hasta que sus consumidores se hayan migrado. Los nombres pueden adaptarse a las convenciones C++ del repositorio.

### 5.2 Responsabilidades y contratos

**`EditorBridge`**: lectura del nivel en el hilo correcto, captura de selección/cámara, operaciones nativas de crear/actualizar/eliminar objetos, sin exponer punteros de GD al modelo. Debe detectar si se cerró/cambió el nivel y cancelar acciones pendientes.

**`OperationSchema`**: representación independiente del proveedor. Soportar create/update/delete, trigger config, color-channel updates, grupos/capas, timeline de efectos y operaciones auxiliares con revisión de versiones.

**`OperationValidator`**: valida IDs, propiedades por clase, relaciones de grupos, ocupación, rangos de velocidad/cámara y límites de objetos. El backend de IA no puede saltarse esta capa.

**`PreviewManager`**: coloca en layer/capa temporal aislada o crea objetos temporales rastreables según lo permita la API de Geode. Debe poder retirar cada elemento sin tocar objetos previos y sin dejar triggers activos después de rechazar.

**`TransactionManager`**: operación atómica a nivel de lote; registro de cambios, undo/redo, restauración incluso ante fallo parcial o cancelación, asignación de IDs controlada.

**`ProviderManager`**: despacho HTTPS, autenticación, cuota, fallback con consentimiento y normalización de errores.

**`SessionManager`**: historial de mensajes y decisiones, resumen condensado y tareas, sin almacenar secretos.

**`StyleReferenceManager` / `ExampleRetriever`**: encontrar fragmentos de niveles y ejemplos JSONC según estilo, tipo de gameplay y petición; devolver referencias compactas al orquestador sin construir objetos directamente ni introducir contenido no verificado.

---

## 6. Contrato de operaciones estructuradas

No permitir al LLM generar arbitrariamente cadenas completas de serialización GD como formato de confianza. Puede proponerse un contrato JSON **interno** que el juego validará y traducirá a llamadas nativas.

```json
{
  "schemaVersion": 1,
  "requestId": "example-001",
  "target": {"type": "selection"},
  "constraints": {
    "preserveGameplay": true,
    "maxAddedObjects": 400,
    "maxNewGroups": 30,
    "maxNewColors": 12
  },
  "operations": [
    {
      "op": "createObject",
      "tempId": "glow-a1",
      "objectId": 505,
      "position": {"x": 450, "y": 165},
      "rotation": 0,
      "scale": 0.75,
      "colorId": 12,
      "zLayer": -3,
      "zOrder": -5,
      "groups": [101]
    }
  ]
}
```

**Importante:** este ejemplo muestra el contrato previsto; la validez exacta del ID 505 y sus propiedades debe comprobarse con el catálogo para el objeto concreto. No presentar como formato oficial de EditorAI o GDShare.

### 6.1 Validaciones obligatorias

- ID presente y permitido; categoría conocida; restricciones de escala/rotación/props según tipo.
- Coordenadas finitas y dentro de una región habilitada de trabajo.
- Grupos/colores únicos cuando correspondan; detección de referencias circulares y conflictos existentes.
- Triggers fuera de la zona de gameplay **visualmente** (p. ej., banda alta configurable), sin confundir su posición visual con su punto de activación temporal en X.
- Objetos decorativos sin hitbox dañina cuando corresponda; no considerar «Alpha 0» equivalente a desactivar colisión.
- Bloquear movimientos/rotaciones de objetos jugables si `preserveGameplay = true`.
- Revisar visibilidad por capas, Z, orden y T4, evitando oclusión de señales críticas.
- Límite de objetos/triggers por lote y límites de memoria/tiempo de procesamiento.
- Registro de aceptación/rechazo por operación, con diagnóstico legible.

---

## 7. Catálogo oficial/técnico de Geometry Dash

### 7.1 Material disponible

El paquete `GD_Editor_Catalogo.zip` contiene, entre otros:

- `04_Documentacion/catalogo_objetos_4083.json`
- `04_Documentacion/tabla_maestra_ids_4083.csv`
- `04_Documentacion/analisis_niveles_gmd.json`
- `04_Documentacion/verificacion_cruzada_10_niveles.json`
- `02_Librerias/gd_editor_lib.py` y `GDEditorCatalog.hpp`
- `03_Niveles_GMD/*.gmd`, incluyendo `fog.gmd` y `l.gmd`
- Recursos `.png`, `.plist`, manual técnico, atlas y `libcocos2dcpp.so`

**No distribuir la librería nativa original de Geometry Dash ni assets propietarios de RobTop dentro de la distribución del mod.** Utilizar los archivos como referencia local de análisis según corresponda. La compilación pública del mod debería emplear IDs/metadata derivados de manera permitida y cargar los recursos del juego desde su instalación.

### 7.2 Modelo de confianza

Cada objeto/trigger debe tener campos: `id`, `sprite`, `category`, `name`, `gdVersion`, `source`, `testedParameters`, `risk`, `status`, `sampleFixture`.

Estados recomendados:

1. `indexed`: ID o sprite observado, sin pruebas de comportamiento.
2. `documented`: documentación fiable de propiedades.
3. `fixture_verified`: exportación real desde GD valida propiedades.
4. `runtime_verified`: objeto creado, visualizado y comprobado dentro de GD.
5. `blocked`: conocido pero no apto hasta resolver una incompatibilidad.

### 7.3 Triggers y efectos

Crear esquemas **por trigger**, no una whitelist única de claves 7..87. Categorías prioritarias: Color, Pulse, Alpha, Move, Rotate, Scale, Spawn, Toggle, Follow, Stop, Camera Guide/Zoom/Offset/Rotate, Keyframe, T4/shaders y sincronización.

Cada esquema documentará: ID, propiedades, rangos, valores por defecto, campos obligatorios, grupos referenciados, capa de editor, orden de ejecución, sample `.gmd` y pruebas runtime.

**T4:** tratar las combinaciones de blending, cámaras, renderizado/superposición y shaders como sistemas independientes, con límites de rendimiento y pruebas visuales; no asumir que usar Z Layer por sí solo genera un efecto T4 correcto.

---

## 7A. Biblioteca de ejemplos JSONC de EditorAI (nuevo en plan v1.1)

### 7A.1 Finalidad y fuentes

Integrar **ejemplos de construcción reales como referencias de estilo** para el coordinador y los agentes de DecoMind. Los ejemplos ayudan a elegir familias de objetos, geometría, ritmo visual, densidad, simetría/asimetría y organización de estructuras, pero **no sustituyen el catálogo oficial/técnico ni prueban que un objeto concreto sea válido**.

Fuentes de EditorAI disponibles en su repositorio:

- `resources/example_levels/*.jsonc`: ejemplos de fragmentos de niveles, tales como `Nine_Circles.jsonc`, `OuterSpace.jsonc`, `Bloodbath.jsonc`, `Future_Funk.jsonc` y `Change_of_Scene.jsonc`. Los archivos inspeccionados tienen una clave `analysis` y una lista `objects` con objetos parcialmente descritos.
- `resources/example_sections.json`: índice de *few-shot examples* con `level_name`, `author`, `theme`, `difficulty`, `tags`, `description`, `orig_x_range`, `object_count` y lista `objects` con X normalizadas por sección según el comentario del archivo.
- `resources/object_ids.json`: mapa de nombres simbólicos (`type`) a IDs numéricos; cruzar con `GD_Editor_Catalogo.zip` y los registros confirmados por el juego.
- `src/design_knowledge.hpp`: criterios técnicos de diseño, gameplay, glow, música y legibilidad; emplear como criterios de orientación, no como pruebas de ejecución.

**Precaución de origen:** una parte de los ejemplos representa fragmentos de niveles publicados por otros creadores. Mantener atribución y procedencia; verificar derechos, condiciones del repositorio y cualquier material de terceros antes de redistribuir copias de geometría/ejemplos. Preferir crear resúmenes de patrones y ejemplos propios cuando no pueda acreditarse la reutilización concreta. La licencia MIT del código EditorAI no certifica por sí sola los derechos sobre diseños ajenos.

### 7A.2 Qué es y qué no es un `.jsonc`

**JSONC = JSON with Comments**. Acepta comentarios `//` y `/* ... */` donde el parser elegido los soporte. En este proyecto se usará como **dato de referencia**, no como formato de importación de GDShare. Por lo general, un ejemplo no incluye cabecera completa de nivel, audio, canales, propiedades completas de triggers ni garantías de jugabilidad. Nunca convertirlo directamente a `.gmd` sin normalización y validación.

Formato observado (fragmento representativo simplificado, sin copiar ejemplos completos):

```jsonc
{
  "analysis": "Fragmento de ejemplo para reconocimiento de estilo",
  "objects": [
    // ID numérico y coordenadas del editor
    { "id": 218, "x": 105.0, "y": 99.0 },
    { "id": 507, "x": 109.0, "y": 105.0, "rot": -90.0 },
    // Referencia simbólica que debe resolverse usando el catálogo
    { "type": "decor_medium_rod", "x": 159.0, "y": 164.0 }
  ]
}
```

El índice de secciones usa metadatos adicionales. Se deben soportar ambos esquemas y versiones identificadas, sin deducir silenciosamente campos ausentes.

### 7A.3 Pipeline obligatorio de importación y preparación

1. **Descubrimiento controlado:** listar `*.jsonc` y `example_sections.json`, registrar nombre, fuente, licencia/atribución, hash de contenido, versión y fecha de incorporación.
2. **Parsing correcto:** usar un parser JSONC que soporte comentarios de forma segura; no eliminar `//` mediante expresiones regulares indiscriminadas (podrían romper URLs o cadenas). Limitar tamaño, profundidad, cantidad de objetos y longitud de cadenas.
3. **Validación de esquema:** aceptar `objects` como lista; exigir `x` e `y` numéricos finitos y exactamente una representación resoluble de objeto (`id` o `type`; si hay ambas, comprobar coherencia). `rot` y `scale` opcionales; validar el resto de campos solo si están documentados.
4. **Resolución semántica:** traducir `type` mediante `object_ids.json`, cotejar su ID numérico con el catálogo de GD y marcar objetos no reconocidos. No atribuir automáticamente colisiones, glow o comportamiento por nombre.
5. **Normalización espacial:** convertir a coordenadas locales, calcular `bbox`, ancho/alto, densidad, distribución por capas, relación con suelo/peligros y familias de objetos. Guardar el desplazamiento original para trazabilidad; **no confundir la X normalizada del índice con la posición real del nivel**.
6. **Etiquetado de confianza:** cada ejemplo y objeto asociado recibe `indexed`, `documented`, `fixture_verified`, `runtime_verified` o `blocked`. Los desconocidos se excluyen de ejecución automática hasta revisión.
7. **Preprocesamiento:** deduplicar entradas equivalentes, resumir patrones, agrupar ejemplos por estilo/gameplay/estructura y crear un manifiesto buscable sin necesidad de cargar todos los objetos en cada prompt.
8. **Revisión de derechos:** para paquetes redistribuidos, documentar procedencia, autores, licencias y política de uso; permitir desactivar o excluir conjuntos de ejemplos.
9. **Pruebas:** cualquier transformación de referencia en propuestas de nivel debe atravesar el contrato `OperationSchema`, el validador y la previsualización nativa; los JSONC nunca deben tener acceso directo al `EditorBridge`.

### 7A.4 Metadatos indexados y selección contextual

Formato interno **propuesto** para cada entrada (no es el esquema oficial de EditorAI):

```json
{
  "exampleId": "editorai:nine-circles:section-001",
  "origin": "entity12208/EditorAI",
  "sourcePath": "resources/example_levels/Nine_Circles.jsonc",
  "styleTags": ["nine-circles", "neon", "geometric"],
  "geometry": { "normalized": true, "width": 594, "objectCount": 200 },
  "gdCompatibility": "needs-validation",
  "usage": "reference-only",
  "attributionStatus": "review-required"
}
```

Los valores del ejemplo anterior son **ilustrativos**, excepto el origen/ruta y los 200 objetos/intervalo 105–699 consignados en el encabezado de ese JSONC: el sistema real deberá derivar las etiquetas y métricas tras la lectura.

Recuperación contextual (*retrieval*):

- Entradas: prompt del usuario, modo de DecoMind, selección/bounding box, tema, dificultad, tipo de gameplay, presupuesto de objetos, restricciones de colisiones y paleta existente.
- Ranking inicial: coincidencia de etiquetas y descripción, forma geométrica, tipo de objeto y densidad; añadir búsqueda semántica después, sin requisito de embeddings remotos.
- Seleccionar **2–5 ejemplos pequeños** por petición; entregar al modelo un resumen + fragmentos relevantes, no cientos de archivos enteros.
- Priorizar estilos compatibles con la selección. Por ejemplo: `Nine_Circles.jsonc` para contornos/patrones wave y `OuterSpace.jsonc` para formas espaciales, **sin reutilizar coordenadas absolutas ni colocar hazards por copia literal**.
- Registrar qué ejemplos influyeron en la propuesta y permitir consultar/deshabilitar esas referencias en modo avanzado.

### 7A.5 Uso por agentes

| Rol | Uso de JSONC | Restricción |
|---|---|---|
| Coordinador | Selecciona familias, decide objetivo y presupuesto | No ejecuta objetos directamente |
| Diseñador | Extrae siluetas, módulos, bordes y rellenos | Adapta escala y región; evita copiar secciones completas |
| Decorador | Deriva paletas, glow selectivo y capas | Mantiene despejada la ruta jugable |
| Sincronizador | Detecta patrones de triggers *si sus campos están documentados* | El sync real proviene de la canción/offset; los JSONC no determinan beats |
| Validador | Contrasta patrones propuestos con IDs y parámetros verificados | No acepta un ejemplo como prueba de compatibilidad |

### 7A.6 Gestión y configuración

- **Predeterminado:** biblioteca curada integrada, sin configuración manual del usuario, con ejemplos de origen permitido y validados para su uso como referencia.
- **Configuración avanzada (dentro de la UI actual):** activar/desactivar biblioteca, elegir estilos permitidos, intensidad de influencia (`baja/media/alta`), ver referencias usadas y administrar ejemplos propios importados localmente.
- **Bibliotecas personalizadas:** permitir importar `.jsonc` o `.json` del usuario con validación, aislándolos como datos no confiables. No cargar código ejecutable ni rutas arbitrarias por estar presentes en los comentarios.
- **Actualizaciones:** versión de esquema del índice, migración de caché, hashes y revocación de entradas defectuosas; la carga local no requiere Termux ni servicio web.
- **Límite de recursos Android:** indexar una sola vez, cachear metadatos compactos y leer las listas de objetos solo bajo demanda.

### 7A.7 Entregables y pruebas de aceptación

- `ExampleLibrary`, `JsoncLoader`, `ExampleNormalizer`, `StyleReferenceManager`, `ExampleRetriever` y documentación `docs/style-examples-jsonc.md`.
- Manifiesto de procedencia y catálogo de casos válidos/rotos.
- Tests con `Nine_Circles.jsonc`, `OuterSpace.jsonc` y `example_sections.json`; también con comentarios, campos faltantes, `type` desconocidos, valores no finitos, IDs discrepantes y JSONC malformado.
- **Prueba de aceptación A:** parsear y normalizar ejemplos sin modificar ningún nivel ni invocar la red.
- **Prueba de aceptación B:** pedir temática espacial, recuperar referencias relevantes y generar una propuesta válida, pero sin ejecución hasta aceptar preview.
- **Prueba de aceptación C:** el mismo layout con biblioteca desactivada no falla y funciona con generación ordinaria.
- **Prueba de aceptación D:** toda propuesta conserva los objetos jugables originales y registra las referencias que utilizó.

**Orden de implementación:** incorporar el lector e índice durante Fase 1; usar la recuperación en Fase 4 y las recetas de estructura/decoración en Fase 5. No bloquear el primer MVP de colocación nativa por cubrir todos los ejemplos o estilos.

---

## 8. Motor de decoración y estructura

### 8.1 Planificación visual

El decorador debe trabajar en este orden:

1. Leer geometría y la región seleccionada; identificar superficies jugables, obstáculos, portales, rutas de jugador y espacios vacíos.
2. Proponer **masas grandes** coherentes con la temática antes de añadir elementos pequeños.
3. Separar contornos, relleno, detalle y luz en grupos/capas específicos.
4. Decorar pinchos con silueta visible, glow restringido y colores coherentes.
5. Añadir decoraciones ambientales y fondos sin tapar los inputs.
6. Incorporar animaciones independientes y transiciones solo cuando exista suficiente margen de visibilidad/rendimiento.
7. Validar paleta y lectura de gameplay a tamaño de teléfono.

### 8.2 Temas iniciales

- **Nine Circles moderno:** masas geométricas oscuras, bordes neón, glow selectivo, contraste alto, secciones wave claramente legibles.
- **Espacial brillante:** nebulosas por capas, estrellas, órbitas, núcleos de energía, partículas y pulsaciones, preservando visibilidad.
- **Tech/industrial:** módulos estructurales repetibles, paneles internos, líneas y juntas; evitar patrones idénticos a lo largo de todo el nivel.

### 8.3 Estilos definibles por usuario

Permitir presets de paleta, densidad, tipo de relleno, márgenes, familias de objetos, z-layers y grupos reservados. Los presets se pasan al orquestador, no se incrustan como lógica fija en el `main.cpp`.

---

## 9. Música, beats y animación sincronizada

**Objetivo:** todos los efectos (BG, glow, estructuras, Pulse, Alpha, Move, T4 y cámara autorizada) comparten una misma línea de tiempo, no ciclos de beats inconexos.

### 9.1 Modelo de tiempo

- `songOffset` real del nivel y comienzo de canción.
- BPM estimado/medido y cambios de tempo.
- Marcadores de kick, snare, hats, drops, builds, pausas, melodía y transitorios.
- Segmentos de velocidad/portales con su transformación tiempo↔X; no usar una velocidad constante global.
- Triggers activados por X y por Spawn/Channel donde proceda.
- Separar la **posición X de activación** de la **posición Y alta de organización visual**.

### 9.2 Política musical

- Analizador local determinista de waveform/BPM si el audio está accesible legal y técnicamente.
- La IA recibe mapa musical resumido y lo convierte en un plan de efectos; los tiempos reales prevalecen sobre una aproximación narrativa.
- Beats fuertes: Pulse, glow, flash selectivo; melodías: cambios suaves; transiciones: cámara moderada.
- No saturar los flashes por fotosensibilidad; ofrecer reducción de flashes y límites configurables.
- Sin análisis fiable, informar que el sync es aproximado y permitir ajuste manual de offset.

### 9.3 Camera Guide

Tratar Camera Guide y triggers de cámara como operaciones de alto riesgo: exigir fixtures válidas, límites, retorno explícito al estado anterior y previsualización por secciones. No habilitar cambios globales sobre niveles jugables hasta superar las pruebas de compatibilidad.

---

## 10. Previsualización, backups y cancelación

### 10.1 Ciclo transaccional

```text
Prompt → Context Snapshot → Plan → Validación
    → Preview en lotes → Revisión del usuario
    ├── Aceptar → Commit + historial + respaldo
    ├── Editar  → Nueva revisión sobre propuesta
    └── Rechazar → Rollback completo
```

### 10.2 Garantías de integridad

- Guardar estado original antes de generar: objetos afectados, propiedades, grupos/colores/trigger states, identificador de nivel.
- Crear lista de objetos temporales propia de la operación.
- Si se cierra el editor, se cambia el nivel o se cancela la sesión, no editar otro nivel por error.
- No considerar un fichero con nombre y cantidad de objetos como backup válido.
- Registro reversible de deltas; respaldo real de datos del nivel antes de operaciones de riesgo.
- Rollback idempotente: rechazar dos veces no elimina objetos ajenos ni duplica objetos.
- Guardar backup atómico en archivo temporal + confirmación/renombrado tras escritura.

### 10.3 Pruebas de reversión

- 1, 10, 100 y 1000 objetos decorativos temporales.
- Cancelación mientras se colocan lotes.
- Rechazo tras una falla de red.
- Modificación con selección previa y sin selección.
- Cerrar/reabrir editor durante generación.
- Repetir aceptar, editar, deshacer y rehacer.
- Verificar objetos, grupos, colores y triggers originales sin cambios tras rollback.

---

## 11. Estrategia de integración del código MIT

El repositorio EditorAI declara licencia MIT, con aviso de copyright de **Entity12208, 2026**. Puede utilizarse/modificarse bajo sus condiciones: conservar el texto legal y la atribución en copias o porciones sustanciales del software.

**Proceso recomendado:**

1. Fijar commit upstream utilizado en `docs/third-party-sources.md`.
2. Identificar una capacidad concreta en `EditorAI/src/main.cpp` (p. ej., preview o colocación).
3. Aislar dependencias, llamadas Geode y estructuras de datos necesarias.
4. Extraer/adaptar a una clase nueva en DecoMind, minimizando interferencia con su UI.
5. Añadir atribución y pruebas del componente adaptado.
6. No copiar `overlay.cpp` ni incorporar ImGui si no aporta una necesidad concreta.
7. Verificar licencias de terceros e integraciones de red por separado.

**Importante:** la licencia MIT de EditorAI no concede derechos automáticos sobre el servicio Platinum ni los recursos originales de Geometry Dash. Son cuestiones independientes.

---

## 12. Plan de migración por versiones y entregables

### Fase 0 — Congelación de línea base

**Rama:** `feature/editorai-hybrid-v040` desde v0.3.8-alpha.  
**Tareas:** etiqueta de respaldo; recopilar builds/manifest; prueba de compilación Android; captura de UI; esquema de datos actual; detectar funciones dependientes de `Server/`; añadir pruebas smoke.  
**Aceptación:** mod compila y abre, sin diferencias visibles ni regresiones en sus nueve modos, colores y controles.  
**Salida:** `baseline-report.md`, log de compilación, checklist de UI.

### Fase 1 — Núcleo de operaciones y catálogo

**Tareas:** `OperationSchema`, `CatalogService`, `OperationValidator`, `EditorPerception`, límites por objeto y trigger, fixtures `.gmd`, parser JSONC e índice local de ejemplos con procedencia.  
**Aceptación:** parsear/validar 100 propuestas variadas, rechazar IDs desconocidos y propiedades fuera de rango, sin modificar el nivel.  
**Salida:** tests unitarios + datos de catálogo con estados de verificación.

### Fase 2 — EditorBridge + Preview/Undo

**Tareas:** adaptar colocación de EditorAI, operaciones nativas, cola main-thread, capas temporales, rollback, backups reales.  
**Aceptación obligatoria:** crear 10/100 objetos en el editor, mostrar preview, rechazar y comprobar igualdad de estado original. Repetir en Android.  
**Salida:** primera build útil de v0.4 alpha, todavía sin IA externa obligatoria.

### Fase 3 — Proveedores HTTPS sin Termux

**Tareas:** `IAIProvider`, `ProviderManager`, adaptadores, API personal, almacenamiento seguro, manejo de errores, consentimiento comunitario; mantener el modo offline por reglas.  
**Aceptación:** ejecutar petición directa desde el mod sin proceso Python; fallo/timeout/cancelación no bloquea el editor; clave no aparece en logs.  
**Salida:** ajustes funcionales y conectividad verificable.

### Fase 4 — IA coordinadora y sesiones

**Tareas:** planner, roles, contexto incremental, resumen, historial, compatibilidad con chat actual; selección de 2–5 ejemplos JSONC apropiados por tarea y validación determinista antes de preview.  
**Aceptación:** solicitud en español → plan → objetos propuestos → preview → aceptar/rechazar; continuidad del chat sin perder referencia a selección ni crear objetos fuera de región.

### Fase 5 — Decoración avanzada

**Tareas:** familias de estructuras, glow, rellenos, pinchos, paletas, capas, variantes, selección contextual y recetas derivadas de ejemplos JSONC curados (sin copiar secciones ajenas completas).  
**Aceptación:** decorar 3 estructuras diferentes con diseños coherentes, jugabilidad intacta y sin objetos flotantes injustificados.

### Fase 6 — Sync, T4 y cámara (posterior a v0.4 inicial)

**Tareas:** análisis de canción, timing map, Pulse+Alpha+Move, efectos T4 comprobados, Camera Guide y restauración de cámara.  
**Aceptación:** grabación dentro del juego con beats verificados, triggers organizados en Y alta, sin flashes excesivos y con restauración de estado.

### Fase 7 — Optimización y publicación alpha

**Tareas:** pruebas Android de memoria, cancelación, consumo de red, degradación offline, migración de settings, documentación, consentimiento y licencias.  
**Aceptación:** sesiones largas estables, errores legibles, rollback comprobado, ninguna dependencia de Termux y UX visual original preservada.

### Priorización de lanzamiento

| Función | v0.4.0-alpha mínima | v0.4.x posteriores |
|---|---|---|
| UI actual | Sí | Sí |
| Lectura de selección | Sí | Mejorada |
| Catálogo y validador | Sí, subset verificado | Cobertura creciente |
| Colocación nativa/preview/rollback | Sí | Optimización |
| IA directa y API personal | Sí, proveedor(es) comprobados | Más proveedores |
| Servicio comunitario sin cuenta | Si autorizable/disponible | Fallback adicional |
| Roles especializados | Coordinador + validador | Subagentes reales opcionales |
| Decoración completa | Primeros estilos | Mayor variedad/calidad |
| Biblioteca JSONC de ejemplos | Parser e índice curado, consulta limitada | Presets, recomendaciones y ejemplos del usuario |
| Sincronización precisa | Básica/experimental | Avanzada |
| T4 + Camera Guide | No bloquear MVP | Tras fixtures y pruebas |

---

## 13. Plan de pruebas y criterios de aceptación globales

### 13.1 Seguridad funcional

- No se alteran portales, hitboxes, speed changes, orbes, pads o objetos originales sin aprobación expresa.
- Todos los `create/update/delete` pasan por schema validator.
- Grupo/color seleccionado no colisiona con IDs usados salvo autorización.
- Undo/Reject devuelve los valores exactos originales.
- La IA no ejecuta llamadas arbitrarias del motor ni acciones de red fuera de la lista de proveedores.

### 13.2 Compatibilidad GD/Geode

- `mod.json` conserva el ID y manifiesto de DecoMind; versión GD probada real.
- Compilación Android arm64 y test de inicio; desactivar hooks problemáticos por plataforma.
- Ningún cambio del selector cromático, robot y botón flotante altera su UI.
- Contador y visibilidad dentro del editor concuerdan; el éxito no se evalúa solo por número de objetos externos.

### 13.3 Red, cuentas y privacidad

- Sin Termux ni servidor Python al instalar.
- Proveedor gratuito externo disponible **solo si legal y operativamente habilitado**.
- Degradación visible cuando no hay proveedor, cola o Internet.
- Consentimiento para enviar contenido del nivel a servicios comunitarios.
- API key no exportada ni registrada; prueba de borrar/revocar clave.
- Sesiones y backups sanitizados; telemetría desactivada por defecto salvo consentimiento.

### 13.4 Calidad de generación

- Tres prompts de test: «deco moderna de una estructura», «espacial brillante en zona seleccionada», «Nine Circles glow con ruta de wave clara».
- Inspección en móvil a escala normal y en editor con distintas capas.
- Densidad y rendimiento configurables.
- No afirmar que un nivel es «jugable» únicamente por análisis geométrico: exigir playtest humano donde proceda.

### 13.5 Matriz mínima de fixtures

| Fixture | Prueba | Resultado esperado |
|---|---|---|
| Nivel vacío creado *dentro* de GD | +10 bloques | Visible, editable y reversible |
| Layout `l.gmd` importado y abierto | Glow solo en selección | Preserva gameplay y grupos |
| `fog.gmd` importado y abierto | Deco espacial | No altera triggers actuales |
| Nivel con múltiples velocidades | Beats → posiciones X | Sincronía relativa coherente |
| Nivel con cámara y T4 | Nuevos efectos limitados | Sin cambios persistentes indebidos |
| Nivel grande | 1000 objetos preview en lotes | UI responde, rollback correcto |

---

## 14. Migración de configuraciones de v0.3.8

**No destruir valores guardados ni forzar un reset del usuario.**

- `mode = server`: migrar a `ai_mode = api-personal` **solo si existe una credencial explícita válida**; en caso contrario seleccionar `auto` y explicar que se eliminó Termux.
- `mode = local`: migrar a `offline-rules` opcional o `auto` con aviso; no llamar «IA local» al generador por reglas.
- `server_url`: conservar únicamente en una clave de migración/legado, sin utilizarla por defecto, sin compartirla.
- Mantener colores, preferencias de UI, estado del botón, fuente, historial y modos existentes.
- `origin_mode`: compatibilidad de lectura durante una versión; no mantener doble fuente de verdad.
- API keys: no migrar automáticamente a almacenamiento inseguro ni almacenar credenciales en texto plano.
- Registrar versión de esquema de ajustes y ruta de reversión.

---

## 15. Riesgos y medidas preventivas

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Platinum no permite clientes externos o se cae | Alto | Revisar autorización/protocolo; fallback sin promesa de gratuidad |
| Copiar `main.cpp` completo de EditorAI | Alto | Extraer solo subsistemas, wrappers y pruebas |
| Incompatibilidad C++/Geode/NDK | Alto | Compilación base y pruebas por módulo |
| Modificación accidental del gameplay | Crítico | `preserveGameplay` por defecto, preview y rollback |
| Backups incompletos | Crítico | Sustituir snapshot `.txt` por respaldo restaurable |
| Catálogo con IDs mal etiquetados | Alto | Estados de confianza y fixtures por categoría |
| Overdraw/lag por glow, T4 o objetos | Alto | Presupuestos por escena, LOD y perfil móvil |
| Sincronización basada solo en BPM promedio | Medio/alto | Waveform + offset + speed map; indicar precisión |
| Exposición de API keys | Crítico | Keystore/secret adapter, TLS, logs sanitizados |
| Respuestas malformadas del proveedor | Alto | JSON schema, límite de tamaño, no aplicar sin validación |
| Cambio inadvertido de UI | Medio | Capturas de referencia y tests manuales de regresión |

---

## 16. Recomendaciones de implementación inmediatas

**Primer cambio recomendado:** separar `LevelExecutor.hpp` en `EditorPerception` y `NativePlacementEngine` sin alterar la UI. Crear prueba nativa que añada una sola pieza al editor actual; registrar la pieza y eliminarla con Reject.

**Segundo cambio:** crear el esquema de operaciones y usar el catálogo para validar la pieza; incluir fixtures de estructura, glow y un Pulse.

**Tercer cambio:** conectar `AIClient` a `ProviderManager` con un proveedor de prueba sin red; después añadir HTTPS real y la API personal. Solo habilitar Platinum como predeterminado después de confirmar permiso, protocolo y privacidad.

**Cuarto cambio:** mover la nueva planificación y sus resultados a la UI actual mediante un adaptador, sin reemplazar ventanas.

**Quinto cambio:** prueba real en Android usando `fog`/`l` para confirmar colocación, visibilidad, colores, rechazo y conservación del gameplay.

### Checklist de aceptación para la primera integración

- [ ] Rama de desarrollo y versión base compiladas.
- [ ] UI actual sin cambios visuales no solicitados.
- [ ] Selección/contexto leídos desde el editor real.
- [ ] Primer objeto agregado de forma nativa y visible.
- [ ] Grupo/color validados y registrados.
- [ ] Preview con botones Aceptar/Rechazar.
- [ ] Rechazar devuelve el nivel exactamente a su estado anterior.
- [ ] Respaldo restaurable, no solo metadatos.
- [ ] Peticiones HTTPS directas sin Termux.
- [ ] Configuración de API personal y borrado de credencial.
- [ ] Proveedor gratuito permitido, si existe, con aviso de privacidad.
- [ ] Mensajes de error claros cuando el proveedor no esté disponible.
- [ ] Atribución MIT completa para código derivado de EditorAI.
- [ ] Pruebas reales en Android/Geode documentadas.

---

## 17. Fuentes y trazabilidad

### Repositorios y documentación

- [EditorAI: ejemplos JSONC](https://github.com/entity12208/EditorAI/tree/main/resources/example_levels), [Nine_Circles.jsonc](https://github.com/entity12208/EditorAI/blob/main/resources/example_levels/Nine_Circles.jsonc) y [OuterSpace.jsonc](https://github.com/entity12208/EditorAI/blob/main/resources/example_levels/OuterSpace.jsonc).
- [EditorAI: índice example_sections.json](https://github.com/entity12208/EditorAI/blob/main/resources/example_sections.json) y [mapa object_ids.json](https://github.com/entity12208/EditorAI/blob/main/resources/object_ids.json).


1. [DecoMind-AI](https://github.com/proj14859-cmyk/DecoMind-AI) — arquitectura original y código de la versión en GitHub.
2. [EditorAI](https://github.com/entity12208/EditorAI) — generación nativa, sesiones, ejemplos y adaptadores.
3. [Licencia MIT de EditorAI](https://github.com/entity12208/EditorAI/blob/main/LICENSE) — permiso y condiciones de reutilización.
4. [README de EditorAI](https://github.com/entity12208/EditorAI/blob/main/README.md) — proveedores, instalación, preview y capacidades documentadas.
5. [EditorAI Platinum](https://github.com/entity12208/editorai-platinum) — comprobar condiciones operativas y de uso actuales antes de activar la integración.
6. `GD_Editor_Catalogo.zip` — catálogo, sprites, 10 niveles de referencia y utilidades aportadas por el propietario del proyecto.

### Advertencias de precisión

- La información del código es una revisión estática de archivos accesibles en los repositorios; **no se ha compilado ni integrado el mod en este documento**.
- La licencia del código no garantiza permiso para utilizar infraestructura comunitaria o redistribuir assets de Geometry Dash.
- «Sin Termux» significa cliente HTTPS nativo, no ausencia de Internet ni garantía de acceso gratuito.
- Los diseños de carpetas, interfaces, clases y JSON en este documento son **especificaciones propuestas**, no afirmaciones de funciones ya implementadas.

---

## 18. Definición de terminado

La **primera entrega v0.4.0-alpha** estará completa cuando DecoMind-AI, conservando su UI y sus modos, pueda: leer una selección real del editor; obtener un plan desde un proveedor configurado directamente en el mod (sin Termux); validar y colocar objetos nativos en previsualización; aceptarlos o rechazarlos; revertir correctamente todas las modificaciones; mostrar el resultado en el historial; y guardar un respaldo restaurable. Todo ello probado en la versión Android/Geode objetivo.

El trabajo de decoración profunda, modelos multiagente opcionales, sincronización musical exacta, Camera Guide y composiciones T4 complejas continuará en entregas posteriores respaldadas por fixtures y pruebas reales.

**Directriz final:** preservar la experiencia de DecoMind-AI; reutilizar los subsistemas de EditorAI que aporten valor; hacer del catálogo una fuente verificable; y construir primero un motor transaccional nativo que funcione antes de ampliar sus efectos visuales.
