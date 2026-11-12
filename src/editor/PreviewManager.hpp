#pragma once
#include "EditorPerception.hpp"
#include "NativePlacementEngine.hpp"
#include "../validation/OperationValidator.hpp"
namespace decomind {
// Owned by the existing popup; tick() is called on the Cocos main thread.
// Original objects and color-channel state are never modified by this create-only MVP.
class PreviewManager {
    geode::Ref<LevelEditorLayer> m_editor;
    geode::Ref<GJGameLevel> m_level;
    geode::Ref<cocos2d::CCArray> m_added;
    operations::Plan m_plan;
    size_t m_cursor = 0;
    bool m_active = false;
    bool sameEditor() const {
        return m_editor && LevelEditorLayer::get() == m_editor.data() && m_editor->m_level == m_level.data();
    }
public:
    ~PreviewManager() { reject(); }
    bool active() const { return m_active; }
    bool ready() const { return m_active && m_cursor == m_plan.objects.size(); }
    size_t count() const { return m_added ? m_added->count() : 0; }
    bool begin(LevelEditorLayer* editor, operations::Plan plan, operations::Context const& context, std::string& message) {
        if (active() || !editor || LevelEditorLayer::get() != editor) { message = "Ya existe una propuesta o cambio el editor."; return false; }
        auto report = OperationValidator::validate(plan, context);
        if (!report.ok()) { message = report.errors.front(); return false; }
        if (!LevelExecutor::saveLevelSnapshot(editor, message)) return false;
        m_editor = editor; m_level = editor->m_level; m_added = cocos2d::CCArray::create();
        m_plan = std::move(plan); m_cursor = 0; m_active = true;
        message = "Preview en curso. /aceptar, /rechazar o /editar nueva instruccion.";
        return true;
    }
    bool tick() {
        if (!m_active) return true;
        if (!sameEditor()) { reject(); return false; }
        size_t end = std::min(m_cursor + 25, m_plan.objects.size());
        while (m_cursor < end) {
            // Native parser receives only internally serialized, validated properties.
            auto created = m_editor->createObjectsFromString(NativePlacementEngine::serialize(m_plan.objects[m_cursor]), true, false);
            if (created) m_added->addObjectsFromArray(created);
            if (!created || created->count() != 1) { reject(); return false; }
            ++m_cursor;
        }
        return true;
    }
    bool accept() {
        if (!ready() || !sameEditor()) return false;
        m_editor->addToUndoList(UndoObject::createWithArray(m_added, UndoCommand::Paste), false);
        if (m_editor->m_editorUI) m_editor->m_editorUI->updateButtons();
        clear(); return true;
    }
    void reject() {
        if (!m_active) return;
        // Ref retains the owning editor through teardown; never use the new current editor.
        if (m_editor && m_added && m_editor->m_objects) {
            for (int i = static_cast<int>(m_added->count()) - 1; i >= 0; --i) {
                auto obj = static_cast<GameObject*>(m_added->objectAtIndex(i));
                if (m_editor->m_objects->containsObject(obj)) m_editor->removeObject(obj, true);
            }
        }
        if (sameEditor() && m_editor->m_editorUI) m_editor->m_editorUI->updateButtons();
        clear();
    }
private:
    void clear() { m_active = false; m_cursor = 0; m_added = nullptr; m_level = nullptr; m_editor = nullptr; m_plan = {}; }
};
}
