#pragma once

#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;

class SceneLoadExample : public Scene
{
public:
	SceneLoadExample(const PhysicsSettings& settings)
		: Scene(settings)
		, m_rng(12345)
	{
	}

	ECSManager* m_tempEcs = nullptr;

private:
	// =====================================================================
	// Types
	// =====================================================================
	struct ShapeBtnInfo
	{
		Entity entity;
		uint16_t shapeType;
	};
	struct CombBtnInfo
	{
		Entity toggleEntity;
		CombinationType combType;
	};
	struct ParamBtn
	{
		Entity shapeEntity;
		Entity textEntity;
	};

	// =====================================================================
	// State
	// =====================================================================
	std::vector<ShapeBtnInfo> m_shapeButtons;
	std::vector<CombBtnInfo> m_combButtons;
	std::array<Entity, 16> m_materialToggles{};
	std::array<ParamBtn, 8> m_paramBtns{};
	Entity m_selInfoText{};

	int m_selectedMaterial = 1;
	int m_selectedCombIdx = 0;
	CombinationType m_selectedCombination = CombinationType::Addition;

	Entity m_selectedEntity = static_cast<Entity>(-1);
	bool m_hasSelection = false;

	std::mt19937 m_rng;

	// =====================================================================
	// Layout (800x800 screen)
	// =====================================================================
	static constexpr float BTN_SIZE = 18.0f;
	static constexpr float BTN_SPACING = 56.0f;
	static constexpr float START_X = 40.0f;
	static constexpr float SHAPE_Y = 770.0f;
	static constexpr float COMB_Y = 706.0f;
	static constexpr float MAT_Y = 50.0f;
	static constexpr float MAT_SPACING = 47.0f;

	static constexpr float PANEL_X = 750.0f;
	static constexpr float PANEL_TOP_Y = 620.0f;
	static constexpr float PARAM_GAP = 48.0f;
	static constexpr float P_BTN_W = 24.0f;
	static constexpr float P_BTN_H = 16.0f;

	static constexpr float HIDDEN = -5000.0f;
	static constexpr int COMB_GRP_BASE = 100;
	static constexpr float SEL_THRESH = 5.0f;

