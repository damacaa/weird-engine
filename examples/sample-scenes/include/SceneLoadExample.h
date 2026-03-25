#pragma once

#include <weird-engine.h>
#include <random>
#include <cmath>
#include <iostream>
#include <cstdio>

#include "globals.h"

using namespace WeirdEngine;

class SceneLoadExample : public Scene
{
public:
    SceneLoadExample(const PhysicsSettings& settings)
        : Scene(settings), m_rng(12345) {}

private:
    // =====================================================================
    // Types
    // =====================================================================
    struct ShapeBtnInfo { Entity entity; uint16_t shapeType; };
    struct CombBtnInfo  { Entity toggleEntity; CombinationType combType; };
    struct ParamBtn     { Entity shapeEntity; Entity textEntity; };

    // =====================================================================
    // State
    // =====================================================================
    std::vector<ShapeBtnInfo> m_shapeButtons;
    std::vector<CombBtnInfo>  m_combButtons;
    std::array<Entity, 16>    m_materialToggles{};
    std::array<ParamBtn, 8>   m_paramBtns{};
    Entity                    m_selInfoText{};

    int              m_selectedMaterial    = 1;
    int              m_selectedCombIdx     = 0;
    CombinationType  m_selectedCombination = CombinationType::Addition;

    Entity m_selectedEntity = static_cast<Entity>(-1);
    bool   m_hasSelection   = false;

    std::mt19937 m_rng;

    // =====================================================================
    // Layout (800x800 screen)
    // =====================================================================
    static constexpr float BTN_SIZE      = 18.0f;
    static constexpr float BTN_SPACING   = 56.0f;
    static constexpr float START_X       = 40.0f;
    static constexpr float SHAPE_Y       = 770.0f;
    static constexpr float COMB_Y        = 706.0f;
    static constexpr float MAT_Y         = 50.0f;
    static constexpr float MAT_SPACING   = 47.0f;

    static constexpr float PANEL_X       = 750.0f;
    static constexpr float PANEL_TOP_Y   = 620.0f;
    static constexpr float PARAM_GAP     = 48.0f;
    static constexpr float P_BTN_W       = 24.0f;
    static constexpr float P_BTN_H       = 16.0f;

    static constexpr float HIDDEN        = -5000.0f;
    static constexpr int   COMB_GRP_BASE = 100;
    static constexpr float SEL_THRESH    = 5.0f;

    // =====================================================================
    // Lifecycle
    // =====================================================================
    void onStart() override
    {
        m_debugInput = true;
        m_debugFly   = true;
        m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

        buildShapeButtons();
        buildCombToggles();
        buildMaterialToggles();
        buildParamPanel();
    }

