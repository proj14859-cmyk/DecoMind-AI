# Migración de ajustes

Se registra settings-schema-version=1 una sola vez. `origin_mode` o mode antiguos se leen en esa migración: local → offline-rules; server/ai → auto, ya que no existe credencial explícita protegida que permita migrar a API personal. La URL se conserva como legacy-server-url y no se usa para red. Los ajustes antiguos continúan visibles como legado en Geode para permitir reversión a v0.3.8, pero no controlan el cliente nuevo.

Colores, fuentes, historial, favoritos, posición/opacidad de botón y secciones se mantienen. `ai-mode` es la fuente de verdad nueva. API personal requiere introducir una clave de sesión explícita. No se importan claves desde archivos del servidor.

Para volver a v0.3.8, instala el binario anterior: sus valores antiguos permanecen. Los backups JSON nuevos son independientes de los registros TXT viejos; estos últimos se conservan en disco y no se anuncian como recuperables.
