# AI Decorator – prueba básica (Geode, Android + Termux)

Mod mínimo que añade un botón **"IA"** en el editor de Geometry Dash. Al pulsarlo coloca un nivel simple (picos, bloques y escalones). Tiene dos modos:

- **local**: el mod genera el nivel por sí mismo. No usa red ni Termux. Sirve para comprobar que el mod carga y coloca objetos.
- **server**: el mod envía el prompt por HTTP a `server/server.py` y coloca lo que este devuelva. El servidor puede correr en Termux (mismo móvil) o en tu PC.

## Estructura

```
ai-decorator/
├── mod.json
├── CMakeLists.txt
├── src/main.cpp
├── server/server.py
└── .github/workflows/build.yml
```

## 1. Compilar el mod (sin instalar nada en el móvil)

La forma más fácil es GitHub Actions:

1. Crea un repositorio en GitHub y sube **todo** el contenido de esta carpeta (incluida `.github`).
2. Ve a la pestaña **Actions → Build Geode mod → Run workflow**.
3. Cuando termine, descarga el artefacto **Build Output**. Dentro viene el archivo `.geode` con Android 32 y 64 bits (y Windows para probar en PC).

Antes de subirlo, edita en `mod.json` el `id` (`tuusuario.ai-decorator`) y el `developer`.

Alternativa local: instalar el CLI de Geode, el SDK y el NDK de Android, y ejecutar `geode build --platform android64`.

## 2. Instalar en Android

1. Instala el launcher oficial de Geode (github.com/geode-sdk/android-launcher) sobre una copia de Geometry Dash.
2. Copia el `.geode` al móvil e impórtalo desde la lista de mods de Geode, o colócalo en la carpeta de mods de Geode.
3. Abre el juego, entra al editor y busca el botón **IA** arriba a la derecha.

## 3. Probar el modo local

Con el ajuste **Modo = local** (valor por defecto), pulsa **IA** en el editor. Deberían aparecer objetos a partir de x = 300. Si aparecen, el mod funciona.

## 4. Probar el modo Termux

En Termux:

```
pkg update
pkg install python
mkdir -p ~/ai && cd ~/ai
# copia aquí server.py (por ejemplo con termux-setup-storage y cp, o pegándolo con nano)
python server.py
```

Deja Termux abierto (conviene bloquear su notificación para que Android no lo cierre; `termux-wake-lock` ayuda).

Comprueba el servidor desde otra sesión de Termux:

```
curl -X POST --data "nivel de 10 segundos" http://127.0.0.1:8765/generate
```

Debe devolver algo como `1,8,2,300,3,15;1,8,2,540,3,15;...`.

Luego, en los ajustes del mod dentro de Geode:

- **Modo** = `server`
- **URL del servidor** = `http://127.0.0.1:8765/generate`

Abre el editor y pulsa **IA**. La primera vez verás "Pidiendo nivel a la IA..." y después los objetos.

## 5. Probar con el servidor en tu PC (alternativa)

Ejecuta `python server.py` en el PC, averigua su IP local (`ipconfig` / `ip a`) y pon en el ajuste `http://IP_DE_TU_PC:8765/generate`. Móvil y PC deben estar en la misma red Wi-Fi.

## Problemas conocidos a revisar

- **Tráfico HTTP sin cifrar:** Android puede bloquear `http://` según la versión. Si "Error del servidor" o la petición se cancela sin que el servidor reciba nada, ese es el primer sospechoso.
- **Termux cerrado por el sistema:** si el servidor muere en segundo plano, desactiva la optimización de batería para Termux.
- **Nombres de la API de Geode:** las clases `web::WebRequest` / `EventListener<web::WebTask>` y `createObjectsFromString` pueden variar según la versión del SDK o los bindings de Android. Si falla la compilación, pega aquí el error y lo ajusto.
- **Versión de GD:** `mod.json` apunta a 2.2074; cámbiala por la que soporte tu Geode.

## Siguiente paso

Cuando esto funcione, se reemplaza `generate_level()` en `server.py` por una llamada a un LLM con tool calling, y se añade al mod una herramienta para leer los objetos del editor y devolverlos a la IA.