    void onUpdate(float delta) override
    {
        g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

        if (Input::GetKeyDown(Input::Q))
            setSceneComplete();
        if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::S))
            saveScene(ASSETS_PATH "example.weird");

        syncMaterialToggles();
        syncCombToggles();

        if (Input::GetMouseButtonDown(Input::LeftClick))
            onLeftClick();
        if (Input::GetMouseButtonDown(Input::RightClick))
            onRightClick();
        if (Input::GetKeyDown(Input::X) && m_hasSelection)
            deleteSelected();

        refreshPanel();
    }

    // =====================================================================
    // Shape buttons (top row) — each uses its own SDF as a preview
    // =====================================================================
    void buildShapeButtons()
    {
        const uint16_t types[] = {
            DefaultShapes::CIRCLE, DefaultShapes::BOX,
            DefaultShapes::LINE,   DefaultShapes::RAMP,
            DefaultShapes::STAR
        };
        const uint16_t colors[] = { 1, 5, 6, 8, 10 };

        for (int i = 0; i < 5; i++)
        {
            float cx = START_X + i * BTN_SPACING;
            float cy = SHAPE_Y;
            float p[8]{};
            previewParams(types[i], cx, cy, p);

            Entity e = addUIShape(types[i], p, colors[i]);
            auto& b  = m_ecs.addComponent<ShapeButton>(e);
            b.modifierAmount = 1.0f;
            b.clickPadding   = 8.0f;

            m_shapeButtons.push_back({ e, types[i] });
            blacklistEntity(e);
        }
    }

    void previewParams(uint16_t t, float cx, float cy, float p[8])
    {
        const float s = BTN_SIZE;
        switch (t)
        {
        case DefaultShapes::CIRCLE:
            p[0] = cx; p[1] = cy; p[2] = s;
            break;
        case DefaultShapes::BOX:
            p[0] = cx; p[1] = cy; p[2] = s; p[3] = s;
            break;
        case DefaultShapes::LINE:
            p[0] = cx - s * 0.8f; p[1] = cy - s * 0.7f;
            p[2] = cx + s * 0.8f; p[3] = cy + s * 0.7f;
            p[4] = 4.0f;
            break;
        case DefaultShapes::RAMP:
            p[0] = cx; p[1] = cy; p[2] = s; p[3] = s; p[4] = 0.0f;
            break;
        case DefaultShapes::STAR:
            p[0] = cx; p[1] = cy; p[2] = s;
            p[3] = 5.0f; p[4] = 5; p[5] = 0.0f;
            break;
        }
    }

    // =====================================================================
    // Combination toggles (second row) — two overlapping circles per type
    // =====================================================================
    void buildCombToggles()
    {
        const CombinationType ct[] = {
            CombinationType::Addition,
            CombinationType::Subtraction,
            CombinationType::Intersection,
            CombinationType::SmoothAddition,
            CombinationType::SmoothSubtraction
        };
        const char* label[] = { "Add", "Sub", "Int", "S+", "S-" };
        constexpr float r   = 12.0f;
        constexpr float off = 7.0f;

        for (int i = 0; i < 5; i++)
        {
            float cx = START_X + i * BTN_SPACING;
            float cy = COMB_Y;
            int   g  = COMB_GRP_BASE + i;

            float p1[8]{ cx - off * 0.5f, cy, r };
            Entity e1 = addUIShape(DefaultShapes::CIRCLE, p1,
                                   static_cast<uint16_t>(1),
                                   CombinationType::Addition, g);

            float p2[8]{ cx + off * 0.5f, cy, r };
            Entity e2 = addUIShape(DefaultShapes::CIRCLE, p2,
                                   static_cast<uint16_t>(1),
                                   ct[i], g);

            if (ct[i] == CombinationType::SmoothAddition ||
                ct[i] == CombinationType::SmoothSubtraction)
                m_ecs.getComponent<UIShape>(e2).smoothFactor = 5.0f;

            auto& tog = m_ecs.addComponent<ShapeToggle>(e1);
            tog.clickPadding = r + 10.0f;
            tog.parameterModifierMask.set(2);
            tog.modifierAmount = 3.0f;

            Entity lbl = m_ecs.createEntity();
            m_ecs.addComponent<Transform>(lbl).position =
                vec3(cx, cy - 2600.0f, 0.0f);
            auto& tx = m_ecs.addComponent<UITextRenderer>(lbl);
            tx.text     = label[i];
            tx.material = 1;
            tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;

            m_combButtons.push_back({ e1, ct[i] });
            blacklistEntity(e1);
            blacklistEntity(e2);
            blacklistEntity(lbl);
        }
        m_ecs.getComponent<ShapeToggle>(m_combButtons[0].toggleEntity).active = true;
    }

    // =====================================================================
    // Material toggles (bottom row)
    // =====================================================================
    void buildMaterialToggles()
    {
        for (int i = 0; i < 16; i++)
        {
            float px = START_X + i * MAT_SPACING;
            float p[8]{ px, MAT_Y, BTN_SIZE - 4.0f };
            Entity e;
            UIShape& sh = addUIShape(DefaultShapes::CIRCLE, p, e);
            sh.material  = static_cast<uint16_t>(i);

            auto& tog = m_ecs.addComponent<ShapeToggle>(e);
            tog.clickPadding = BTN_SIZE + 3.0f;
            tog.parameterModifierMask.set(2);
            tog.modifierAmount = 5.0f;

            m_materialToggles[i] = e;
            blacklistEntity(e);
        }
        m_ecs.getComponent<ShapeToggle>(m_materialToggles[m_selectedMaterial]).active = true;
    }

    // =====================================================================
    // Parameter editing panel (right side, hidden until selection)
    // =====================================================================
    void buildParamPanel()
    {
        m_selInfoText = m_ecs.createEntity();
        auto& selInfoTf = m_ecs.addComponent<Transform>(m_selInfoText);
        selInfoTf.position = vec3(HIDDEN, PANEL_TOP_Y + 35.0f, 0.0f);
        selInfoTf.isDirty = true;

        auto& hdr = m_ecs.addComponent<UITextRenderer>(m_selInfoText);
        hdr.material = 1;
        hdr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
        blacklistEntity(m_selInfoText);

        for (int i = 0; i < 8; i++)
        {
            float py = PANEL_TOP_Y - i * PARAM_GAP;

            float bp[8]{ HIDDEN, py, P_BTN_W, P_BTN_H };
            Entity be = addUIShape(DefaultShapes::BOX, bp,
                                   static_cast<uint16_t>(3));
            auto& btn = m_ecs.addComponent<ShapeButton>(be);
            btn.modifierAmount = 1.0f;
            btn.clickPadding   = 3.0f;

            Entity te = m_ecs.createEntity();
            auto& ttf = m_ecs.addComponent<Transform>(te);
            ttf.position = vec3(HIDDEN, py, 0.0f);
            ttf.isDirty = true;
            auto& tx = m_ecs.addComponent<UITextRenderer>(te);
            tx.material = 0;
            tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
            tx.verticalAlignment   = TextRenderer::VerticalAlignment::Center;

            m_paramBtns[i] = { be, te };
            blacklistEntity(be);
            blacklistEntity(te);
        }
    }

    // =====================================================================
    // Input
    // =====================================================================
    void onLeftClick()
    {
        for (auto& sb : m_shapeButtons)
        {
            if (m_ecs.getComponent<ShapeButton>(sb.entity).state == ButtonState::Down)
            {
                spawnShape(sb.shapeType);
                return;
            }
        }

        if (m_hasSelection)
        {
            for (int i = 0; i < 8; i++)
            {
                if (m_ecs.getComponent<ShapeButton>(m_paramBtns[i].shapeEntity)
                        .state == ButtonState::Down)
                {
                    promptParam(i);
                    return;
                }
            }
        }

        if (!Input::isUIClick())
        {
            auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
            vec2 wp   = ECS::Camera::screenPositionToWorldPosition2D(
                cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
            spawnPhysicsEntity(wp);
        }
    }

    void onRightClick()
    {
        auto& cam = m_ecs.getComponent<Transform>(m_mainCamera);
        vec2 wp   = ECS::Camera::screenPositionToWorldPosition2D(
            cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
        selectNearest(wp);
    }

    // =====================================================================
    // Selection
    // =====================================================================
    void selectNearest(vec2 pos)
    {
        auto cs = m_ecs.getComponentArray<CustomShape>();
        auto ui = m_ecs.getComponentArray<UIShape>();

        float  best = SEL_THRESH;
        Entity hit  = static_cast<Entity>(-1);

        for (size_t i = 0; i < cs->getSize(); i++)
        {
            Entity e = cs->getEntityAtIdx(i);
            if (ui->hasData(e)) continue;

            auto& s = cs->getDataAtIdx(i);
            float p[11]{};
            std::copy(std::begin(s.parameters), std::end(s.parameters), p);
            p[9]  = pos.x;
            p[10] = pos.y;
            m_sdfs[s.distanceFieldId]->propagateValues(p);
            float d = m_sdfs[s.distanceFieldId]->getValue();
            if (d < best) { best = d; hit = e; }
        }

        (hit != static_cast<Entity>(-1)) ? doSelect(hit) : doDeselect();
    }

    void doSelect(Entity e)
    {
        m_selectedEntity = e;
        m_hasSelection   = true;
        showPanel();
    }

    void doDeselect()
    {
        m_selectedEntity = static_cast<Entity>(-1);
        m_hasSelection   = false;
        hidePanel();
    }

    void deleteSelected()
    {
        m_ecs.destroyEntity(m_selectedEntity);
        m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
        doDeselect();
    }

    // =====================================================================
    // Panel show / hide / refresh
    // =====================================================================
    void showPanel()
    {
        for (int i = 0; i < 8; i++)
        {
            float py = PANEL_TOP_Y - i * PARAM_GAP;
            auto& u = m_ecs.getComponent<UIShape>(m_paramBtns[i].shapeEntity);
            u.parameters[0] = PANEL_X + (P_BTN_W) * 0.5f; 
            u.parameters[1] = py;
            u.isDirty = true;
            
            auto& t = m_ecs.getComponent<Transform>(m_paramBtns[i].textEntity);
            t.position = vec3(PANEL_X - P_BTN_W - 10.0f, py, 0.0f);
            t.isDirty = true;
        }
        auto& ht = m_ecs.getComponent<Transform>(m_selInfoText);
        ht.position = vec3(PANEL_X, PANEL_TOP_Y + 35.0f, 0.0f);
        ht.isDirty = true;
        m_UIRenderSystem.shaderNeedsUpdate() = true;
    }

    void hidePanel()
    {
        for (int i = 0; i < 8; i++)
        {
            auto& u = m_ecs.getComponent<UIShape>(m_paramBtns[i].shapeEntity);
            u.parameters[0] = HIDDEN; u.isDirty = true;
            auto& t = m_ecs.getComponent<Transform>(m_paramBtns[i].textEntity);
            t.position.x = HIDDEN; t.isDirty = true;
        }
        auto& ht = m_ecs.getComponent<Transform>(m_selInfoText);
        ht.position.x = HIDDEN; 
        ht.isDirty = true;
        m_UIRenderSystem.shaderNeedsUpdate() = true;
    }

    void refreshPanel()
    {
        if (!m_hasSelection) 
            return;
        if (!entityHasShape(m_selectedEntity)) { doDeselect(); return; }

        auto& cs = m_ecs.getComponent<CustomShape>(m_selectedEntity);

        auto& hdr = m_ecs.getComponent<UITextRenderer>(m_selInfoText);
        const char* name = shapeName(cs.distanceFieldId);
        if (hdr.text != name) { hdr.text = name; hdr.dirty = true; }

        int pc = paramCount(cs.distanceFieldId);
        for (int i = 0; i < 8; i++)
        {
            auto& tx = m_ecs.getComponent<UITextRenderer>(m_paramBtns[i].textEntity);
            if (i < pc)
            {
                char buf[48];
                std::snprintf(buf, sizeof(buf), "%s:%.2f",
                              paramName(cs.distanceFieldId, i),
                              cs.parameters[i]);
                if (tx.text != buf) { tx.text = buf; tx.dirty = true; }

                auto& u = m_ecs.getComponent<UIShape>(m_paramBtns[i].shapeEntity);
                if (u.parameters[0] < 0.0f)
                {
                    u.parameters[0] = PANEL_X + (P_BTN_W) * 0.5f;
                    u.parameters[1] = PANEL_TOP_Y - i * PARAM_GAP;
                    u.isDirty = true;
                }

                auto& t = m_ecs.getComponent<Transform>(m_paramBtns[i].textEntity);
                float py = PANEL_TOP_Y - i * PARAM_GAP;
                float txX = PANEL_X - P_BTN_W - 10.0f;
                if (t.position.x < 0.0f || std::abs(t.position.y - py) > 0.001f)
                {
                    t.position = vec3(txX, py, 0.0f);
                    t.isDirty = true;
                }
            }
            else
            {
                if (!tx.text.empty()) { tx.text.clear(); tx.dirty = true; }
                auto& u = m_ecs.getComponent<UIShape>(m_paramBtns[i].shapeEntity);
                if (u.parameters[0] > 0.0f)
                {
                    u.parameters[0] = HIDDEN; u.isDirty = true;
                    auto& t2 = m_ecs.getComponent<Transform>(m_paramBtns[i].textEntity);
                    t2.position.x = HIDDEN; t2.isDirty = true;
                }
            }
        }
    }

    bool entityHasShape(Entity e)
    {
        auto arr = m_ecs.getComponentArray<CustomShape>();
        for (size_t i = 0; i < arr->getSize(); i++)
            if (arr->getEntityAtIdx(i) == e) return true;
        return false;
    }

    // =====================================================================
    // cin-based parameter input
    // =====================================================================
    void promptParam(int idx)
    {
        if (!m_hasSelection || !entityHasShape(m_selectedEntity)) return;

        auto& cs = m_ecs.getComponent<CustomShape>(m_selectedEntity);
        int pc   = paramCount(cs.distanceFieldId);
        if (idx >= pc) return;

        std::cout << "[" << shapeName(cs.distanceFieldId) << "] "
                  << paramName(cs.distanceFieldId, idx)
                  << " (now " << cs.parameters[idx] << "): " << std::flush;

        float v;
        if (std::cin >> v)
        {
            if (cs.distanceFieldId == DefaultShapes::STAR && idx == 4)
                v = std::round(v);
            cs.parameters[idx] = v;
            cs.isDirty = true;
            m_sdfRenderSystem2D.shaderNeedsUpdate() = true;
        }
        else
        {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
        }
    }

    // =====================================================================
    // Exclusive toggle sync
    // =====================================================================
    void syncMaterialToggles()
    {
        int activated = -1;
        for (int i = 0; i < 16; i++)
        {
            auto& t = m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]);
            if (t.active && t.state == ButtonState::Down)
                { activated = i; break; }
        }
        if (activated >= 0)
        {
            m_selectedMaterial = activated;
            for (int i = 0; i < 16; i++)
                if (i != activated)
                    m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]).active = false;
        }
        else
        {
            for (int i = 0; i < 16; i++)
                if (m_ecs.getComponent<ShapeToggle>(m_materialToggles[i]).active)
                    { m_selectedMaterial = i; break; }
        }
    }

    void syncCombToggles()
    {
        int activated = -1;
        for (int i = 0; i < (int)m_combButtons.size(); i++)
        {
            auto& t = m_ecs.getComponent<ShapeToggle>(m_combButtons[i].toggleEntity);
            if (t.active && t.state == ButtonState::Down)
                { activated = i; break; }
        }
        if (activated >= 0)
        {
            m_selectedCombIdx     = activated;
            m_selectedCombination = m_combButtons[activated].combType;
            for (int i = 0; i < (int)m_combButtons.size(); i++)
                if (i != activated)
                    m_ecs.getComponent<ShapeToggle>(m_combButtons[i].toggleEntity).active = false;
        }
        else
        {
            for (int i = 0; i < (int)m_combButtons.size(); i++)
                if (m_ecs.getComponent<ShapeToggle>(m_combButtons[i].toggleEntity).active)
                {
                    m_selectedCombIdx     = i;
                    m_selectedCombination = m_combButtons[i].combType;
                    break;
                }
        }
    }

    // =====================================================================
    // Spawning
    // =====================================================================
    void spawnShape(uint16_t type)
    {
        float p[8]{};
        fillRandomParams(type, p);
        Entity e = addShape(type, p,
                            static_cast<uint16_t>(m_selectedMaterial),
                            m_selectedCombination);
        if (m_selectedCombination == CombinationType::SmoothAddition ||
            m_selectedCombination == CombinationType::SmoothSubtraction)
            m_ecs.getComponent<CustomShape>(e).smoothFactor = 1.5f;
        doSelect(e);
    }

    void spawnPhysicsEntity(vec2 wp)
    {
        Entity e   = m_ecs.createEntity();
        auto& t    = m_ecs.addComponent<Transform>(e);
        t.position = vec3(wp.x, wp.y, 0.0f);
        auto& sdf    = m_ecs.addComponent<SDFRenderer>(e);
        sdf.materialId = static_cast<unsigned int>(m_selectedMaterial);
        m_ecs.addComponent<RigidBody2D>(e);
    }

    // =====================================================================
    // Shape metadata (covers all types for editing loaded scenes)
    // =====================================================================
    static const char* shapeName(uint16_t t)
    {
        switch (t) {
        case DefaultShapes::CIRCLE:   return "Circle";
        case DefaultShapes::BOX:      return "Box";
        case DefaultShapes::BOX_LINE: return "BoxLine";
        case DefaultShapes::LINE:     return "Line";
        case DefaultShapes::RAMP:     return "Ramp";
        case DefaultShapes::SINE:     return "Sine";
        case DefaultShapes::STAR:     return "Star";
        default: return "Shape";
        }
    }

    static int paramCount(uint16_t t)
    {
        switch (t) {
        case DefaultShapes::CIRCLE:   return 3;
        case DefaultShapes::BOX:      return 4;
        case DefaultShapes::BOX_LINE: return 5;
        case DefaultShapes::LINE:     return 5;
        case DefaultShapes::RAMP:     return 5;
        case DefaultShapes::SINE:     return 4;
        case DefaultShapes::STAR:     return 6;
        default: return 3;
        }
    }

    static const char* paramName(uint16_t t, int i)
    {
        static const char* C[]  = { "posX","posY","rad" };
        static const char* B[]  = { "posX","posY","hW","hH" };
        static const char* BL[] = { "posX","posY","hW","hH","thick" };
        static const char* L[]  = { "Ax","Ay","Bx","By","w" };
        static const char* R[]  = { "posX","posY","w","h","skew" };
        static const char* SI[] = { "amp","per","spd","yOff" };
        static const char* ST[] = { "posX","posY","rad","disp","pts","spin" };
        switch (t) {
        case DefaultShapes::CIRCLE:   return C[i];
        case DefaultShapes::BOX:      return B[i];
        case DefaultShapes::BOX_LINE: return BL[i];
        case DefaultShapes::LINE:     return L[i];
        case DefaultShapes::RAMP:     return R[i];
        case DefaultShapes::SINE:     return SI[i];
        case DefaultShapes::STAR:     return ST[i];
        default: return "?";
        }
    }

    // =====================================================================
    // Random parameter generation
    // =====================================================================
    float rnd(float lo, float hi)
    {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(m_rng);
    }

    vec2 camCentre()
    {
        auto& t = m_ecs.getComponent<Transform>(m_mainCamera);
        return vec2(t.position.x, t.position.y);
    }

    void fillRandomParams(uint16_t type, float p[8])
    {
        std::memset(p, 0, sizeof(float) * 8);
        vec2 c = camCentre();
        switch (type)
        {
        case DefaultShapes::CIRCLE:
            p[0] = c.x + rnd(-2.0f, 2.0f);
            p[1] = c.y + rnd(-2.0f, 2.0f);
            p[2] = rnd(0.6f, 2.5f);
            break;
        case DefaultShapes::BOX:
            p[0] = c.x + rnd(-3.0f, 3.0f);
            p[1] = c.y + rnd(-3.0f, 3.0f);
            p[2] = rnd(0.5f, 3.0f);
            p[3] = rnd(0.5f, 3.0f);
            break;
        case DefaultShapes::LINE:
            p[0] = c.x + rnd(-3.0f, -0.5f);
            p[1] = c.y + rnd(-2.0f, 2.0f);
            p[2] = c.x + rnd(0.5f, 3.0f);
            p[3] = c.y + rnd(-2.0f, 2.0f);
            p[4] = rnd(0.05f, 0.3f);
            break;
        case DefaultShapes::RAMP:
            p[0] = c.x + rnd(-2.0f, 2.0f);
            p[1] = c.y + rnd(-2.0f, 2.0f);
            p[2] = rnd(1.0f, 4.0f);
            p[3] = rnd(1.0f, 4.0f);
            p[4] = rnd(-1.5f, 1.5f);
            break;
        case DefaultShapes::STAR:
            p[0] = c.x + rnd(-2.0f, 2.0f);
            p[1] = c.y + rnd(-2.0f, 2.0f);
            p[2] = rnd(0.8f, 2.5f);
            p[3] = rnd(0.1f, 0.6f);
            p[4] = static_cast<float>(static_cast<int>(rnd(3.0f, 8.0f)));
            p[5] = rnd(0.5f, 2.0f);
            break;
        default:
            p[0] = c.x; p[1] = c.y; p[2] = 1.0f;
            break;
        }
    }
};
