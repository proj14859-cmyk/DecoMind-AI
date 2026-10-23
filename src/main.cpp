// ===========================================================================
// DecoMind AI v0.3 - menu del editor
//
// Estructura del menu (ver imagen de concepto):
//   - Barra superior: flecha verde (cerrar), logo y nombre.
//   - Izquierda: 5 botones (Inicio, Modos, Chat, Estadisticas, Ajustes) que
//     cambian el contenido del centro; el activo se ilumina.
//   - Centro: pagina activa. "Inicio" = lista deslizante de modos; cada modo
//     lleva un prompt de contexto que se envia a la IA.
//   - Derecha: uso de la IA, CHAT / COMANDO, HISTORIAL (con boton para abrir
//     el historial completo) y un letrero RGB con frases de un .txt.
//   - Abajo: barra oculta (Favoritos, Tutoriales, Mis niveles, Guardar,
//     Reiniciar) que se despliega con una flechita.
//
// Nota: las fuentes de GD no dibujan tildes ni la enie, por eso todo el texto
// del juego va en ASCII.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Paleta (de tu imagen de referencia)
// ---------------------------------------------------------------------------
static const ccColor3B C_NAVY  = ccc3(0x06, 0x1a, 0x32);
static const ccColor3B C_PANEL = ccc3(0x00, 0x2d, 0x64);
static const ccColor3B C_CARD  = ccc3(0x01, 0x39, 0x68);
static const ccColor3B C_BLUE  = ccc3(0x16, 0x59, 0xe8);
static const ccColor3B C_CYAN  = ccc3(0x30, 0xb7, 0xee);
static const ccColor3B C_SKY   = ccc3(0x6a, 0xd2, 0xf5);
static const ccColor3B C_ICE   = ccc3(0xb2, 0xed, 0xff);
static const ccColor3B C_DIM   = ccc3(0x46, 0x69, 0xa5);
static const ccColor3B C_WHITE = ccc3(255, 255, 255);

static ccColor4B rgba(ccColor3B c, int a) {
    return ccc4(c.r, c.g, c.b, static_cast<GLubyte>(a));
}

static constexpr float PW = 520.f; // ancho del menu principal
static constexpr float PH = 300.f; // alto del menu principal

// ---------------------------------------------------------------------------
// Modos. Cada modo lleva un prompt de contexto (ctx) que le dice a la IA en
// que debe ayudar exactamente. Se envia al servidor junto con la peticion.
// ---------------------------------------------------------------------------
struct ModeInfo {
    const char* id;
    const char* name;
    const char* tag;   // abreviatura del icono
    const char* desc;  // descripcion corta (tarjeta)
    const char* ctx;   // prompt de contexto para la IA
    ccColor3B color;   // color de acento
};

static const ModeInfo MODES[9] = {
    {"estructurar", "Estructurar", "ES",
     "Organiza grupos, capas y triggers",
     "MODO ESTRUCTURAR. Analiza el nivel indicado (grupos, capas, canales de color, triggers, secciones) "
     "y organizalo: agrupa objetos relacionados, ordena capas, elimina triggers redundantes y libera IDs "
     "de grupo sin usar. No cambies la jugabilidad (colisiones, hitboxes, tiempos). Responde con un plan "
     "breve y despues con las operaciones a realizar.",
     ccc3(0x1f, 0xd1, 0xb0)},
    {"layout", "Layout", "LY",
     "Crea la jugabilidad del nivel",
     "MODO LAYOUT. Crea o rehaz SOLO la jugabilidad (bloques, picos, portales, pads, aros) con un ritmo "
     "coherente, para el modo de juego pedido. Respeta distancias y alturas superables, evita secciones "
     "imposibles y no anadas decoracion. Verifica que cada salto sea posible.",
     ccc3(0x1a, 0x6b, 0xff)},
    {"deco", "Deco", "DC",
     "Decora sin tocar la jugabilidad",
     "MODO DECO. Decora SIN modificar la jugabilidad: no muevas, borres ni cambies bloques solidos, picos "
     "ni triggers que afecten colisiones. Usa solo objetos decorativos, colores y capas (z-order) "
     "coherentes con el estilo pedido. Respeta los grupos y canales de color que ya estan en uso.",
     ccc3(0xa0, 0x4c, 0xff)},
    {"layout_deco", "Layout + Deco", "L+D",
     "Jugabilidad y deco en dos pasadas",
     "MODO LAYOUT + DECO. Trabaja en dos pasadas: 1) crea la jugabilidad como en LAYOUT y comprueba que "
     "sea superable; 2) decora encima como en DECO sin alterar la jugabilidad ya validada. Informa de "
     "cada pasada por separado.",
     ccc3(0xff, 0xc8, 0x3d)},
    {"block_design", "Block Design", "BL",
     "Disena bloques y estructuras propias",
     "MODO BLOCK DESIGN. Disena bloques y estructuras personalizadas (bordes, relleno, esquinas, detalles) "
     "con un estilo consistente. La forma jugable (hitbox) debe quedar identica a la original. Reutiliza "
     "piezas y agrupalas para poder duplicarlas.",
     ccc3(0xff, 0x4d, 0x6d)},
    {"triggers", "Triggers", "TG",
     "Configura triggers automaticamente",
     "MODO TRIGGERS. Configura triggers (movimiento, color, pulso, alfa, toggle, spawn, etc.) para lograr "
     "el efecto pedido. Antes de crear grupos o canales de color, consulta cuales estan en uso y elige "
     "libres. Explica el orden y el tiempo de cada trigger.",
     ccc3(0x2c, 0xe0, 0xf0)},
    {"transiciones", "Transiciones", "TR",
     "Transiciones suaves entre zonas",
     "MODO TRANSICIONES. Crea transiciones suaves entre dos zonas o estilos del nivel (colores, fondo, "
     "escenario, modo de juego) usando triggers y decoracion, sin alterar la jugabilidad ni el tiempo de "
     "las secciones.",
     ccc3(0x8a, 0x5c, 0xff)},
    {"efectos", "Efectos avanzados", "FX",
     "Efectos especiales y movimiento",
     "MODO EFECTOS AVANZADOS. Anade efectos especiales y movimiento (particulas, pulsos al ritmo, shake, "
     "parallax, animaciones de mover/rotar/escalar) de forma ordenada y sin saturar el nivel. No cambies "
     "la jugabilidad.",
     ccc3(0xff, 0x8a, 0x1f)},
    {"ideas", "Ideas", "ID",
     "Ideas y consejos, sin tocar el nivel",
     "MODO IDEAS. No modifiques el nivel. Analiza lo que hay y propone ideas, mejoras y consejos concretos "
     "(ritmo, contraste, composicion, deco, triggers) ordenados de mayor a menor impacto.",
     ccc3(0x4d, 0xa6, 0xff)},
};