	// =====================================================================
	// Lifecycle
	// =====================================================================
	void onStart(ECSManager& ecs, const TagMap& tags) override
	{
		m_tempEcs = &ecs;
		m_debugInput = true;
		m_debugFly = true;
		m_tempEcs->getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

		buildShapeButtons();
		buildCombToggles();
		buildMaterialToggles();
		buildParamPanel();
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		m_tempEcs = &ecs;
		g_cameraPositon = m_tempEcs->getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
			setSceneComplete();
		if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::S))
			saveScene(ASSETS_PATH "example.weird");

		syncMaterialToggles();
		syncCombToggles();

		if (Input::GetMouseButtonDown(Input::LeftClick))
			onLeftClick();
		if (Input::GetMouseButton(Input::LeftClick))
		{
			auto& cam = m_tempEcs->getComponent<Transform>(m_mainCamera);
			vec2 wp = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
			spawnPhysicsEntity(wp);
		}
		if (Input::GetMouseButtonDown(Input::RightClick))
			onRightClick();
		if (Input::GetKeyDown(Input::X) && m_hasSelection)
			deleteSelected();

		refreshPanel();
		destroyOffscreenEntities();
	}

	void destroyOffscreenEntities()
	{
		auto transformArray = m_tempEcs->getComponentArray<Transform>();
		if (!transformArray)
			return;

		for (size_t i = 0; i < transformArray->getSize(); ++i)
		{
			auto& transform = transformArray->getDataAtIdx(i);
			Entity entity = transform.Owner;
			if (entity == m_mainCamera)
				continue;

			if (transform.position.y < -10.0f)
			{
				m_tempEcs->destroyEntity(entity);
			}
		}
	}

	// =====================================================================
	// Shape buttons (top row) — each uses its own SDF as a preview
	// =====================================================================
	void buildShapeButtons()
	{
		const uint16_t types[] = {DefaultShapes::CIRCLE, DefaultShapes::BOX, DefaultShapes::TRIANGLE, DefaultShapes::LINE, DefaultShapes::RAMP,
								  DefaultShapes::STAR};

		for (int i = 0; i < 6; i++)
		{
			float cx = START_X + i * BTN_SPACING;
			float cy = SHAPE_Y;
			float p[8]{};
			previewParams(types[i], cx, cy, p);

			Entity e = addUIShape(types[i], p, 2);
			auto& b = m_tempEcs->addComponent<ShapeButton>(e);
			b.modifierAmount = 1.0f;
			b.clickPadding = 8.0f;

			m_shapeButtons.push_back({e, types[i]});
			blacklistEntity(e);
		}
	}

	void previewParams(uint16_t t, float cx, float cy, float p[8])
	{
		const float s = BTN_SIZE;
		if (t == DefaultShapes::CIRCLE)
		{
			p[0] = cx;
			p[1] = cy;
			p[2] = s;
		}
		else if (t == DefaultShapes::BOX)
		{
			p[0] = cx;
			p[1] = cy;
			p[2] = s;
			p[3] = s;
		}
		else if (t == DefaultShapes::TRIANGLE)
		{
			p[0] = cx;
			p[1] = cy;
			p[2] = 2.0f * s;
			p[3] = 2.0f * s;
			p[4] = 0.0f;
		}
		else if (t == DefaultShapes::LINE)
		{
			p[0] = cx - s * 0.8f;
			p[1] = cy - s * 0.7f;
			p[2] = cx + s * 0.8f;
			p[3] = cy + s * 0.7f;
			p[4] = 4.0f;
		}
		else if (t == DefaultShapes::RAMP)
		{
			p[0] = cx;
			p[1] = cy;
			p[2] = s;
			p[3] = s;
			p[4] = 0.0f;
		}
		else if (t == DefaultShapes::STAR)
		{
			p[0] = cx;
			p[1] = cy;
			p[2] = s;
			p[3] = 5.0f;
			p[4] = 5;
			p[5] = 0.0f;
		}
	}

	// =====================================================================
	// Combination toggles (second row) — two overlapping circles per type
	// =====================================================================
	void buildCombToggles()
	{
		const CombinationType ct[] = {CombinationType::Addition, CombinationType::Subtraction,
									  CombinationType::Intersection, CombinationType::SmoothAddition,
									  CombinationType::SmoothSubtraction};
		const char* label[] = {"Add", "Sub", "Int", "S+", "S-"};
		constexpr float r = 12.0f;
		constexpr float off = 7.0f;

		for (int i = 0; i < 5; i++)
		{
			float cx = START_X + i * BTN_SPACING;
			float cy = COMB_Y;
			int g = COMB_GRP_BASE + i;

			float p1[8]{cx - off * 0.5f, cy, r};
			Entity e1 = addUIShape(DefaultShapes::CIRCLE, p1, static_cast<uint16_t>(1), CombinationType::Addition, g);

			float p2[8]{cx + off * 0.5f, cy, r};
			Entity e2 = addUIShape(DefaultShapes::CIRCLE, p2, static_cast<uint16_t>(1), ct[i], g);

			if (ct[i] == CombinationType::SmoothAddition || ct[i] == CombinationType::SmoothSubtraction)
				m_tempEcs->getComponent<UIShape>(e2).smoothFactor = 5.0f;

			auto& tog = m_tempEcs->addComponent<ShapeToggle>(e1);
			tog.clickPadding = r + 10.0f;
			tog.parameterModifierMask.set(2);
			tog.modifierAmount = 3.0f;

			Entity lbl = m_tempEcs->createEntity();
			m_tempEcs->addComponent<Transform>(lbl).position = vec3(cx, cy - 2600.0f, 0.0f);
			auto& tx = m_tempEcs->addComponent<UITextRenderer>(lbl);
			tx.text = label[i];
			tx.material = 1;
			tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;

			m_combButtons.push_back({e1, ct[i]});
			blacklistEntity(e1);
			blacklistEntity(e2);
			blacklistEntity(lbl);
		}
		m_tempEcs->getComponent<ShapeToggle>(m_combButtons[0].toggleEntity).active = true;
	}

	// =====================================================================
	// Material toggles (bottom row)
	// =====================================================================
	void buildMaterialToggles()
	{
		for (int i = 0; i < 16; i++)
		{
			float px = START_X + i * MAT_SPACING;
			float p[8]{px, MAT_Y, BTN_SIZE - 4.0f};
			Entity e;
			UIShape& sh = addUIShape(DefaultShapes::CIRCLE, p, e);
			sh.material = static_cast<uint16_t>(i);

			auto& tog = m_tempEcs->addComponent<ShapeToggle>(e);
			tog.clickPadding = BTN_SIZE + 3.0f;
			tog.parameterModifierMask.set(2);
			tog.modifierAmount = 5.0f;

			m_materialToggles[i] = e;
			blacklistEntity(e);
		}
		m_tempEcs->getComponent<ShapeToggle>(m_materialToggles[m_selectedMaterial]).active = true;
	}

	// =====================================================================
	// Parameter editing panel (right side, hidden until selection)
	// =====================================================================
	void buildParamPanel()
	{
		m_selInfoText = m_tempEcs->createEntity();
		auto& selInfoTf = m_tempEcs->addComponent<Transform>(m_selInfoText);
		selInfoTf.position = vec3(HIDDEN, PANEL_TOP_Y + 35.0f, 0.0f);
		selInfoTf.isDirty = true;

		auto& hdr = m_tempEcs->addComponent<UITextRenderer>(m_selInfoText);
		hdr.material = 1;
		hdr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
		blacklistEntity(m_selInfoText);

		for (int i = 0; i < 8; i++)
		{
			float py = PANEL_TOP_Y - i * PARAM_GAP;

			float bp[8]{HIDDEN, py, P_BTN_W, P_BTN_H};
			Entity be = addUIShape(DefaultShapes::BOX, bp, static_cast<uint16_t>(3));
			auto& btn = m_tempEcs->addComponent<ShapeButton>(be);
			btn.modifierAmount = 1.0f;
			btn.clickPadding = 3.0f;

			Entity te = m_tempEcs->createEntity();
			auto& ttf = m_tempEcs->addComponent<Transform>(te);
			ttf.position = vec3(HIDDEN, py, 0.0f);
			ttf.isDirty = true;
			auto& tx = m_tempEcs->addComponent<UITextRenderer>(te);
			tx.material = 0;
			tx.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			tx.verticalAlignment = TextRenderer::VerticalAlignment::Center;

			m_paramBtns[i] = {be, te};
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
			if (m_tempEcs->getComponent<ShapeButton>(sb.entity).state == ButtonState::Down)
			{
				spawnShape(sb.shapeType);
				return;
			}
		}

		if (m_hasSelection)
		{
			for (int i = 0; i < 8; i++)
			{
				if (m_tempEcs->getComponent<ShapeButton>(m_paramBtns[i].shapeEntity).state == ButtonState::Down)
				{
					promptParam(i);
					return;
				}
			}
		}
	}

	void onRightClick()
	{
		auto& cam = m_tempEcs->getComponent<Transform>(m_mainCamera);
		vec2 wp = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(Input::GetMouseX(), Input::GetMouseY()));
		selectNearest(wp);
	}

	// =====================================================================
	// Selection
	// =====================================================================
	void selectNearest(vec2 pos)
	{
		auto cs = m_tempEcs->getComponentArray<CustomShape>();
		auto ui = m_tempEcs->getComponentArray<UIShape>();

		float best = SEL_THRESH;
		Entity hit = static_cast<Entity>(-1);

		for (size_t i = 0; i < cs->getSize(); i++)
		{
			Entity e = cs->getEntityAtIdx(i);
			if (ui->hasData(e))
				continue;

			auto& s = cs->getDataAtIdx(i);
			float p[11]{};
			std::copy(std::begin(s.parameters), std::end(s.parameters), p);
			p[9] = pos.x;
			p[10] = pos.y;
			float d = m_sdfs[s.distanceFieldId]->getValue(p);
			if (d < best)
			{
				best = d;
				hit = e;
			}
		}

		(hit != static_cast<Entity>(-1)) ? doSelect(hit) : doDeselect();
	}

	void doSelect(Entity e)
	{
		m_selectedEntity = e;
		m_hasSelection = true;
		showPanel();
	}

	void doDeselect()
	{
		m_selectedEntity = static_cast<Entity>(-1);
		m_hasSelection = false;
		hidePanel();
	}

	void deleteSelected()
	{
		m_tempEcs->destroyEntity(m_selectedEntity);
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
			auto& u = m_tempEcs->getComponent<UIShape>(m_paramBtns[i].shapeEntity);
			u.parameters[0] = PANEL_X + (P_BTN_W) * 0.5f;
			u.parameters[1] = py;

			auto& t = m_tempEcs->getComponent<Transform>(m_paramBtns[i].textEntity);
			t.position = vec3(PANEL_X - P_BTN_W - 10.0f, py, 0.0f);
			t.isDirty = true;
		}
		auto& ht = m_tempEcs->getComponent<Transform>(m_selInfoText);
		ht.position = vec3(PANEL_X, PANEL_TOP_Y + 35.0f, 0.0f);
		ht.isDirty = true;
		// m_UIRenderSystem.shaderNeedsUpdate() = true;
	}

	void hidePanel()
	{
		for (int i = 0; i < 8; i++)
		{
			auto& u = m_tempEcs->getComponent<UIShape>(m_paramBtns[i].shapeEntity);
			u.parameters[0] = HIDDEN;
			auto& t = m_tempEcs->getComponent<Transform>(m_paramBtns[i].textEntity);
			t.position.x = HIDDEN;
			t.isDirty = true;
		}
		auto& ht = m_tempEcs->getComponent<Transform>(m_selInfoText);
		ht.position.x = HIDDEN;
		ht.isDirty = true;
		// m_UIRenderSystem.shaderNeedsUpdate() = true;
	}

	void refreshPanel()
	{
		if (!m_hasSelection)
			return;
		if (!entityHasShape(m_selectedEntity))
		{
			doDeselect();
			return;
		}

		auto& cs = m_tempEcs->getComponent<CustomShape>(m_selectedEntity);

		auto& hdr = m_tempEcs->getComponent<UITextRenderer>(m_selInfoText);
		const char* name = shapeName(cs.distanceFieldId);
		if (hdr.text != name)
		{
			hdr.text = name;
			hdr.dirty = true;
		}

		int pc = paramCount(cs.distanceFieldId);
		for (int i = 0; i < 8; i++)
		{
			auto& tx = m_tempEcs->getComponent<UITextRenderer>(m_paramBtns[i].textEntity);
			if (i < pc)
			{
				char buf[48];
				std::snprintf(buf, sizeof(buf), "%s:%.2f", paramName(cs.distanceFieldId, i), cs.parameters[i]);
				if (tx.text != buf)
				{
					tx.text = buf;
					tx.dirty = true;
				}

				auto& u = m_tempEcs->getComponent<UIShape>(m_paramBtns[i].shapeEntity);
				if (u.parameters[0] < 0.0f)
				{
					u.parameters[0] = PANEL_X + (P_BTN_W) * 0.5f;
					u.parameters[1] = PANEL_TOP_Y - i * PARAM_GAP;
				}

				auto& t = m_tempEcs->getComponent<Transform>(m_paramBtns[i].textEntity);
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
				if (!tx.text.empty())
				{
					tx.text.clear();
					tx.dirty = true;
				}
				auto& u = m_tempEcs->getComponent<UIShape>(m_paramBtns[i].shapeEntity);
				if (u.parameters[0] > 0.0f)
				{
					u.parameters[0] = HIDDEN;
					auto& t2 = m_tempEcs->getComponent<Transform>(m_paramBtns[i].textEntity);
					t2.position.x = HIDDEN;
					t2.isDirty = true;
				}
			}
		}
	}

	bool entityHasShape(Entity e)
	{
		auto arr = m_tempEcs->getComponentArray<CustomShape>();
		for (size_t i = 0; i < arr->getSize(); i++)
			if (arr->getEntityAtIdx(i) == e)
				return true;
		return false;
	}

	// =====================================================================
	// cin-based parameter input
	// =====================================================================
	void promptParam(int idx)
	{
		if (!m_hasSelection || !entityHasShape(m_selectedEntity))
			return;

		auto& cs = m_tempEcs->getComponent<CustomShape>(m_selectedEntity);
		int pc = paramCount(cs.distanceFieldId);
		if (idx >= pc)
			return;

		std::string msg = "[" + std::string(shapeName(cs.distanceFieldId)) + "] " + std::string(paramName(cs.distanceFieldId, idx)) + " (now " + std::to_string(cs.parameters[idx]) + "): ";
		WeirdEngine::Logger::log(msg);

		float v;
		if (std::cin >> v)
		{
			if (cs.distanceFieldId == DefaultShapes::STAR && idx == 4)
				v = std::round(v);
			cs.parameters[idx] = v;
			cs.isDirty = true;
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
			auto& t = m_tempEcs->getComponent<ShapeToggle>(m_materialToggles[i]);
			if (t.active && t.state == ButtonState::Down)
			{
				activated = i;
				break;
			}
		}
		if (activated >= 0)
		{
			m_selectedMaterial = activated;
			for (int i = 0; i < 16; i++)
				if (i != activated)
					m_tempEcs->getComponent<ShapeToggle>(m_materialToggles[i]).active = false;
		}
		else
		{
			for (int i = 0; i < 16; i++)
				if (m_tempEcs->getComponent<ShapeToggle>(m_materialToggles[i]).active)
				{
					m_selectedMaterial = i;
					break;
				}
		}
	}

	void syncCombToggles()
	{
		int activated = -1;
		for (int i = 0; i < (int)m_combButtons.size(); i++)
		{
			auto& t = m_tempEcs->getComponent<ShapeToggle>(m_combButtons[i].toggleEntity);
			if (t.active && t.state == ButtonState::Down)
			{
				activated = i;
				break;
			}
		}
		if (activated >= 0)
		{
			m_selectedCombIdx = activated;
			m_selectedCombination = m_combButtons[activated].combType;
			for (int i = 0; i < (int)m_combButtons.size(); i++)
				if (i != activated)
					m_tempEcs->getComponent<ShapeToggle>(m_combButtons[i].toggleEntity).active = false;
		}
		else
		{
			for (int i = 0; i < (int)m_combButtons.size(); i++)
				if (m_tempEcs->getComponent<ShapeToggle>(m_combButtons[i].toggleEntity).active)
				{
					m_selectedCombIdx = i;
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
		Entity e = addShape(type, p, static_cast<uint16_t>(m_selectedMaterial), m_selectedCombination);
		if (m_selectedCombination == CombinationType::SmoothAddition ||
			m_selectedCombination == CombinationType::SmoothSubtraction)
			m_tempEcs->getComponent<CustomShape>(e).smoothFactor = 1.5f;
		doSelect(e);
	}

	void spawnPhysicsEntity(vec2 wp)
	{
		Entity e = m_tempEcs->createEntity();
		auto& t = m_tempEcs->addComponent<Transform>(e);
		t.position = vec3(wp.x, wp.y, 0.0f);
		t.isDirty = true;
		auto& sdf = m_tempEcs->addComponent<Dot>(e);
		sdf.materialId = static_cast<unsigned int>(m_selectedMaterial);
		m_tempEcs->addComponent<RigidBody2D>(e);
		blacklistEntity(e);
	}

	// =====================================================================
	// Shape metadata (covers all types for editing loaded scenes)
	// =====================================================================
	static const char* shapeName(uint16_t t)
	{
		if (t == DefaultShapes::CIRCLE) return "Circle";
		if (t == DefaultShapes::BOX) return "Box";
		if (t == DefaultShapes::BOX_LINE) return "BoxLine";
		if (t == DefaultShapes::TRIANGLE) return "Triangle";
		if (t == DefaultShapes::TRIANGLE_LINE) return "TriangleLine";
		if (t == DefaultShapes::LINE) return "Line";
		if (t == DefaultShapes::RAMP) return "Ramp";
		if (t == DefaultShapes::SINE) return "Sine";
		if (t == DefaultShapes::STAR) return "Star";
		return "Shape";
	}

	static int paramCount(uint16_t t)
	{
		if (t == DefaultShapes::CIRCLE) return 3;
		if (t == DefaultShapes::BOX) return 4;
		if (t == DefaultShapes::BOX_LINE) return 5;
		if (t == DefaultShapes::TRIANGLE) return 5;
		if (t == DefaultShapes::TRIANGLE_LINE) return 6;
		if (t == DefaultShapes::LINE) return 5;
		if (t == DefaultShapes::RAMP) return 5;
		if (t == DefaultShapes::SINE) return 4;
		if (t == DefaultShapes::STAR) return 6;
		return 3;
	}

	static const char* paramName(uint16_t t, int i)
	{
		static const char* C[] = {"posX", "posY", "rad"};
		static const char* B[] = {"posX", "posY", "hW", "hH"};
		static const char* BL[] = {"posX", "posY", "hW", "hH", "thick"};
		static const char* T[] = {"posX", "posY", "w", "h", "angle"};
		static const char* TL[] = {"posX", "posY", "w", "h", "angle", "thick"};
		static const char* L[] = {"Ax", "Ay", "Bx", "By", "w"};
		static const char* R[] = {"posX", "posY", "w", "h", "skew"};
		static const char* SI[] = {"amp", "per", "spd", "yOff"};
		static const char* ST[] = {"posX", "posY", "rad", "disp", "pts", "spin"};
		if (t == DefaultShapes::CIRCLE) return C[i];
		if (t == DefaultShapes::BOX) return B[i];
		if (t == DefaultShapes::BOX_LINE) return BL[i];
		if (t == DefaultShapes::TRIANGLE) return T[i];
		if (t == DefaultShapes::TRIANGLE_LINE) return TL[i];
		if (t == DefaultShapes::LINE) return L[i];
		if (t == DefaultShapes::RAMP) return R[i];
		if (t == DefaultShapes::SINE) return SI[i];
		if (t == DefaultShapes::STAR) return ST[i];
		return "?";
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
		auto& t = m_tempEcs->getComponent<Transform>(m_mainCamera);
		return vec2(t.position.x, t.position.y);
	}

	void fillRandomParams(uint16_t type, float p[8])
	{
		std::memset(p, 0, sizeof(float) * 8);
		vec2 c = camCentre();
		if (type == DefaultShapes::CIRCLE)
		{
			p[0] = c.x + rnd(-2.0f, 2.0f);
			p[1] = c.y + rnd(-2.0f, 2.0f);
			p[2] = rnd(0.6f, 2.5f);
		}
		else if (type == DefaultShapes::BOX)
		{
			p[0] = c.x + rnd(-3.0f, 3.0f);
			p[1] = c.y + rnd(-3.0f, 3.0f);
			p[2] = rnd(0.5f, 3.0f);
			p[3] = rnd(0.5f, 3.0f);
		}
		else if (type == DefaultShapes::LINE)
		{
			p[0] = c.x + rnd(-3.0f, -0.5f);
			p[1] = c.y + rnd(-2.0f, 2.0f);
			p[2] = c.x + rnd(0.5f, 3.0f);
			p[3] = c.y + rnd(-2.0f, 2.0f);
			p[4] = rnd(0.05f, 0.3f);
		}
		else if (type == DefaultShapes::RAMP)
		{
			p[0] = c.x + rnd(-2.0f, 2.0f);
			p[1] = c.y + rnd(-2.0f, 2.0f);
			p[2] = rnd(1.0f, 4.0f);
			p[3] = rnd(1.0f, 4.0f);
			p[4] = rnd(-1.5f, 1.5f);
		}
		else if (type == DefaultShapes::STAR)
		{
			p[0] = c.x + rnd(-2.0f, 2.0f);
			p[1] = c.y + rnd(-2.0f, 2.0f);
			p[2] = rnd(0.8f, 2.5f);
			p[3] = rnd(0.1f, 0.6f);
			p[4] = static_cast<float>(static_cast<int>(rnd(3.0f, 8.0f)));
			p[5] = rnd(0.5f, 2.0f);
		}
		else
		{
			p[0] = c.x;
			p[1] = c.y;
			p[2] = 1.0f;
			p[3] = 1.0f;
			p[4] = 0.0f;
		}
	}
};
