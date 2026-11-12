# DecoMind-AI v0.4.0-alpha — primera integración

Esta rama implementa una **primera integración experimental** del plan maestro v1.1 del 12/11/2026. No equivale a completar todas sus fases ni a certificar funcionamiento dentro de Android.

## Flujo implementado

UI existente → AIClient → ProviderManager (reglas u OpenAI/Groq HTTPS) → PlanJson → OperationValidator → PreviewManager → motor nativo de GD.

La región se captura al solicitar (selección + margen de 45 unidades, o viewport). Se compara el nivel serializado al recibir la respuesta; si cambió, se descarta. Ningún objeto original se actualiza o elimina. El motor solo convierte propiedades tipadas y validadas al formato que recibe `createObjectsFromString`; nunca envía serialización generada libremente por el modelo al juego.

Antes del preview se escribe y verifica un respaldo JSON completo mediante archivo temporal y renombrado. Incluye el resultado de `getLevelString()` y metadatos de canción. Las copias se recuperan como un nivel nuevo desde Mis niveles y respaldos. Los antiguos `.txt` no se presentan como respaldos restaurables.

Preview coloca hasta 25 objetos por tick; los objetos añadidos se retienen en una lista propia. Rechazar, cerrar, cancelar o cambiar de editor elimina únicamente esa lista en su editor propietario. Aceptar registra un único UndoObject de tipo Paste. Deshacer en DecoMind solo actúa si esa acción continúa al final del stack, para evitar deshacer acciones ajenas.

Al terminar la colocación se oculta temporalmente el panel para inspeccionar el editor y aparecen ACEPTAR / RECHAZAR / EDITAR. Editar retira la propuesta y devuelve la instrucción al chat. Los comandos `/aceptar`, `/rechazar` y `/editar nueva instruccion` también están disponibles.

## Límites deliberados

- Solo createObject. update/delete, cambios de canales, triggers, cámara y T4 se rechazan.
- Catálogo inicial reducido: IDs 1,15,16,17,503,504,505; 1 es gameplay y está bloqueado por defecto. Son entradas documentadas, **sin certificación runtime**. No se declara cobertura de 4.083 objetos.
- Las nueve páginas/modos originales permanecen; sus capacidades avanzadas todavía no están implementadas por el nuevo motor. Layout también está limitado a propuestas decorativas, e Ideas no puede crear objetos.
- Automatico y Sin conexion no sustituyen los pedidos por patrones fijos. Si no existe un proveedor o receta capaz de atenderlos, lo indican sin crear objetos. El patron de diez elementos queda reservado al comando explicito `/prueba-nativa` en Sin conexion, como diagnostico. Ideas mantiene los consejos consultivos.
- La biblioteca JSONC inicial contiene tres referencias semánticas originales; no redistribuye geometría de niveles ajenos. Recuperación local por tags, hasta cinco referencias, guardadas en historial al aceptar.
- No hay sync musical, cámara, agentes LLM adicionales, ni comunidad Platinum habilitada.
- Persistencia de claves protegida por SO pendiente. Las claves solo viven en memoria durante la sesión.
- La cancelación descarta la petición; no hay reintentos ni fallback remoto automático.
- La comparación serializada, carga de respuestas y guardado de backup no se ejecutan en un worker. Medir latencia y memoria en niveles grandes antes de ampliar presupuestos.

## Dependencias

Desde v0.4.1-alpha: C++20, Geode 6.0.0-alpha.1 y GD 2.2091. La primera integración v0.4.0 usaba Geode 5.11.2 / GD 2.2081. Sin ImGui ni dependencia runtime de Python/Termux. Las pruebas del núcleo usan matjson 4.0.1 y Result 1.5.0, iguales a Geode. No se incorporan binarios ni sprites propietarios de GD.