static const char* PAGE_NAMES[5] = {"Inicio", "Modos", "Chat", "Estadisticas", "Ajustes"};

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
static std::string trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string oneLine(std::string s) {
    for (auto& c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    return s;
}

static ccColor3B hsvToRgb(float h, float s, float v) {
    h = h - std::floor(h);
    int i = static_cast<int>(h * 6.f);
    float f = h * 6.f - static_cast<float>(i);
    float p = v * (1.f - s);
    float q = v * (1.f - f * s);
    float t = v * (1.f - (1.f - f) * s);
    float r = v, g = t, b = p;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return ccc3(
        static_cast<GLubyte>(r * 255.f),
        static_cast<GLubyte>(g * 255.f),
        static_cast<GLubyte>(b * 255.f)
    );
}

// Ajusta un texto a un ancho maximo reduciendo la escala.
static void fitLabel(CCLabelBMFont* l, std::string const& text, float maxW, float scale) {
    l->setString(text.c_str());
    l->setScale(scale);
    l->limitLabelWidth(maxW, scale, .12f);
}

static int modeIndexById(std::string const& id) {
    for (int i = 0; i < 9; ++i) {
        if (id == MODES[i].id) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Generador LOCAL (origen "local"): sin red, solo para probar.
// Formato de objeto de GD: "1,<id>,2,<x>,3,<y>;"  (id 1 = bloque, 8 = pico)
// ---------------------------------------------------------------------------
static std::string makeLocalLevel(int sections) {
    std::string s;
    auto obj = [&](int id, int x, int y) {
        s += fmt::format("1,{},2,{},3,{};", id, x, y);
    };

    int x = 300;
    for (int i = 0; i < sections; ++i) {
        switch (i % 4) {
            case 0:
                obj(8, x, 15);
                x += 240;
                break;
            case 1:
                obj(8, x, 15);
                obj(8, x + 30, 15);
                x += 270;
                break;
            case 2:
                obj(1, x, 15);
                obj(8, x + 90, 15);
                x += 270;
                break;
            case 3:
                obj(1, x, 15);
                obj(1, x + 30, 15);
                obj(1, x + 30, 45);
                obj(1, x + 60, 15);
                obj(1, x + 60, 45);
                obj(1, x + 60, 75);
                x += 330;
                break;
        }
    }
    return s;
}

// Busca una linea "CLAVE:valor" en la respuesta del servidor.
static std::string getField(std::string const& body, std::string const& key) {
    std::string prefix = key + ":";
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        std::string line = body.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
        pos = end + 1;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Historial (se guarda en la carpeta de datos del mod: historial.txt)
// Formato por linea: <id del modo> TAB <peticion>
// ---------------------------------------------------------------------------
struct HistoryEntry {
    std::string modeId;
    std::string prompt;
};

static std::vector<HistoryEntry>& historyList() {
    static std::vector<HistoryEntry> h;
    return h;
}

static std::filesystem::path historyPath() {
    return Mod::get()->getSaveDir() / "historial.txt";
}

static void loadHistory() {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    auto res = utils::file::readString(historyPath());
    if (!res.isOk()) return;
    std::string text = res.unwrap();

    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        historyList().push_back({line.substr(0, tab), line.substr(tab + 1)});
    }
}

static void saveHistory() {
    auto& h = historyList();
    if (h.size() > 100) {
        h.erase(h.begin(), h.begin() + static_cast<std::ptrdiff_t>(h.size() - 100));
    }
    std::string out;
    for (auto const& e : h) {
        out += e.modeId + "\t" + e.prompt + "\n";
    }
    (void)utils::file::writeString(historyPath(), out);
}

// ---------------------------------------------------------------------------
// Frases del letrero RGB (se leen de frases.txt, una por linea)
// ---------------------------------------------------------------------------
static std::vector<std::string> defaultPhrases() {
    return {
        "Tu aliado en el editor de Geometry Dash!",
        "Una buena deco empieza con un buen layout.",
        "Menos es mas: deja respirar tu nivel.",
        "Prueba un pulso suave al ritmo de la musica.",
        "Contraste: fondo oscuro, detalles claros.",
        "Guarda una copia antes de cada cambio grande.",
        "Los triggers ordenados ahorran dolores de cabeza.",
        "Cada grupo libre es una oportunidad.",
        "Tip: pide ideas en el modo Ideas.",
        "La IA propone, tu decides.",
    };
}

static std::filesystem::path phrasesPath() {
    return Mod::get()->getSaveDir() / "frases.txt";
}

static std::vector<std::string> loadPhrases() {
    std::vector<std::string> out;
    auto path = phrasesPath();

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::string def =
            "# Frases del letrero de DecoMind AI. Una por linea.\n"
            "# Las lineas que empiezan con # se ignoran. Usa solo letras sin tilde.\n";
        for (auto const& p : defaultPhrases()) def += p + "\n";
        (void)utils::file::writeString(path, def);
    }

    auto res = utils::file::readString(path);
    if (res.isOk()) {
        std::string text = res.unwrap();
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string line = trim(text.substr(pos, end - pos));
            pos = end + 1;
            if (line.empty() || line[0] == '#') continue;
            out.push_back(line);
        }
    }
    if (out.empty()) out = defaultPhrases();
    return out;
}

// ---------------------------------------------------------------------------
// Base de los popups de DecoMind: marco propio con la paleta azul
// ---------------------------------------------------------------------------
class DMPopupBase : public Popup {
protected:
    CCMenu* m_ui = nullptr; // menu de botones fijos (z alto)
    float m_w = 0.f;
    float m_h = 0.f;

    CCLayerColor* fillRect(float x, float y, float w, float h, ccColor4B c, int z = 1) {
        auto l = CCLayerColor::create(c, w, h);
        l->setPosition({x, y});
        m_mainLayer->addChild(l, z);
        return l;
    }

    // Tarjeta: borde azul + relleno oscuro.
    void card(float x, float y, float w, float h, int z = 1) {
        this->fillRect(x, y, w, h, rgba(C_BLUE, 255), z);
        this->fillRect(x + 1.5f, y + 1.5f, w - 3.f, h - 3.f, rgba(C_PANEL, 245), z);
    }

    CCLabelBMFont* makeLabel(
        CCNode* parent, char const* text, char const* font, float scale,
        CCPoint pos, ccColor3B col, CCPoint anchor = ccp(0.f, 0.5f), int z = 10
    ) {
        auto l = CCLabelBMFont::create(text, font);
        l->setScale(scale);
        l->setColor(col);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        parent->addChild(l, z);
        return l;
    }

    CCMenuItemSpriteExtra* addButton(
        CCMenu* menu, CCNode* spr, CCPoint pos, std::function<void()> cb
    ) {
        auto btn = CCMenuItemExt::createSpriteExtra(
            spr, [cb](CCMenuItemSpriteExtra*) { cb(); }
        );
        btn->setPosition(pos);
        menu->addChild(btn);
        return btn;
    }

    ButtonSprite* makeButtonSpr(char const* text, int width, float height, float scale) {
        return ButtonSprite::create(
            text, width, true, "bigFont.fnt", "GJ_button_02.png", height, scale
        );
    }

    bool initFrame(float w, float h) {
        if (!Popup::init(w, h)) return false;
        m_w = w;
        m_h = h;

        // Se oculta el fondo marron y la X por defecto: el marco es propio.
        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        this->fillRect(0.f, 0.f, w, h, rgba(C_CYAN, 255));
        this->fillRect(2.f, 2.f, w - 4.f, h - 4.f, rgba(C_NAVY, 250));

        m_ui = CCMenu::create();
        m_ui->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_ui, 60);

        // Flecha verde de GD para cerrar (arriba a la izquierda).
        auto arrow = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        arrow->setScale(.75f);
        this->addButton(m_ui, arrow, ccp(24.f, h - 22.f), [this]() { this->onClose(nullptr); });
        return true;
    }
};

