# Proveedores y credenciales

1. En ajustes de Geode del mod elige `ai-provider`: openai o groq, y el modelo real de ese proveedor. El modelo predeterminado es gpt-4.1-mini (OpenAI); para Groq debes cambiarlo, por ejemplo a llama-3.3-70b-versatile si sigue disponible.
2. En la tarjeta de conexión de DecoMind selecciona API personal. Introduce la clave en el campo enmascarado y pulsa GUARDAR.
3. Esa acción habilita solicitudes al proveedor elegido. El prompt y hasta 40 objetos seleccionados/visibles se comparten con ese proveedor. La API puede tener coste y condiciones propias. Nunca se envía el nivel completo ni el historial.
4. Para borrar la clave, deja el campo vacío y pulsa GUARDAR. Al salir del proceso también se pierde. Cambiar de proveedor no reutiliza la clave del anterior.
5. Elige Sin conexión para no usar red. `auto` informa de la falta de un proveedor automatico y no coloca patrones en sustitucion del pedido; `free` informa de la falta de servicio. En Sin conexion solo hay consejos consultivos y una prueba fija explicita con `/prueba-nativa`.

Endpoints fijos HTTPS, validación TLS predeterminada de Geode, timeout de 45 segundos, máximo de 4096 tokens solicitados, máximo de 1 MiB para procesar la respuesta recibida y 512 KiB para el plan. El límite de cuerpo se comprueba después de recibirlo; todavía no limita el buffer del transporte. Redirecciones HTTP desactivadas, sin fallback entre proveedores, sin retries automáticos. Errores 401/403,429,5xx y transporte se presentan sin imprimir cuerpos ni cabeceras.

Las claves no se guardan en settings, backups, prompts ni historial. Esto es almacenamiento **solo en sesión**, no un Android Keystore, ni una promesa de borrado criptográfico de todas las copias internas del cliente HTTPS. Integrar almacenamiento protegido es requisito pendiente para persistencia pública.

No se ha usado ninguna clave real ni una cuota de pago en las pruebas de esta rama. La compatibilidad en vivo, el coste efectivo y la disponibilidad de cada modelo requieren prueba explícita en el dispositivo.
