# Como funciona el selector de color de GD y el boton flotante (Verificado contra `libcocos2dcpp.so` ARM64)

Documento tecnico verificado mediante ingenieria inversa directa sobre `lib/arm64-v8a/libcocos2dcpp.so` (Geometry Dash 2.2081 / Geode v5.11.2) y `CCControlColourPickerSpriteSheet-hd.plist`.

---

## 1. Arquitectura de Dos Capas del Selector de Color Oficial

1. **`cocos2d::extension::CCControlColourPicker`** (`create` en `0xa9ce88`, `init` en `0xa9cb8c`, `568` bytes):
   - Rueda nativa compuesta por el anillo exterior de matiz (`CCControlHuePicker`, `0xa9d14c`) y el disco central de saturacion/brillo (`CCControlSaturationBrightnessPicker`, `0xa9d920`).
   - Guarda `m_colorValue` (`ccColor3B` en `+0x1f0` / `496`), `m_hsv` (`HSV` de 3 `double` en `+0x1f8` / `504`), `m_colourPicker` (`+0x210` / `528`), `m_huePicker` (`+0x218` / `536`), `m_background` (`+0x220` / `544`), `m_colorTarget` (`+0x228` / `552`) y `m_delegate` (`ColorPickerDelegate*` en `+0x230` / `560`).
2. **`ColorSelectPopup`** (`init` en `0x73d208`):
   - Popup del editor que instancia `CCControlColourPicker::create()` (`0x73d2dc`), conecta `ColorPickerDelegate` (`this + 0x3c8` en `[picker + 560]`), crea las dos muestras `whiteSquare60_001.png` (`m_colorSprite` en `+0x418` con escala `1.0` y `m_originalColorSprite` en `+0x420` con escala `0.5`) y anade los `CCTextInputNode` para `R:`, `G:`, `B:` y `HEX:` junto a los botones `Copy`, `Paste`, `Default` y `OK`.

---

## 2. Sprites Oficiales (`CCControlColourPickerSpriteSheet.png`)

Cargados mediante `CCSpriteBatchNode::create("CCControlColourPickerSpriteSheet.png", 29)` en `0xa9cbfc`:
- `menuColourPanelBackground.png` (`168x168` pt, con `setFlipX(true)` y desplazado `-1.75` pt en X).
- `huePickerBackground.png` (`140x140` pt, ubicado en `shift + (14, 8)`).
- `colourPickerBackground.png` (`100x100` pt, ubicado en `shift + (34, 28)`, tenido con el color de matiz puro `HSV(h, 1.0, 1.0)`).
- `colourPickerOverlay.png` (`100x100` pt, degradado radial de saturacion y brillo).
- `colourPickerShadow.png` (`106x106` pt, bisel interior).
- `colourPicker.png` (`26x26` pt, cursor circular tanto para el anillo como para el disco).

---

## 3. Flujo Matematico y Sincronizacion sin Bucles

- **Anillo de matiz (`CCControlHuePicker::checkSliderPosition` en `0xa9d500`)**:
  - Calcula `dist = sqrt((x + 10)^2 + y^2)`. En el toque inicial (`ccTouchBegan`), solo acepta `50.0 < dist < 85.0`.
  - Calcula el angulo con `atan2f(dy, dx)` convertido a `[0, 360)` grados y posiciona el cursor a radio `67.5` pt.
- **Disco de saturacion/brillo (`CCControlSaturationBrightnessPicker::checkSliderPosition` en `0xa9ded8`)**:
  - Acepta toques dentro del radio `width * 0.5` (`50` pt) y limita el cursor a un radio maximo de `35` pt (`m_boxSize = 35` en `+0x220`).
- **Pivote de escalado (`setAnchorPoint({0.f, 0.f})`)**:
  - Como `CCControlColourPicker` hereda de `CCControl -> CCLayer`, tiene `ignoreAnchorPointForPosition(true)` y `contentSize = (168, 168)`, pero sus hijos estan centrados en `(0, 0)`. Si se llama a `setScale(s)` con `anchorPoint = (0.5, 0.5)`, `CCNode::nodeToParentTransform` (`0xacb544`) escala respecto a `(84, 84)` y desplaza la rueda. Fijar `m_wheel->setAnchorPoint({0.f, 0.f})` hace que `setScale` escale exactamente sobre `(0, 0)`.
- **Guarda anti-recursion (`m_updatingInputs`)**:
  - `CCControlColourPicker::setColorValue` (`0xa9cab0`) invoca automaticamente a `m_delegate->colorValueChanged(m_colorValue)` (`0xa9cb60`).
  - Al igual que `ColorSelectPopup::updateColorLabels` (`0x74046c`, que usa `m_disableTextDelegate` en `+0x2cc` / `716`), `OfficialColorPickerPopup` utiliza `m_updatingInputs` para no sobrescribir el `TextInput` activo mientras el usuario escribe en `R`, `G`, `B` o `HEX`.

---

## 4. Boton Flotante del Editor (`DragButton` en `src/EditorButton.hpp`)

Verificado contra `CCLayer::init` (`0xaea00c`), `CCLayer::onEnter` (`0xaec3d4`) y `CCTouchDispatcher::addPrioTargetedDelegate` (`0xb3ecb4`):
- En `CCLayer::init()`, `m_bTouchEnabled` (`[this + 360]`) se inicializa en `false`. Es obligatorio llamar a `setTouchMode(kCCTouchesOneByOne)` y `setTouchEnabled(true)` para que `CCLayer::onEnter()` registre el delegado tactil.
- Se configura `ignoreAnchorPointForPosition(false)` y `setAnchorPoint({0.5f, 0.5f})` para que `setPosition()` represente el centro geometrico real del boton.
- Registra prioridad tactil `-250`: por encima de `EditorUI` (`0`) y sus `CCMenu` (`-128`), pero por debajo de cualquier modal `FLAlertLayer` / `Popup` (`-501` o menor con `ForcePrio`), comprobando ademas que no haya popups abiertos ni modo Playtest activo.