// ---------------------------------------------------------------------------
// Historial completo
// ---------------------------------------------------------------------------
class HistoryPopup : public DMPopupBase {
protected:
    std::function<void(int, std::string const&)> m_onPick;
    std::function<void()> m_onClear;
    ScrollLayer* m_scroll = nullptr;

    bool init(
        std::function<void(int, std::string const&)> onPick,
        std::function<void()> onClear
    ) {
        if (!this->initFrame(300.f, 230.f)) return false;
        m_onPick = std::move(onPick);
        m_onClear = std::move(onClear);

        this->makeLabel(m_mainLayer, "HISTORIAL COMPLETO", "bigFont.fnt", .5f,
                        ccp(150.f, 208.f), C_ICE, ccp(.5f, .5f));
        this->fillRect(8.f, 194.f, 284.f, 1.f, rgba(C_CYAN, 120));

        this->card(10.f, 44.f, 280.f, 146.f);
        m_scroll = ScrollLayer::create(CCSize{268.f, 134.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({150.f, 117.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto clearSpr = this->makeButtonSpr("Borrar historial", 130, 26.f, .4f);
        this->addButton(m_ui, clearSpr, ccp(150.f, 24.f), [this]() {
            if (m_onClear) m_onClear();
            this->buildList();
        });

        this->buildList();
        return true;
    }

    void buildList() {
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto const& h = historyList();
        float viewW = 268.f;
        float viewH = 134.f;

        if (h.empty()) {
            layer->setContentSize({viewW, viewH});
            this->makeLabel(layer, "Aun no hay peticiones.", "chatFont.fnt", .6f,
                            ccp(viewW / 2.f, viewH / 2.f), C_ICE, ccp(.5f, .5f));
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 38.f;
        float gap = 5.f;
        float total = 4.f + static_cast<float>(h.size()) * (itemH + gap);
        layer->setContentSize({viewW, std::max(viewH, total)});
        total = std::max(viewH, total);

        // El mas reciente arriba.
        int row = 0;
        for (int k = static_cast<int>(h.size()) - 1; k >= 0; --k, ++row) {
            auto const& e = h[k];
            int mi = modeIndexById(e.modeId);
            ccColor3B accent = mi >= 0 ? MODES[mi].color : C_CYAN;
            std::string modeName = mi >= 0 ? MODES[mi].name : e.modeId;

            auto node = CCNode::create();
            node->setContentSize({256.f, itemH});
            auto bg = CCLayerColor::create(rgba(C_CARD, 235), 256.f, itemH);
            node->addChild(bg);
            auto bar = CCLayerColor::create(rgba(accent, 255), 4.f, itemH);
            node->addChild(bar);
            auto l1 = CCLabelBMFont::create("", "bigFont.fnt");
            l1->setAnchorPoint({0.f, .5f});
            l1->setColor(accent);
            l1->setPosition({12.f, 27.f});
            fitLabel(l1, modeName, 230.f, .32f);
            node->addChild(l1);
            auto l2 = CCLabelBMFont::create("", "chatFont.fnt");
            l2->setAnchorPoint({0.f, .5f});
            l2->setColor(C_ICE);
            l2->setPosition({12.f, 11.f});
            fitLabel(l2, e.prompt, 236.f, .5f);
            node->addChild(l2);

            std::string prompt = e.prompt;
            auto menu = CCMenu::create();
            menu->setContentSize({viewW, itemH});
            menu->setPosition({0.f, total - 4.f - static_cast<float>(row) * (itemH + gap) - itemH});
            auto btn = CCMenuItemExt::createSpriteExtra(node, [this, mi, prompt](CCMenuItemSpriteExtra*) {
                if (m_onPick) m_onPick(mi, prompt);
                this->onClose(nullptr);
            });
            btn->setPosition({viewW / 2.f, itemH / 2.f});
            menu->addChild(btn);
            layer->addChild(menu);
        }
        m_scroll->scrollToTop();
    }

public:
    static HistoryPopup* create(
        std::function<void(int, std::string const&)> onPick,
        std::function<void()> onClear
    ) {
        auto ret = new HistoryPopup();
        if (ret->init(std::move(onPick), std::move(onClear))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Menu principal
// ---------------------------------------------------------------------------
struct ChatLine {
    bool user;
    std::string text;
};

class DecoMindPopup : public DMPopupBase {
protected:
    Ref<LevelEditorLayer> m_lel;
    int m_mode = 2; // Deco por defecto
    int m_page = 0;
    bool m_busy = false;
    int m_pending = -1; // indice del mensaje "Pensando..." en m_chat
    std::vector<ChatLine> m_chat;
    int m_sessionReq = 0;
    int m_sessionObjs = 0;

    // Estructura
    CCNode* m_pages[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    std::vector<ButtonSprite*> m_sideSprites;

    // Pagina Inicio
    std::vector<CCLayerColor*> m_cardBg;
    std::vector<CCLayerColor*> m_cardBorder;

    // Pagina Modos
    CCLabelBMFont* m_detName = nullptr;
    CCLabelBMFont* m_detDesc = nullptr;
    ScrollLayer* m_ctxScroll = nullptr;
    SimpleTextArea* m_ctxArea = nullptr;

    // Pagina Chat
    ScrollLayer* m_chatScroll = nullptr;
    SimpleTextArea* m_chatArea = nullptr;

    // Pagina Estadisticas
    CCLabelBMFont* m_stat[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    // Pagina Ajustes
    ButtonSprite* m_originSpr = nullptr;
    TextInput* m_urlInput = nullptr;
    CCLabelBMFont* m_secLabel = nullptr;
    CCLabelBMFont* m_pathLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;

    // Panel derecho
    CCLayerColor* m_barFill = nullptr;
    CCLabelBMFont* m_usageLabel = nullptr;
    CCLabelBMFont* m_modeLabel = nullptr;
    TextInput* m_cmdInput = nullptr;
    CCLabelBMFont* m_hist[2] = {nullptr, nullptr};

    // Barra inferior oculta
    CCNode* m_bar = nullptr;
    CCSprite* m_handle = nullptr;
    bool m_barOpen = false;

    // Letrero RGB
    CCLayerColor* m_signBorder = nullptr;
    SimpleTextArea* m_signText = nullptr;
    std::vector<std::string> m_phrases;
    size_t m_phraseIdx = 0;
    float m_hue = 0.f;
    float m_phraseTimer = 0.f;
    float m_colorTimer = 0.f;

    async::TaskHolder<web::WebResponse> m_task;

    static constexpr float BAR_W = 138.f;

    // ------------------------------------------------------------------
    bool init(LevelEditorLayer* lel) {
        if (!this->initFrame(PW, PH)) return false;
        m_lel = lel;
        loadHistory();

        // Si la pantalla es pequena, se reduce todo el menu para que quepa.
        auto win = CCDirector::get()->getWinSize();
        float s = std::min(1.f, std::min((win.width - 8.f) / PW, (win.height - 4.f) / PH));
        m_mainLayer->setScale(s);

        this->buildTopBar();
        this->buildSidebar();

        // Tarjetas de fondo
        this->card(120.f, 52.f, 230.f, 202.f);   // centro
        this->card(358.f, 192.f, 154.f, 62.f);   // uso
        this->card(358.f, 112.f, 154.f, 76.f);   // chat / comando
        this->card(358.f, 52.f, 154.f, 56.f);    // historial

        for (int i = 0; i < 5; ++i) {
            m_pages[i] = CCNode::create();
            m_mainLayer->addChild(m_pages[i], 5);
        }
        this->buildInicio();
        this->buildModos();
        this->buildChat();
        this->buildStats();
        this->buildAjustes();

        this->buildRightPanel();
        this->buildBottomBar();
        this->buildSign();

        this->selectMode(m_mode);
        this->showPage(0);
        this->refreshUsage();
        this->refreshHistoryMini();
        this->refreshStats();
        this->refreshChat();

        this->schedule(schedule_selector(DecoMindPopup::tick));
        return true;
    }

    // ------------------------------------------------------------------
    // Barra superior
    // ------------------------------------------------------------------
    void buildTopBar() {
        this->fillRect(8.f, 259.f, PW - 16.f, 1.f, rgba(C_CYAN, 120));
        if (auto icon = CCSprite::create("decomind-icon.png"_spr)) {
            icon->setScale(30.f / icon->getContentWidth());
            icon->setPosition({190.f, PH - 20.f});
            m_mainLayer->addChild(icon, 10);
        }
        this->makeLabel(m_mainLayer, "DecoMind AI", "bigFont.fnt", .55f,
                        ccp(209.f, PH - 20.f), C_ICE);
    }

    // ------------------------------------------------------------------
    // Menu lateral
    // ------------------------------------------------------------------
    void buildSidebar() {
        for (int i = 0; i < 5; ++i) {
            auto spr = this->makeButtonSpr(PAGE_NAMES[i], 100, 30.f, .36f);
            m_sideSprites.push_back(spr);
            this->addButton(m_ui, spr, ccp(60.f, 234.f - static_cast<float>(i) * 40.f), [this, i]() {
                this->showPage(i);
            });
        }
    }

    void showPage(int p) {
        m_page = p;
        for (int i = 0; i < 5; ++i) {
            m_pages[i]->setVisible(i == p);
            m_sideSprites[i]->setColor(i == p ? C_WHITE : C_DIM);
        }
        // El campo de URL esta en una pagina oculta: se desactiva para que
        // no capture toques cuando no se ve.
        if (m_urlInput) m_urlInput->setEnabled(p == 4);
        if (p == 2) this->refreshChat();
        if (p == 3) this->refreshStats();
    }

    // ------------------------------------------------------------------
    // Pagina 0: Inicio = lista deslizante de modos
    // ------------------------------------------------------------------
    void buildInicio() {
        auto page = m_pages[0];
        this->makeLabel(page, "ELIGE UN MODO", "bigFont.fnt", .38f, ccp(130.f, 243.f), C_SKY);
        this->makeLabel(page, "desliza", "chatFont.fnt", .45f, ccp(344.f, 243.f), C_DIM, ccp(1.f, .5f));

        float viewW = 218.f;
        float viewH = 176.f;
        auto scroll = ScrollLayer::create(CCSize{viewW, viewH});
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition({120.f + 115.f, 56.f + 88.f});
        page->addChild(scroll, 5);

        float itemW = 206.f;
        float itemH = 50.f;
        float gap = 6.f;
        float total = 4.f + 9.f * (itemH + gap);
        scroll->m_contentLayer->setContentSize({viewW, total});

        for (int i = 0; i < 9; ++i) {
            auto const& m = MODES[i];

            auto node = CCNode::create();
            node->setContentSize({itemW, itemH});

            auto border = CCLayerColor::create(rgba(C_SKY, 255), itemW, itemH);
            node->addChild(border);
            auto bg = CCLayerColor::create(rgba(C_CARD, 240), itemW - 4.f, itemH - 4.f);
            bg->setPosition({2.f, 2.f});
            node->addChild(bg);

            auto accent = CCLayerColor::create(rgba(m.color, 255), 4.f, itemH - 4.f);
            accent->setPosition({2.f, 2.f});
            node->addChild(accent);

            auto iconBg = CCLayerColor::create(rgba(m.color, 90), 30.f, 30.f);
            iconBg->setPosition({10.f, 10.f});
            node->addChild(iconBg);
            auto tag = CCLabelBMFont::create(m.tag, "bigFont.fnt");
            tag->setScale(.4f);
            tag->setColor(m.color);
            tag->setPosition({25.f, 25.f});
            node->addChild(tag);

            auto name = CCLabelBMFont::create("", "bigFont.fnt");
            name->setAnchorPoint({0.f, .5f});
            name->setColor(C_WHITE);
            name->setPosition({46.f, 33.f});
            fitLabel(name, m.name, 150.f, .4f);
            node->addChild(name);

            auto desc = CCLabelBMFont::create("", "chatFont.fnt");
            desc->setAnchorPoint({0.f, .5f});
            desc->setColor(C_ICE);
            desc->setPosition({46.f, 15.f});
            fitLabel(desc, m.desc, 150.f, .5f);
            node->addChild(desc);

            m_cardBg.push_back(bg);
            m_cardBorder.push_back(border);

            // Un menu por tarjeta: el contenedor oculta solo las que quedan
            // fuera de la zona visible, asi no se pueden tocar sin querer.
            auto menu = CCMenu::create();
            menu->setContentSize({viewW, itemH});
            menu->setPosition({0.f, total - 4.f - static_cast<float>(i) * (itemH + gap) - itemH});
            auto btn = CCMenuItemExt::createSpriteExtra(node, [this, i](CCMenuItemSpriteExtra*) {
                this->selectMode(i);
            });
            btn->setPosition({viewW / 2.f, itemH / 2.f});
            menu->addChild(btn);
            scroll->m_contentLayer->addChild(menu);
        }
        scroll->scrollToTop();
    }

    void selectMode(int i) {
        m_mode = i;
        for (int k = 0; k < static_cast<int>(m_cardBg.size()); ++k) {
            bool sel = (k == i);
            m_cardBg[k]->setColor(sel ? C_BLUE : C_CARD);
            m_cardBorder[k]->setColor(sel ? C_SKY : C_BLUE);
            m_cardBorder[k]->setOpacity(sel ? 255 : 0);
        }
        auto const& m = MODES[i];
        if (m_detName) {
            fitLabel(m_detName, m.name, 206.f, .55f);
            m_detName->setColor(m.color);
        }
        if (m_detDesc) fitLabel(m_detDesc, m.desc, 206.f, .55f);
        if (m_ctxArea) this->fillScrollText(m_ctxScroll, m_ctxArea, m.ctx);
        if (m_modeLabel) fitLabel(m_modeLabel, std::string("Modo: ") + m.name, 138.f, .5f);
        this->refreshStats();
    }

    // Crea un texto largo dentro de un ScrollLayer. Va dentro de un contenedor
    // con tamano propio para que el scroll calcule bien que parte se ve.
    SimpleTextArea* makeScrollText(ScrollLayer* sc, float width, float scale) {
        auto holder = CCNode::create();
        holder->setContentSize({sc->getContentWidth(), sc->getContentHeight()});
        sc->m_contentLayer->addChild(holder);

        auto area = SimpleTextArea::create(" ", "chatFont.fnt", scale, width);
        area->setWrappingMode(WORD_WRAP);
        area->setColor(rgba(C_ICE, 255));
        area->setAnchorPoint({0.f, 1.f});
        holder->addChild(area);
        return area;
    }

    // Cambia el texto y lo deja arriba del todo.
    void fillScrollText(ScrollLayer* sc, SimpleTextArea* area, std::string const& text) {
        area->setText(text.empty() ? std::string(" ") : text);
        float pad = 6.f;
        float viewH = sc->getContentHeight();
        float w = sc->getContentWidth();
        float h = std::max(viewH, area->getHeight() + pad * 2.f);
        sc->m_contentLayer->setContentSize({w, h});
        area->getParent()->setContentSize({w, h});
        area->setPosition({pad, h - pad});
        sc->scrollToTop();
    }

    // ------------------------------------------------------------------
    // Pagina 1: Modos = detalle del modo activo y su prompt de contexto
    // ------------------------------------------------------------------
    void buildModos() {
        auto page = m_pages[1];
        this->makeLabel(page, "MODO ACTIVO", "bigFont.fnt", .38f, ccp(130.f, 243.f), C_SKY);
        m_detName = this->makeLabel(page, "", "bigFont.fnt", .55f, ccp(130.f, 222.f), C_WHITE);
        m_detDesc = this->makeLabel(page, "", "chatFont.fnt", .55f, ccp(130.f, 203.f), C_ICE);
        this->makeLabel(page, "Contexto que recibe la IA:", "chatFont.fnt", .5f,
                        ccp(130.f, 186.f), C_SKY);

        m_ctxScroll = ScrollLayer::create(CCSize{218.f, 100.f});
        m_ctxScroll->ignoreAnchorPointForPosition(false);
        m_ctxScroll->setPosition({235.f, 124.f});
        page->addChild(m_ctxScroll, 5);
        m_ctxArea = this->makeScrollText(m_ctxScroll, 204.f, .45f);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);
        this->addButton(menu, this->makeButtonSpr("Cambiar modo", 120, 26.f, .4f),
                        ccp(235.f, 72.f), [this]() { this->showPage(0); });
    }

    // ------------------------------------------------------------------
    // Pagina 2: Chat = conversacion (lo mas reciente arriba)
    // ------------------------------------------------------------------
    void buildChat() {
        auto page = m_pages[2];
        this->makeLabel(page, "CONVERSACION", "bigFont.fnt", .38f, ccp(130.f, 243.f), C_SKY);
        this->makeLabel(page, "lo nuevo arriba", "chatFont.fnt", .45f,
                        ccp(344.f, 243.f), C_DIM, ccp(1.f, .5f));

        m_chatScroll = ScrollLayer::create(CCSize{218.f, 176.f});
        m_chatScroll->ignoreAnchorPointForPosition(false);
        m_chatScroll->setPosition({235.f, 144.f});
        page->addChild(m_chatScroll, 5);
        m_chatArea = this->makeScrollText(m_chatScroll, 204.f, .45f);
    }

    void addChat(bool user, std::string const& text) {
        m_chat.push_back({user, text});
        if (m_chat.size() > 40) {
            m_chat.erase(m_chat.begin());
            if (m_pending >= 0) --m_pending;
        }
        this->refreshChat();
    }

    void refreshChat() {
        if (!m_chatArea) return;
        std::string out;
        if (m_chat.empty()) {
            out = "Aun no hay mensajes. Elige un modo en Inicio y escribe tu peticion en CHAT / COMANDO.";
        } else {
            for (int i = static_cast<int>(m_chat.size()) - 1; i >= 0; --i) {
                out += (m_chat[i].user ? "TU: " : "IA: ") + m_chat[i].text + "\n\n";
            }
        }
        this->fillScrollText(m_chatScroll, m_chatArea, out);
    }

    // ------------------------------------------------------------------
    // Pagina 3: Estadisticas
    // ------------------------------------------------------------------
    void buildStats() {
        auto page = m_pages[3];
        this->makeLabel(page, "ESTADISTICAS", "bigFont.fnt", .38f, ccp(130.f, 243.f), C_SKY);
        for (int i = 0; i < 6; ++i) {
            m_stat[i] = this->makeLabel(page, "", "chatFont.fnt", .6f,
                                        ccp(132.f, 218.f - static_cast<float>(i) * 24.f), C_ICE);
        }
    }

    void refreshStats() {
        if (!m_stat[0]) return;
        auto source = Mod::get()->getSettingValue<std::string>("mode");
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        int total = Mod::get()->getSavedValue<int>("total", 0);
        std::string lines[6] = {
            "Origen: " + std::string(source == "local" ? "local (prueba)" : "servidor"),
            "Modo activo: " + std::string(MODES[m_mode].name),
            source == "local" ? std::string("Limite diario: sin limite (local)")
                              : fmt::format("Peticiones hoy: {} / {}", used, limit),
            fmt::format("Peticiones esta sesion: {}", m_sessionReq),
            fmt::format("Peticiones totales: {}", total),
            fmt::format("Objetos colocados (sesion): {}", m_sessionObjs),
        };
        for (int i = 0; i < 6; ++i) fitLabel(m_stat[i], lines[i], 210.f, .6f);
    }

    // ------------------------------------------------------------------
    // Pagina 4: Ajustes de la IA
    // ------------------------------------------------------------------
    void buildAjustes() {
        auto page = m_pages[4];
        this->makeLabel(page, "AJUSTES DE LA IA", "bigFont.fnt", .38f, ccp(130.f, 243.f), C_SKY);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        // Origen: local / servidor
        this->makeLabel(page, "Origen de la IA", "chatFont.fnt", .55f, ccp(130.f, 218.f), C_ICE);
        auto source = Mod::get()->getSettingValue<std::string>("mode");
        m_originSpr = this->makeButtonSpr(source == "local" ? "Local" : "Servidor", 84, 26.f, .4f);
        this->addButton(menu, m_originSpr, ccp(302.f, 218.f), [this]() { this->toggleOrigin(); });

        // URL del servidor
        this->makeLabel(page, "URL del servidor", "chatFont.fnt", .55f, ccp(130.f, 192.f), C_ICE);
        m_urlInput = TextInput::create(166.f, "http://IP:8765/generate", "chatFont.fnt");
        m_urlInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.:-_");
        m_urlInput->setMaxCharCount(90);
        m_urlInput->setString(Mod::get()->getSettingValue<std::string>("server-url"));
        m_urlInput->setPosition({216.f, 168.f});
        page->addChild(m_urlInput, 10);
        this->addButton(menu, this->makeButtonSpr("OK", 44, 26.f, .4f), ccp(325.f, 168.f),
                        [this]() { this->saveUrl(); });

        // Secciones del origen local
        this->makeLabel(page, "Secciones (local)", "chatFont.fnt", .55f, ccp(130.f, 136.f), C_ICE);
        this->addButton(menu, this->makeButtonSpr("-", 28, 26.f, .5f), ccp(266.f, 136.f),
                        [this]() { this->changeSections(-1); });
        m_secLabel = this->makeLabel(page, "", "bigFont.fnt", .5f, ccp(296.f, 136.f), C_WHITE, ccp(.5f, .5f));
        this->addButton(menu, this->makeButtonSpr("+", 28, 26.f, .5f), ccp(326.f, 136.f),
                        [this]() { this->changeSections(+1); });
        this->updateSecLabel();

        // Frases del letrero
        this->makeLabel(page, "Frases del letrero (txt)", "chatFont.fnt", .55f, ccp(130.f, 108.f), C_ICE);
        m_pathLabel = this->makeLabel(page, "", "chatFont.fnt", .4f, ccp(130.f, 92.f), C_DIM);
        fitLabel(m_pathLabel, phrasesPath().string(), 212.f, .4f);
        this->addButton(menu, this->makeButtonSpr("Recargar frases", 120, 24.f, .38f), ccp(190.f, 72.f),
                        [this]() {
                            this->reloadPhrases();
                            this->setStatus(fmt::format("{} frases cargadas", m_phrases.size()));
                        });

        m_status = this->makeLabel(page, "", "chatFont.fnt", .5f, ccp(260.f, 72.f), C_SKY);
    }

    void setStatus(std::string const& s) {
        if (m_status) fitLabel(m_status, s, 90.f, .5f);
    }

    void toggleOrigin() {
        auto cur = Mod::get()->getSettingValue<std::string>("mode");
        std::string next = (cur == "local") ? "server" : "local";
        Mod::get()->setSettingValue<std::string>("mode", next);
        m_originSpr->setString(next == "local" ? "Local" : "Servidor");
        this->setStatus(next == "local" ? "Origen: local" : "Origen: servidor");
        this->refreshUsage();
        this->refreshStats();
    }

    void saveUrl() {
        std::string v = trim(std::string(m_urlInput->getString().c_str()));
        if (v.empty()) {
            this->setStatus("URL vacia");
            return;
        }
        Mod::get()->setSettingValue<std::string>("server-url", v);
        this->setStatus("URL guardada");
    }

    void changeSections(int delta) {
        int64_t v = Mod::get()->getSettingValue<int64_t>("sections") + delta;
        v = std::max<int64_t>(1, std::min<int64_t>(40, v));
        Mod::get()->setSettingValue<int64_t>("sections", v);
        this->updateSecLabel();
    }

    void updateSecLabel() {
        int64_t v = Mod::get()->getSettingValue<int64_t>("sections");
        m_secLabel->setString(fmt::format("{}", v).c_str());
    }

    // ------------------------------------------------------------------
    // Panel derecho: uso, chat / comando, historial
    // ------------------------------------------------------------------
    void buildRightPanel() {
        // Uso de la IA
        this->makeLabel(m_mainLayer, "USO DE LA IA", "bigFont.fnt", .35f, ccp(366.f, 244.f), C_SKY);
        this->fillRect(366.f, 226.f, BAR_W, 7.f, rgba(C_CARD, 255), 6);
        m_barFill = this->fillRect(366.f, 226.f, 0.01f, 7.f, rgba(C_CYAN, 255), 7);
        m_usageLabel = this->makeLabel(m_mainLayer, "", "chatFont.fnt", .5f, ccp(366.f, 213.f), C_ICE);
        m_modeLabel = this->makeLabel(m_mainLayer, "", "chatFont.fnt", .5f, ccp(366.f, 200.f), C_SKY);

        // Chat / comando
        this->makeLabel(m_mainLayer, "CHAT / COMANDO", "bigFont.fnt", .35f, ccp(366.f, 178.f), C_SKY);
        m_cmdInput = TextInput::create(138.f, "Escribe tu peticion...", "chatFont.fnt");
        m_cmdInput->setCommonFilter(CommonFilter::Any);
        m_cmdInput->setMaxCharCount(140);
        m_cmdInput->setPosition({435.f, 154.f});
        m_mainLayer->addChild(m_cmdInput, 20);
        this->addButton(m_ui, this->makeButtonSpr("Enviar", 138, 24.f, .42f), ccp(435.f, 126.f),
                        [this]() { this->onSend(); });

        // Historial (con boton para abrir el historial completo)
        this->makeLabel(m_mainLayer, "HISTORIAL", "bigFont.fnt", .35f, ccp(366.f, 98.f), C_SKY);
        for (int i = 0; i < 2; ++i) {
            m_hist[i] = this->makeLabel(m_mainLayer, "", "chatFont.fnt", .5f,
                                        ccp(366.f, 82.f - static_cast<float>(i) * 14.f), C_ICE);
        }
        this->addButton(m_ui, this->makeButtonSpr("Todo", 34, 16.f, .3f), ccp(492.f, 98.f),
                        [this]() { this->openHistory(); });
    }

    void refreshUsage() {
        auto source = Mod::get()->getSettingValue<std::string>("mode");
        if (source == "local") {
            m_barFill->changeWidth(0.01f);
            fitLabel(m_usageLabel, "Origen local: sin limite", 138.f, .5f);
            return;
        }
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        this->updateUsage(used, limit);
    }

    void updateUsage(int used, int limit) {
        if (limit < 1) limit = 1;
        float frac = std::min(1.f, std::max(0.f, static_cast<float>(used) / static_cast<float>(limit)));
        m_barFill->changeWidth(std::max(0.01f, BAR_W * frac));
        fitLabel(m_usageLabel, fmt::format("Limite diario  {} / {}", used, limit), 138.f, .5f);
    }

    void refreshHistoryMini() {
        auto const& h = historyList();
        for (int i = 0; i < 2; ++i) {
            int k = static_cast<int>(h.size()) - 1 - i;
            std::string s;
            if (k >= 0) {
                int mi = modeIndexById(h[k].modeId);
                s = (mi >= 0 ? std::string(MODES[mi].name) : h[k].modeId) + ": " + h[k].prompt;
            } else if (i == 0) {
                s = "(vacio)";
            }
            fitLabel(m_hist[i], s, 138.f, .5f);
        }
    }

    void pushHistory(std::string const& prompt) {
        historyList().push_back({MODES[m_mode].id, oneLine(prompt)});
        saveHistory();
        this->refreshHistoryMini();
    }

    void openHistory() {
        auto popup = HistoryPopup::create(
            [this](int mi, std::string const& prompt) {
                if (mi >= 0) this->selectMode(mi);
                m_cmdInput->setString(prompt);
            },
            [this]() {
                historyList().clear();
                saveHistory();
                this->refreshHistoryMini();
            }
        );
        if (popup) popup->show();
    }

    // ------------------------------------------------------------------
    // Barra inferior oculta + flechita
    // ------------------------------------------------------------------
    void buildBottomBar() {
        m_bar = CCNode::create();
        m_bar->setContentSize({342.f, 40.f});
        m_bar->setPosition({8.f, 4.f});
        m_bar->setScaleY(.01f);
        m_bar->setVisible(false);
        m_mainLayer->addChild(m_bar, 30);

        auto border = CCLayerColor::create(rgba(C_BLUE, 255), 342.f, 40.f);
        m_bar->addChild(border);
        auto bg = CCLayerColor::create(rgba(C_PANEL, 250), 339.f, 37.f);
        bg->setPosition({1.5f, 1.5f});
        m_bar->addChild(bg);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_bar->addChild(menu, 5);

        char const* names[5] = {"Favoritos", "Tutoriales", "Mis niveles", "Guardar", "Reiniciar"};
        for (int i = 0; i < 5; ++i) {
            auto spr = this->makeButtonSpr(names[i], 62, 28.f, .3f);
            float x = 36.f + static_cast<float>(i) * 67.5f;
            this->addButton(menu, spr, ccp(x, 20.f), [this, i]() { this->onBarButton(i); });
        }

        // Flechita ubicada arriba de la barra (la despliega / la oculta).
        m_handle = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        m_handle->setScale(.45f);
        m_handle->setRotation(90.f); // apunta hacia arriba = "desplegar"
        this->addButton(m_ui, m_handle, ccp(179.f, 48.f), [this]() { this->toggleBar(); });
    }

    void toggleBar() {
        m_barOpen = !m_barOpen;
        m_bar->stopAllActions();
        if (m_barOpen) {
            m_bar->runAction(CCSequence::create(
                CCShow::create(), CCScaleTo::create(.18f, 1.f, 1.f), nullptr
            ));
            m_handle->setRotation(-90.f);
        } else {
            m_bar->runAction(CCSequence::create(
                CCScaleTo::create(.15f, 1.f, .01f), CCHide::create(), nullptr
            ));
            m_handle->setRotation(90.f);
        }
    }

    void soon(std::string const& text) {
        FLAlertLayer::create("DecoMind AI", text, "OK")->show();
    }

    void onBarButton(int i) {
        switch (i) {
            case 0: this->soon("Pronto: guarda tus peticiones favoritas para repetirlas."); break;
            case 1: this->soon("Pronto: tutoriales dentro del editor."); break;
            case 2: this->soon("Pronto: elige sobre que nivel trabaja la IA."); break;
            case 3: this->soon("Pronto: copia de seguridad del nivel antes de aplicar cambios."); break;
            default:
                m_chat.clear();
                m_pending = -1;
                m_cmdInput->setString("");
                this->refreshChat();
                this->showPage(2);
                break;
        }
    }

    // ------------------------------------------------------------------
    // Letrero RGB decorativo
    // ------------------------------------------------------------------
    void buildSign() {
        m_signBorder = this->fillRect(358.f, 4.f, 154.f, 40.f, rgba(C_CYAN, 255), 30);
        this->fillRect(360.f, 6.f, 150.f, 36.f, rgba(C_NAVY, 255), 31);
        if (auto icon = CCSprite::create("decomind-icon.png"_spr)) {
            icon->setScale(28.f / icon->getContentWidth());
            icon->setPosition({377.f, 24.f});
            m_mainLayer->addChild(icon, 32);
        }
        m_signText = SimpleTextArea::create(" ", "chatFont.fnt", .42f, 106.f);
        m_signText->setWrappingMode(WORD_WRAP);
        m_signText->setMaxLines(3);
        m_signText->setAnchorPoint({0.f, .5f});
        m_signText->setPosition({396.f, 24.f});
        m_mainLayer->addChild(m_signText, 32);

        this->reloadPhrases();
    }

    void reloadPhrases() {
        m_phrases = loadPhrases();
        m_phraseIdx = m_phrases.empty() ? 0 : static_cast<size_t>(std::rand()) % m_phrases.size();
        m_phraseTimer = 0.f;
        this->showPhrase();
    }

    void showPhrase() {
        if (m_phrases.empty()) return;
        m_signText->setText(m_phrases[m_phraseIdx % m_phrases.size()]);
    }

    void tick(float dt) {
        // Colores tipo RGB
        m_hue += dt * .25f;
        if (m_hue > 1.f) m_hue -= 1.f;
        auto c = hsvToRgb(m_hue, .85f, 1.f);
        m_signBorder->setColor(c);

        m_colorTimer += dt;
        if (m_colorTimer > .08f) {
            m_colorTimer = 0.f;
            m_signText->setColor(ccc4(c.r, c.g, c.b, 255));
        }

        // Cambia de frase cada cierto tiempo
        m_phraseTimer += dt;
        if (m_phraseTimer > 7.f) {
            m_phraseTimer = 0.f;
            m_phraseIdx = (m_phraseIdx + 1) % std::max<size_t>(1, m_phrases.size());
            this->showPhrase();
        }
    }

    // ------------------------------------------------------------------
    // Enviar peticion
    // ------------------------------------------------------------------
    static int countObjects(std::string const& objs) {
        return static_cast<int>(std::count(objs.begin(), objs.end(), ';'));
    }

    void onSend() {
        if (m_busy) {
            this->showPage(2);
            return;
        }

        std::string prompt = trim(std::string(m_cmdInput->getString().c_str()));
        if (prompt.empty()) {
            this->showPage(2);
            this->addChat(false, "Escribe una peticion en el cuadro CHAT / COMANDO.");
            return;
        }

        this->showPage(2);
        this->addChat(true, prompt);
        this->pushHistory(prompt);
        m_cmdInput->setString("");
        m_sessionReq += 1;

        auto source = Mod::get()->getSettingValue<std::string>("mode");
        if (source == "local") {
            int sections = static_cast<int>(Mod::get()->getSettingValue<int64_t>("sections"));
            std::string objs = makeLocalLevel(sections);
            m_lel->createObjectsFromString(objs, true, true);
            m_sessionObjs += countObjects(objs);
            this->addChat(false, fmt::format(
                "Origen local (prueba): coloque {} secciones de layout. El modo elegido ({}) aun no cambia nada.",
                sections, MODES[m_mode].name
            ));
            this->refreshStats();
            return;
        }

        auto url = Mod::get()->getSettingValue<std::string>("server-url");
        auto req = web::WebRequest();
        req.bodyString(
            std::string("MODE:") + MODES[m_mode].id + "\n" +
            "CONTEXT:" + oneLine(MODES[m_mode].ctx) + "\n" +
            "PROMPT:" + oneLine(prompt) + "\n"
        );
        req.timeout(std::chrono::seconds(60));

        m_busy = true;
        this->addChat(false, "Pensando...");
        m_pending = static_cast<int>(m_chat.size()) - 1;

        m_task.spawn(
            "decomind-request",
            req.post(url),
            [this](web::WebResponse res) {
                m_busy = false;
                if (!res.ok()) {
                    this->setReply(fmt::format(
                        "Error del servidor: {}. Revisa que el servidor este corriendo y la URL en Ajustes.",
                        res.code()
                    ));
                    return;
                }
                this->handleReply(res.string().unwrapOr(""));
            }
        );
    }

    void setReply(std::string const& text) {
        if (m_pending >= 0 && m_pending < static_cast<int>(m_chat.size())) {
            m_chat[m_pending].text = text;
            m_pending = -1;
            this->refreshChat();
        } else {
            this->addChat(false, text);
        }
    }

    void handleReply(std::string const& body) {
        std::string msg = getField(body, "MSG");
        std::string objs = getField(body, "OBJ");
        std::string usedS = getField(body, "USED");
        std::string limitS = getField(body, "LIMIT");

        int total = Mod::get()->getSavedValue<int>("total", 0) + 1;
        Mod::get()->setSavedValue<int>("total", total);

        if (!usedS.empty() && !limitS.empty()) {
            int used = std::atoi(usedS.c_str());
            int limit = std::atoi(limitS.c_str());
            Mod::get()->setSavedValue<int>("used", used);
            Mod::get()->setSavedValue<int>("limit", limit);
            this->updateUsage(used, limit);
        }

        std::string out = msg.empty() ? std::string("(el servidor no envio mensaje)") : msg;
        if (!objs.empty()) {
            m_lel->createObjectsFromString(objs, true, true);
            int n = countObjects(objs);
            m_sessionObjs += n;
            out += fmt::format("\n\nObjetos colocados en el editor: {}.", n);
        }
        this->setReply(out);
        this->refreshStats();
    }

public:
    static DecoMindPopup* create(LevelEditorLayer* lel) {
        auto ret = new DecoMindPopup();
        if (ret->init(lel)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Boton "IA" en el editor
// ---------------------------------------------------------------------------
class $modify(DecoMindEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto spr = ButtonSprite::create("IA", "bigFont.fnt", "GJ_button_02.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(DecoMindEditorUI::onOpenDecoMind)
        );
        auto menu = CCMenu::create();
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width * 0.34f, win.height - 28.f});
        menu->setID("decomind-menu"_spr);
        this->addChild(menu, 100);
        return true;
    }

    void onOpenDecoMind(CCObject*) {
        if (auto popup = DecoMindPopup::create(m_editorLayer)) {
            popup->show();
        }
    }
};
