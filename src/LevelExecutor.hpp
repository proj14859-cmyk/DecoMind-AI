#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Percepcion del Nivel, Seleccion, Grupos y Colores
// + Ejecucion de Acciones y Generador Contextual Local
//
// Capacidades v0.3.4-alpha:
//   1. Escaneo real de objetos seleccionados (EditorUI::get()->getSelectedObjects())
//      con caja delimitadora (minX, maxX, minY, maxY), deteccion de bordes/esquinas/
//      relleno e inspeccion individual de propiedades (ID, X, Y, rot, scale,
//      Z-Layer, Z-Order, Color 1/2 y Group IDs).
//   2. Escaneo de la camara/vista actual (m_objectLayer) cuando no hay seleccion.
//   3. Deteccion completa de Group IDs ocupados y libres (1..9999), incluyendo
//      m_groups en GameObject y m_targetGroupID / m_centerGroupID en triggers.
//   4. Deteccion completa de Color IDs ocupados y libres (1..999), incluyendo
//      colores asignados a objetos y canales modificados en GJEffectManager
//      (RGB, blending, opacidad).
//   5. Generador contextual local capaz de decorar estructuras seleccionadas,
//      usar los rangos de Group ID y Color ID pedidos por el usuario (o asignar
//      libres automaticamente) y anadir animaciones de entrada y salida.
// ===========================================================================
#include "LevelValidator.hpp"
#include "Modes.hpp"
#include <Geode/utils/file.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace decomind {

struct BackupInfo {
    std::string filename;
    std::string levelName;
    int objectCount = 0;
    std::string dateStr;
};

struct ObjectSnapshot {
    int id = 0;
    int x = 0;
    int y = 0;
    int rot = 0;
    float scale = 1.f;
    int zLayer = 0;
    int zOrder = 0;
    int color1 = 0;
    int color2 = 0;
    std::vector<int> groups;
};

struct EditorContextSnapshot {
    std::string levelTitle = "Sin titulo";
    int totalObjects = 0;
    int cameraX = 300;
    int cameraY = 150;
    float cameraZoom = 1.f;

    // Seleccion activa en el editor
    int selectedCount = 0;
    int selMinX = 0;
    int selMaxX = 0;
    int selMinY = 0;
    int selMaxY = 0;
    int selWidthBlocks = 0;
    int selHeightBlocks = 0;
    std::vector<ObjectSnapshot> selectedObjects;

    // Objetos cercanos a la camara cuando no hay seleccion
    int viewportCount = 0;
    int viewMinX = 0;
    int viewMaxX = 0;
    int viewMinY = 0;
    int viewMaxY = 0;
    std::vector<ObjectSnapshot> viewportObjects;

    // Group IDs (1..9999) y Color IDs (1..999)
    std::set<int> usedGroups;
    std::vector<int> freeGroups;
    int nextFreeGroup = 1;

    std::set<int> usedColors;
    std::vector<int> freeColors;
    int nextFreeColor = 1;

    std::string usedGroupsCompact;
    std::string freeGroupsCompact;
    std::string usedColorsCompact;
    std::string freeColorsCompact;
    std::string colorPaletteSummary;
};

struct PromptDirectives {
    int groupStart = 0;
    int groupEnd = 0;
    int colorStart = 0;
    int colorEnd = 0;
    bool hasEntryAnim = false;
    bool hasExitAnim = false;
    bool hasPulseAnim = false;
    bool hasRotateAnim = false;
    std::string styleTag = "neon";
};

class LevelExecutor {
public:
    static std::string getLevelTitle(LevelEditorLayer* lel) {
        if (!lel || !lel->m_level) return "Sin titulo";
        std::string name = sanitizeForGD(std::string(lel->m_level->m_levelName));
        name = trimStr(name);
        return name.empty() ? "Sin titulo" : name;
    }

    static int getCurrentObjectCount(LevelEditorLayer* lel) {
        if (!lel || !lel->m_objects) return 0;
        return static_cast<int>(lel->m_objects->count());
    }

    // Compacta un conjunto de enteros ordenados en rangos legibles: "1-5,8,10-14"
    static std::string formatCompactRanges(std::set<int> const& nums, size_t maxChars = 360) {
        if (nums.empty()) return "ninguno";
        std::string out;
        int start = -1, prev = -1;
        auto flushRange = [&](int a, int b) {
            std::string piece = (a == b) ? fmt::format("{}", a) : fmt::format("{}-{}", a, b);
            if (!out.empty()) out += ",";
            out += piece;
        };
        for (int v : nums) {
            if (start == -1) {
                start = prev = v;
            } else if (v == prev + 1) {
                prev = v;
            } else {
                flushRange(start, prev);
                if (out.size() >= maxChars) {
                    out += ",...";
                    return out;
                }
                start = prev = v;
            }
        }
        if (start != -1) flushRange(start, prev);
        return out;
    }

