# Verificación y aceptación pendiente

## Automatizada

```
cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

Verifica 100 planes válidos variados, IDs desconocidos/gameplay, región, NaN/Inf, escalas, Z, conflictos y duplicados, presupuestos, JSON malformado, enteros, rechazo de delete/rawGD, comentarios JSONC/URLs, recuperación de referencias e índice ausente. Son pruebas del núcleo, **no sustituyen pruebas nativas del juego**.

La workflow compila Windows, Android32 y Android64 y ejecuta estas pruebas en PRs y en la rama de integración.

## En dispositivo GD 2.2081 + Geode 5.11.2

Todo este bloque requiere ejecución humana y permanece pendiente hasta registrar evidencia:

- Abrir las páginas, nueve modos, selector de color, mascota y botón flotante; comparar con v0.3.8.
- Nivel vacío creado dentro del juego: preview de 10 elementos, aceptar, undo/redo.
- Selección en fog/l reales: preview y rechazo; comparar getLevelString antes/después incluyendo grupos, colores y triggers.
- 1/10/100/1000 objetos: validar lotes, respuesta táctil, cancelación parcial y consumo de memoria (1000 exige subir el límite interno del contexto de prueba; interfaz pública usa 400).
- Cerrar editor/popup mientras coloca y mientras espera HTTPS; reabrir otro nivel, comprobar aislamiento y ausencia de llamadas tardías.
- Rechazar dos veces; editar y generar de nuevo; acciones manuales posteriores a aceptar; deshacer conserva su orden.
- Guardar snapshot, recuperar copia, abrirla y comprobar geometría, configuración de nivel, canciones, grupos, colores y triggers. Nunca sustituir el nivel original durante esta prueba.
- OpenAI/Groq con clave propia de prueba: válido, 401,429,5xx,timeout, cancelación, JSON incompleto, modelos no disponibles. Revisar ausencia de secretos en logs/historial/backup.
- Borrar clave, cambiar proveedor y reiniciar proceso: no reutilizar credenciales.
- Sin Internet, auto/free/offline: mensajes veraces; ningún proceso Python requerido.
- Tres prompts del plan y playtest humano; verificar que decoración no tapa inputs.

No publicar como entrega completa del plan hasta superar estas comprobaciones y resolver la persistencia protegida de claves. Sync/T4/cámara son fases posteriores.