    static std::string formatIntVector(std::vector<int> const& vec, size_t maxItems = 25) {
        if (vec.empty()) return "ninguno";
        std::string out;
        size_t n = std::min(vec.size(), maxItems);
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) out += ",";
            out += fmt::format("{}", vec[i]);
        }
        return out;
    }

    static ObjectSnapshot inspectSingleObject(GameObject* obj, std::set<int>& outGroups, std::set<int>& outColors) {
        ObjectSnapshot snap;
        if (!obj) return snap;

        snap.id = obj->m_objectID;
        snap.x = static_cast<int>(std::round(obj->getPositionX()));
        snap.y = static_cast<int>(std::round(obj->getPositionY()));
        snap.rot = static_cast<int>(std::round(obj->getRotation()));
        snap.scale = obj->getScaleX();
        snap.zLayer = static_cast<int>(obj->m_zLayer);
        snap.zOrder = obj->m_zOrder;

        if (obj->m_baseColor) {
            int c1 = obj->m_baseColor->m_colorID;
            if (c1 > 0 && c1 < 1000) {
                snap.color1 = c1;
                outColors.insert(c1);
            }
        }
        if (obj->m_detailColor) {
            int c2 = obj->m_detailColor->m_colorID;
            if (c2 > 0 && c2 < 1000) {
                snap.color2 = c2;
                outColors.insert(c2);
            }
        }

        if (obj->m_groups) {
            for (short g : *obj->m_groups) {
                if (g > 0 && g <= 9999) {
                    snap.groups.push_back(static_cast<int>(g));
                    outGroups.insert(static_cast<int>(g));
                }
            }
        }

        // Si es un trigger o EffectGameObject, registrar tambien sus grupos/colores objetivo
        if (auto eff = typeinfo_cast<EffectGameObject*>(obj)) {
            if (eff->m_targetGroupID > 0 && eff->m_targetGroupID <= 9999) {
                outGroups.insert(eff->m_targetGroupID);
            }
            if (eff->m_centerGroupID > 0 && eff->m_centerGroupID <= 9999) {
                outGroups.insert(eff->m_centerGroupID);
            }
            if (eff->m_targetColor > 0 && eff->m_targetColor < 1000) {
                outColors.insert(eff->m_targetColor);
            }
        }

        return snap;
    }

    // Escanea el estado completo del nivel, la seleccion activa, la camara, los grupos y los colores
    static EditorContextSnapshot scanEditorContext(LevelEditorLayer* lel) {
        EditorContextSnapshot ctx;
        if (!lel) {
            for (int g = 1; g <= 30; ++g) ctx.freeGroups.push_back(g);
            for (int c = 1; c <= 20; ++c) ctx.freeColors.push_back(c);
            ctx.usedGroupsCompact = "ninguno";
            ctx.freeGroupsCompact = formatIntVector(ctx.freeGroups, 20);
            ctx.usedColorsCompact = "ninguno";
            ctx.freeColorsCompact = formatIntVector(ctx.freeColors, 20);
            return ctx;
        }

        ctx.levelTitle = getLevelTitle(lel);
        ctx.totalObjects = getCurrentObjectCount(lel);

        // 1. Calcular centro aproximado de la camara en el editor
        if (lel->m_objectLayer) {
            auto win = CCDirector::get()->getWinSize();
            float scale = lel->m_objectLayer->getScale();
            if (scale <= 0.001f) scale = 1.f;
            ctx.cameraZoom = scale;
            CCPoint layerPos = lel->m_objectLayer->getPosition();
            ctx.cameraX = std::max(0, static_cast<int>(std::round((win.width * 0.5f - layerPos.x) / scale)));
            ctx.cameraY = std::max(0, static_cast<int>(std::round((win.height * 0.5f - layerPos.y) / scale)));
        }

        // 2. Escanear todos los objetos del nivel para detectar Group IDs y Color IDs ocupados
        //    y recopilar objetos dentro de la vista de la camara
        bool firstView = true;
        if (lel->m_objects) {
            int count = static_cast<int>(lel->m_objects->count());
            for (int i = 0; i < count; ++i) {
                auto obj = static_cast<GameObject*>(lel->m_objects->objectAtIndex(i));
                if (!obj) continue;
                ObjectSnapshot snap = inspectSingleObject(obj, ctx.usedGroups, ctx.usedColors);

                // Si esta dentro de la ventana de la camara (+-600 en X, +-420 en Y)
                if (std::abs(snap.x - ctx.cameraX) <= 600 && std::abs(snap.y - ctx.cameraY) <= 420) {
                    ctx.viewportCount++;
                    if (firstView) {
                        ctx.viewMinX = ctx.viewMaxX = snap.x;
                        ctx.viewMinY = ctx.viewMaxY = snap.y;
                        firstView = false;
                    } else {
                        ctx.viewMinX = std::min(ctx.viewMinX, snap.x);
                        ctx.viewMaxX = std::max(ctx.viewMaxX, snap.x);
                        ctx.viewMinY = std::min(ctx.viewMinY, snap.y);
                        ctx.viewMaxY = std::max(ctx.viewMaxY, snap.y);
                    }
                    if (ctx.viewportObjects.size() < 80) {
                        ctx.viewportObjects.push_back(snap);
                    }
                }
            }
        }

        // 3. Escanear canales de color configurados en LevelSettings / GJEffectManager
        if (lel->m_levelSettings && lel->m_levelSettings->m_effectManager) {
            std::vector<std::string> activePalettes;
            for (int colId = 1; colId <= 999; ++colId) {
                auto ca = lel->m_levelSettings->m_effectManager->getColorAction(colId);
                if (!ca) continue;
                bool isNonDefault = (ca->m_fromColor.r != 255 || ca->m_fromColor.g != 255 || ca->m_fromColor.b != 255) ||
                                    ca->m_blending || (ca->m_fromOpacity < 0.99f) || (ca->m_copyID > 0);
                if (isNonDefault) {
                    ctx.usedColors.insert(colId);
                    if (activePalettes.size() < 16) {
                        activePalettes.push_back(fmt::format(
                            "C{}(#{:02X}{:02X}{:02X}{}{})",
                            colId,
                            static_cast<int>(ca->m_fromColor.r),
                            static_cast<int>(ca->m_fromColor.g),
                            static_cast<int>(ca->m_fromColor.b),
                            ca->m_blending ? ",B" : "",
                            ca->m_fromOpacity < 0.99f ? fmt::format(",a{:.1f}", ca->m_fromOpacity) : ""
                        ));
                    }
                }
            }
            for (size_t i = 0; i < activePalettes.size(); ++i) {
                if (i > 0) ctx.colorPaletteSummary += " ";
                ctx.colorPaletteSummary += activePalettes[i];
            }
        }

        // 4. Escanear objetos seleccionados actualmente por el usuario en EditorUI
        if (auto ui = EditorUI::get()) {
            if (auto selArr = ui->getSelectedObjects()) {
                int selN = static_cast<int>(selArr->count());
                ctx.selectedCount = selN;
                bool firstSel = true;
                for (int i = 0; i < selN; ++i) {
                    auto obj = static_cast<GameObject*>(selArr->objectAtIndex(i));
                    if (!obj) continue;
                    ObjectSnapshot snap = inspectSingleObject(obj, ctx.usedGroups, ctx.usedColors);
                    if (firstSel) {
                        ctx.selMinX = ctx.selMaxX = snap.x;
                        ctx.selMinY = ctx.selMaxY = snap.y;
                        firstSel = false;
                    } else {
                        ctx.selMinX = std::min(ctx.selMinX, snap.x);
                        ctx.selMaxX = std::max(ctx.selMaxX, snap.x);
                        ctx.selMinY = std::min(ctx.selMinY, snap.y);
                        ctx.selMaxY = std::max(ctx.selMaxY, snap.y);
                    }
                    if (ctx.selectedObjects.size() < 120) {
                        ctx.selectedObjects.push_back(snap);
                    }
                }
                if (!firstSel) {
                    ctx.selWidthBlocks = std::max(1, (ctx.selMaxX - ctx.selMinX) / 30 + 1);
                    ctx.selHeightBlocks = std::max(1, (ctx.selMaxY - ctx.selMinY) / 30 + 1);
                }
            }
        }

        // 5. Calcular Group IDs libres (1..9999) y Color IDs libres (1..999)
        for (int g = 1; g <= 9999 && ctx.freeGroups.size() < 40; ++g) {
            if (ctx.usedGroups.find(g) == ctx.usedGroups.end()) {
                ctx.freeGroups.push_back(g);
            }
        }
        ctx.nextFreeGroup = ctx.freeGroups.empty() ? 9000 : ctx.freeGroups.front();

        for (int c = 1; c <= 999 && ctx.freeColors.size() < 30; ++c) {
            if (ctx.usedColors.find(c) == ctx.usedColors.end()) {
                ctx.freeColors.push_back(c);
            }
        }
        ctx.nextFreeColor = ctx.freeColors.empty() ? 900 : ctx.freeColors.front();

        ctx.usedGroupsCompact = formatCompactRanges(ctx.usedGroups);
        ctx.freeGroupsCompact = formatIntVector(ctx.freeGroups, 25);
        ctx.usedColorsCompact = formatCompactRanges(ctx.usedColors);
        ctx.freeColorsCompact = formatIntVector(ctx.freeColors, 20);
        return ctx;
    }

    static std::string serializeObjectsBrief(std::vector<ObjectSnapshot> const& objs, size_t limit = 70) {
        std::string out;
        size_t n = std::min(objs.size(), limit);
        for (size_t i = 0; i < n; ++i) {
            auto const& o = objs[i];
            if (i > 0) out += ";";
            out += fmt::format("{}@{},{}", o.id, o.x, o.y);
            if (o.rot != 0) out += fmt::format("r{}", o.rot);
            if (std::abs(o.scale - 1.f) > 0.04f) out += fmt::format("s{:.2f}", o.scale);
            if (o.color1 > 0) out += fmt::format("c{}", o.color1);
            if (!o.groups.empty()) {
                out += "g";
                for (size_t k = 0; k < o.groups.size() && k < 4; ++k) {
                    if (k > 0) out += ".";
                    out += fmt::format("{}", o.groups[k]);
                }
            }
        }
        return out;
    }

    // Construye las lineas de contexto que AIClient envia al servidor Python
    static std::string buildProtocolContextLines(LevelEditorLayer* lel) {
        EditorContextSnapshot ctx = scanEditorContext(lel);
        std::string lines;

        lines += fmt::format(
            "LEVEL_STATS:total_objects={}|camera={},{}|zoom={:.2f}\n",
            ctx.totalObjects, ctx.cameraX, ctx.cameraY, ctx.cameraZoom
        );

        if (ctx.selectedCount > 0) {
            lines += fmt::format(
                "SELECTION:count={}|bbox={},{},{},{}|grid={}x{}|objs={}\n",
                ctx.selectedCount,
                ctx.selMinX, ctx.selMinY, ctx.selMaxX, ctx.selMaxY,
                ctx.selWidthBlocks, ctx.selHeightBlocks,
                serializeObjectsBrief(ctx.selectedObjects, 80)
            );
        } else {
            lines += "SELECTION:none\n";
        }

        if (ctx.viewportCount > 0) {
            lines += fmt::format(
                "VIEWPORT:count={}|bbox={},{},{},{}|objs={}\n",
                ctx.viewportCount,
                ctx.viewMinX, ctx.viewMinY, ctx.viewMaxX, ctx.viewMaxY,
                serializeObjectsBrief(ctx.viewportObjects, 60)
            );
        } else {
            lines += fmt::format("VIEWPORT:empty|camera={},{}\n", ctx.cameraX, ctx.cameraY);
        }

        lines += fmt::format("USED_GROUPS:{}\n", ctx.usedGroupsCompact);
        lines += fmt::format("FREE_GROUPS:next={}|pool={}\n", ctx.nextFreeGroup, ctx.freeGroupsCompact);
        lines += fmt::format(
            "USED_COLORS:{}{}\n",
            ctx.usedColorsCompact,
            ctx.colorPaletteSummary.empty() ? "" : ("|palette=" + ctx.colorPaletteSummary)
        );
        lines += fmt::format("FREE_COLORS:next={}|pool={}\n", ctx.nextFreeColor, ctx.freeColorsCompact);
        return lines;
    }

    // Extrae del texto del usuario si pidio rangos de grupos, rangos de colores o animaciones de entrada/salida
    static PromptDirectives parseUserDirectives(std::string const& prompt, EditorContextSnapshot const& ctx) {
        PromptDirectives d;
        std::string low = sanitizeForGD(prompt);
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        d.hasEntryAnim = (low.find("entrada") != std::string::npos || low.find("entry") != std::string::npos ||
                          low.find("aparec") != std::string::npos || low.find("intro") != std::string::npos);
        d.hasExitAnim  = (low.find("salida") != std::string::npos || low.find("exit") != std::string::npos ||
                          low.find("desaparec") != std::string::npos || low.find("outro") != std::string::npos);
        d.hasPulseAnim = (low.find("pulso") != std::string::npos || low.find("pulse") != std::string::npos ||
                          low.find("brill") != std::string::npos || low.find("ritmo") != std::string::npos);
        d.hasRotateAnim = (low.find("rotac") != std::string::npos || low.find("giro") != std::string::npos ||
                           low.find("gira") != std::string::npos || low.find("sierra") != std::string::npos);

        if (low.find("tech") != std::string::npos || low.find("tecnolog") != std::string::npos ||
            low.find("cyber") != std::string::npos || low.find("futurist") != std::string::npos) {
            d.styleTag = "tech";
        } else if (low.find("glow") != std::string::npos || low.find("atmosfer") != std::string::npos ||
                   low.find("espacial") != std::string::npos) {
            d.styleTag = "glow";
        } else if (low.find("modern") != std::string::npos || low.find("minimal") != std::string::npos ||
                   low.find("limpio") != std::string::npos) {
            d.styleTag = "modern";
        }

        // Buscar numeros precedidos por "color" / "canal" o "grupo" / "group"
        auto extractRangeNearKeyword = [&](std::vector<std::string> const& keys, int& outStart, int& outEnd, int maxLimit) {
            for (auto const& kw : keys) {
                size_t pos = low.find(kw);
                if (pos == std::string::npos) continue;
                std::string sub = low.substr(pos, std::min<size_t>(65, low.size() - pos));
                std::vector<int> nums;
                size_t i = 0;
                while (i < sub.size() && nums.size() < 2) {
                    if (std::isdigit(static_cast<unsigned char>(sub[i]))) {
                        int val = 0;
                        while (i < sub.size() && std::isdigit(static_cast<unsigned char>(sub[i]))) {
                            val = val * 10 + (sub[i] - '0');
                            ++i;
                        }
                        if (val >= 1 && val <= maxLimit) nums.push_back(val);
                    } else {
                        ++i;
                    }
                }
                if (!nums.empty()) {
                    outStart = nums[0];
                    outEnd = (nums.size() >= 2 && nums[1] >= nums[0]) ? nums[1] : nums[0];
                    return;
                }
            }
        };

        extractRangeNearKeyword({"colores", "color id", "color", "canal"}, d.colorStart, d.colorEnd, 999);
        extractRangeNearKeyword({"grupo de objetos", "group id de objetos", "grupos", "group id", "grupo"}, d.groupStart, d.groupEnd, 9999);

        // Si no especifico grupos, tomar automaticamente de los grupos libres del nivel
        if (d.groupStart <= 0) {
            d.groupStart = ctx.nextFreeGroup;
            d.groupEnd = (ctx.freeGroups.size() >= 4) ? ctx.freeGroups[3] : (d.groupStart + 3);
        }
        // Si no especifico colores, tomar automaticamente de los canales libres del nivel
        if (d.colorStart <= 0) {
            d.colorStart = ctx.nextFreeColor;
            d.colorEnd = (ctx.freeColors.size() >= 3) ? ctx.freeColors[2] : (d.colorStart + 2);
        }

        return d;
    }

    static ValidationResult applyValidatedObjects(
        LevelEditorLayer* lel,
        std::string const& rawObjs,
        std::string const& modeId
    ) {
        ValidationResult vr = LevelValidator::validateObjects(rawObjs, modeId);
        if (!vr.allowed || vr.sanitizedObjects.empty() || !lel) {
            return vr;
        }
        lel->createObjectsFromString(vr.sanitizedObjects, true, true);
        return vr;
    }

    // Generador local inteligente y sensible al contexto (seleccion, grupos/colores libres y animaciones)
    static std::string generateContextualLocalObjects(
        LevelEditorLayer* lel,
        std::string const& modeId,
        std::string const& userPrompt,
        int sections,
        std::string& outSummary
    ) {
        EditorContextSnapshot ctx = scanEditorContext(lel);
        PromptDirectives dir = parseUserDirectives(userPrompt, ctx);

        if (modeId == "ideas") {
            outSummary = fmt::format(
                "[Modo Local Contextual] Analisis de '{}' ({} objetos, {} seleccionados). "
                "Grupos ocupados: [{}] | Primeros grupos libres: [{}, {}]. "
                "Colores ocupados: [{}] | Primeros colores libres: [{}, {}]. "
                "Sugerencia: agrupa bordes en el grupo {} y el relleno interior en el grupo {} con pulso en el canal {}.",
                ctx.levelTitle, ctx.totalObjects, ctx.selectedCount,
                ctx.usedGroupsCompact, dir.groupStart, dir.groupEnd,
                ctx.usedColorsCompact, dir.colorStart, dir.colorEnd,
                dir.groupStart, dir.groupEnd, dir.colorStart
            );
            return "";
        }

        std::string s;
        int gMain   = dir.groupStart;
        int gDetail = std::min(9999, (dir.groupEnd > dir.groupStart) ? (dir.groupStart + 1) : (dir.groupStart + 1));
        int gGlow   = std::min(9999, dir.groupEnd);
        int cBase   = dir.colorStart;
        int cDetail = std::min(999, (dir.colorEnd > dir.colorStart) ? (dir.colorStart + 1) : (dir.colorStart + 1));
        int cGlow   = std::min(999, dir.colorEnd);

        // Si el usuario tiene una estructura seleccionada (o visible en camara) y pide Deco / Block Design / Estructurar / Triggers / Efectos
        bool hasStructureTarget = (ctx.selectedCount > 0) ||
                                  (ctx.viewportCount > 0 && (modeId == "deco" || modeId == "block_design" || modeId == "estructurar" || modeId == "efectos" || modeId == "triggers"));

        if (hasStructureTarget && modeId != "layout" && modeId != "layout_deco") {
            int minX = (ctx.selectedCount > 0) ? ctx.selMinX : ctx.viewMinX;
            int maxX = (ctx.selectedCount > 0) ? ctx.selMaxX : std::min(ctx.viewMaxX, ctx.viewMinX + 240);
            int minY = (ctx.selectedCount > 0) ? ctx.selMinY : ctx.viewMinY;
            int maxY = (ctx.selectedCount > 0) ? ctx.selMaxY : std::min(ctx.viewMaxY, ctx.viewMinY + 150);
            if (maxX < minX) std::swap(minX, maxX);
            if (maxY < minY) std::swap(minY, maxY);

            // 1. Decorar / estructurar cada celda del borde e interior de la estructura seleccionada
            if (modeId != "triggers") {
                for (int x = minX; x <= maxX; x += 30) {
                    for (int y = minY; y <= maxY; y += 30) {
                        bool isTop = (y + 30 > maxY);
                        bool isBottom = (y - 30 < minY);
                        bool isLeft = (x - 30 < minX);
                        bool isRight = (x + 30 > maxX);

                        // Relleno tecnologico / geometrico interior (Z-Order bajo)
                        int fillId = (dir.styleTag == "tech") ? 211 : 503;
                        s += fmt::format(
                            "1,{},2,{},3,{},21,{},22,{},24,-1,25,-2,57,{};",
                            fillId, x, y, cBase, cDetail, gMain
                        );

                        // Bordes y esquinas superiores/inferiores con orientacion exacta
                        if (isTop) {
                            s += fmt::format(
                                "1,40,2,{},3,{},6,0,21,{},24,1,25,3,57,{}.{};",
                                x, y, cDetail, gMain, gDetail
                            );
                        }
                        if (isBottom && !isTop) {
                            s += fmt::format(
                                "1,40,2,{},3,{},6,180,21,{},24,1,25,3,57,{}.{};",
                                x, y, cDetail, gMain, gDetail
                            );
                        }
                        if (isLeft && isTop) {
                            s += fmt::format(
                                "1,505,2,{},3,{},32,0.75,21,{},24,1,25,5,57,{}.{};",
                                x, y, cGlow, gMain, gDetail
                            );
                        } else if (isRight && isTop) {
                            s += fmt::format(
                                "1,505,2,{},3,{},6,90,32,0.75,21,{},24,1,25,5,57,{}.{};",
                                x, y, cGlow, gMain, gDetail
                            );
                        }
                    }
                }

                // Glow atmosferico y detalles superiores sin hitbox (IDs 503, 504, 1705)
                int midX = (minX + maxX) / 2;
                int midY = (minY + maxY) / 2;
                s += fmt::format(
                    "1,504,2,{},3,{},32,2.0,21,{},24,-3,25,-10,57,{}.{};",
                    midX, midY, cGlow, gMain, gGlow
                );
                if (dir.hasRotateAnim || modeId == "efectos") {
                    s += fmt::format(
                        "1,1705,2,{},3,{},32,0.85,21,{},22,{},24,-2,25,-6,57,{}.{};",
                        midX, maxY + 30, cDetail, cGlow, gMain, gDetail
                    );
                }
            }

            // 2. Configurar triggers de Color inicial, Entrada, Pulso, Rotacion y Salida
            int trigX = std::max(30, minX - 120);
            int trigY = maxY + 90;

            // Color Trigger (899) para inicializar los canales libres/pedidos con estética neón/tech
            s += fmt::format("1,899,2,{},3,{},7,24,8,200,9,255,10,0.2,23,{},35,1;", trigX, trigY, cBase);
            s += fmt::format("1,899,2,{},3,{},7,182,8,72,9,246,10,0.2,23,{},35,1;", trigX, trigY + 30, cDetail);

            if (dir.hasEntryAnim || modeId == "triggers" || modeId == "efectos" || modeId == "transiciones") {
                // Animacion de entrada: preparar abajo y transparente -> subir suavemente (EasingOut=2) y aparecer
                s += fmt::format("1,901,2,{},3,{},10,0,28,0,29,-90,30,0,51,{};", std::max(15, trigX - 60), trigY + 60, gMain);
                s += fmt::format("1,1007,2,{},3,{},10,0,35,0,51,{};", std::max(15, trigX - 60), trigY + 90, gMain);
                s += fmt::format("1,901,2,{},3,{},10,0.65,28,0,29,90,30,2,85,2,51,{};", trigX, trigY + 60, gMain);
                s += fmt::format("1,1007,2,{},3,{},10,0.55,35,1,51,{};", trigX, trigY + 90, gMain);
            }

            if (dir.hasPulseAnim || modeId == "deco" || modeId == "efectos") {
                // Pulse Trigger (1006) al pasar por la estructura
                s += fmt::format("1,1006,2,{},3,{},7,120,8,255,9,235,45,0.12,46,0.15,47,0.35,51,{},52,1;", minX, trigY, gDetail);
            }

            if (dir.hasRotateAnim || modeId == "efectos") {
                // Rotate Trigger (1346)
                s += fmt::format("1,1346,2,{},3,{},10,1.5,68,180,30,2,51,{},71,{};", minX, trigY + 30, gDetail, gMain);
            }

            if (dir.hasExitAnim || modeId == "transiciones") {
                // Animacion de salida despues de superar la estructura
                int exitX = maxX + 90;
                s += fmt::format("1,901,2,{},3,{},10,0.6,28,0,29,120,30,3,85,2,51,{};", exitX, trigY + 60, gMain);
                s += fmt::format("1,1007,2,{},3,{},10,0.5,35,0,51,{};", exitX, trigY + 90, gMain);
            }

            outSummary = fmt::format(
                "[Contextual v0.3.4-alpha] Estructura {}x{} en X=[{}..{}], Y=[{}..{}] decorada en estilo '{}'. "
                "Grupos usados: {}..{} (ocupados previos: {}). Colores usados: {}..{} (ocupados previos: {}).{}",
                (maxX - minX) / 30 + 1, (maxY - minY) / 30 + 1, minX, maxX, minY, maxY, dir.styleTag,
                gMain, gGlow, ctx.usedGroupsCompact,
                cBase, cGlow, ctx.usedColorsCompact,
                (dir.hasEntryAnim || dir.hasExitAnim) ? " Incluye triggers de animacion de entrada/salida." : ""
            );
            return s;
        }

        // Si no hay seleccion o es modo Layout / Layout+Deco: construir a partir de la camara o final del nivel
        int x = std::max(300, ctx.cameraX - 120);
        for (int i = 0; i < sections; ++i) {
            if (modeId == "deco" || modeId == "efectos" || modeId == "transiciones" || modeId == "triggers") {
                s += fmt::format("1,503,2,{},3,{},21,{},24,-1,25,-2,57,{};", x, 105 + (i % 3) * 30, cBase, gMain);
                s += fmt::format("1,505,2,{},3,{},21,{},24,1,25,3,57,{}.{};", x + 30, 135, cDetail, gMain, gDetail);
                x += 210;
            } else if (modeId == "block_design") {
                s += fmt::format("1,1,2,{},3,15,21,{},57,{};", x, cBase, gMain);
                s += fmt::format("1,1,2,{},3,15,21,{},57,{};", x + 30, cBase, gMain);
                s += fmt::format("1,1,2,{},3,45,21,{},57,{};", x, cBase, gMain);
                s += fmt::format("1,1,2,{},3,45,21,{},57,{};", x + 30, cBase, gMain);
                s += fmt::format("1,40,2,{},3,45,21,{},24,1,25,3,57,{}.{};", x, cDetail, gMain, gDetail);
                s += fmt::format("1,40,2,{},3,45,21,{},24,1,25,3,57,{}.{};", x + 30, cDetail, gMain, gDetail);
                x += 210;
            } else {
                switch (i % 4) {
                    case 0:
                        s += fmt::format("1,8,2,{},3,15;", x);
                        x += 240;
                        break;
                    case 1:
                        s += fmt::format("1,8,2,{},3,15;1,8,2,{},3,15;", x, x + 30);
                        x += 270;
                        break;
                    case 2:
                        s += fmt::format("1,1,2,{},3,15,21,{},57,{};1,8,2,{},3,15;", x, cBase, gMain, x + 90);
                        if (modeId == "layout_deco") {
                            s += fmt::format("1,40,2,{},3,15,21,{},24,1,25,2,57,{};", x, cDetail, gDetail);
                        }
                        x += 270;
                        break;
                    case 3:
                        s += fmt::format(
                            "1,1,2,{},3,15,21,{},57,{};1,1,2,{},3,15,21,{},57,{};1,1,2,{},3,45,21,{},57,{};"
                            "1,1,2,{},3,15,21,{},57,{};1,1,2,{},3,45,21,{},57,{};1,1,2,{},3,75,21,{},57,{};",
                            x, cBase, gMain, x + 30, cBase, gMain, x + 30, cBase, gMain,
                            x + 60, cBase, gMain, x + 60, cBase, gMain, x + 60, cBase, gMain
                        );
                        if (modeId == "layout_deco") {
                            s += fmt::format(
                                "1,504,2,{},3,45,32,1.5,21,{},24,-2,25,-5,57,{};",
                                x + 30, cGlow, gGlow
                            );
                        }
                        x += 330;
                        break;
                }
            }
        }

        outSummary = fmt::format(
            "[Modo Local Contextual] Generadas {} secciones en '{}' usando grupos libres {}..{} y colores {}..{}.",
            sections, MODES[std::max(0, modeIndexById(modeId))].name, gMain, gGlow, cBase, cGlow
        );
        return s;
    }

    // Compatibilidad con llamadas simples de 2 argumentos
    static std::string generateLocalDemoObjects(std::string const& modeId, int sections) {
        std::string dummy;
        return generateContextualLocalObjects(LevelEditorLayer::get(), modeId, "", sections, dummy);
    }

    // Gestion de respaldos / puntos de guardado locales
    static std::filesystem::path backupsDir() {
        auto dir = Mod::get()->getSaveDir() / "backups";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    static bool saveLevelSnapshot(LevelEditorLayer* lel, std::string& outMsg) {
        if (!lel) {
            outMsg = "No hay un nivel activo en el editor.";
            return false;
        }
        std::string title = getLevelTitle(lel);
        int objCount = getCurrentObjectCount(lel);
        std::time_t now = std::time(nullptr);
        char timeBuf[32] = {0};
        if (auto tm = std::localtime(&now)) {
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);
        } else {
            std::snprintf(timeBuf, sizeof(timeBuf), "%lld", static_cast<long long>(now));
        }

        std::string fname = fmt::format("snapshot_{}.txt", static_cast<long long>(now));
        std::string content = fmt::format(
            "LEVEL:{}\nOBJECTS:{}\nDATE:{}\n",
            title, objCount, timeBuf
        );

        auto res = utils::file::writeString(backupsDir() / fname, content);
        if (!res.isOk()) {
            outMsg = "No se pudo escribir el punto de guardado en disco.";
            return false;
        }
        outMsg = fmt::format("Punto de guardado creado para '{}' ({} objetos).", title, objCount);
        return true;
    }

    static std::vector<BackupInfo> listSnapshots() {
        std::vector<BackupInfo> list;
        std::error_code ec;
        auto dir = backupsDir();
        if (!std::filesystem::exists(dir, ec)) return list;

        for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            auto r = utils::file::readString(entry.path());
            if (!r.isOk()) continue;
            std::string body = r.unwrap();
            BackupInfo info;
            info.filename = entry.path().filename().string();
            info.levelName = extractLineValue(body, "LEVEL");
            info.objectCount = std::atoi(extractLineValue(body, "OBJECTS").c_str());
            info.dateStr = extractLineValue(body, "DATE");
            if (info.levelName.empty()) info.levelName = "Sin titulo";
            list.push_back(info);
        }
        std::sort(list.begin(), list.end(), [](BackupInfo const& a, BackupInfo const& b) {
            return a.filename > b.filename;
        });
        return list;
    }

private:
    static std::string extractLineValue(std::string const& body, std::string const& key) {
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
};

} // namespace decomind
