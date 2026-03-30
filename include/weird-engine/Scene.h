#pragma once
#include "../weird-renderer/Shape.h"
#include "../weird-physics/Simulation.h"
#include "../weird-physics/Simulation2D.h"
#include "../weird-renderer/RenderTarget.h"
#include "../weird-renderer/Shape.h"

#include "ecs/ECS.h"

#include "ResourceManager.h"

#include <map>
#include <string>

namespace WeirdEngine
{
	using namespace ECS;

	class Scene
	{
	public:
		Scene();
		~Scene();
		void start();
		void startFromFile(const std::string& path);

		void renderModels(WeirdRenderer::RenderTarget& renderTarget, WeirdRenderer::Shader& shader, WeirdRenderer::Shader& instancingShader);

		void updateRayMarchingShader(WeirdRenderer::Shader& shader);
		void update(double delta, double time);

		void get2DShapesData(WeirdRenderer::Dot2D*& data, uint32_t& size);

		Scene(const Scene&) = default;						 // Deleted copy constructor
		Scene& operator=(const Scene&) = default; // Deleted copy assignment operator
		Scene(Scene&&) = default;								 // Defaulted move constructor
		Scene& operator=(Scene&&) = default;			 // Defaulted move assignment operator

		WeirdRenderer::Camera& getCamera();
		std::vector<WeirdRenderer::Light>& getLigths();

		float getTime();

		void fillShapeDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size);

		enum class RenderMode
		{
			Simple3D,
			RayMarching3D,
			RayMarching2D,
			RayMarchingBoth
		};

		RenderMode getRenderMode() const;

		// Tag system: returns all tags currently assigned in this scene
		const std::map<std::string, Entity>& getTags() const;

	protected:
		virtual void onCreate() {};
		virtual void onStart() = 0;
		virtual void onUpdate(float delta) = 0;
		virtual void onRender(WeirdRenderer::RenderTarget& renderTarget) {};
		virtual void onCollision(WeirdEngine::CollisionEvent& event) {};
		virtual void onDestroy() {};
		// Called after startFromFile() has loaded tags from a .weird file
		virtual void onStartFromFile(const std::map<std::string, Entity>& tags) {};

		ECSManager m_ecs;
		Entity m_mainCamera;
		ResourceManager m_resourceManager;
		Simulation2D m_simulation2D;

		std::vector<std::shared_ptr<IMathExpression>> m_sdfs;

		Entity addShape(ShapeId shapeId, float* variables, uint16_t material, CombinationType combination = CombinationType::Addition, bool hasCollision = true, int group = 0);

		void lookAt(Entity entity);

		SDFRenderSystem m_sdfRenderSystem;
		SDFRenderSystem2D m_sdfRenderSystem2D;
		RenderSystem m_renderSystem;
		InstancedRenderSystem m_instancedRenderSystem;
		PhysicsSystem2D m_rbPhysicsSystem2D;
		PlayerMovementSystem m_playerMovementSystem;
		CameraSystem m_cameraSystem;

		PhysicsInteractionSystem m_physicsInteractionSystem;

		bool m_debugFly = true;
		bool m_debugInput = false;

		RenderMode m_renderMode = RenderMode::RayMarching2D;

		int m_charWidth;
		int m_charHeight;

		std::vector<std::vector<vec2>> m_letters;

		void print(const std::string& text);
		void printAtRow(const std::string& text, int row);
		void clearText();
		void loadFont(const char* imagePath, int charWidth, int charHeight, const char* characters);

		// Entities created by print()/printAtRow(); cleared by clearText()
		std::vector<Entity> m_textEntities;

		// --- Tag system ---
		// Assign a unique tag to an entity. If the tag is already used by another
		// entity, it is removed from that entity first. Passing an empty name
		// removes any existing tag from the entity.
		void tag(Entity entity, const std::string& name);
		// Remove the tag from an entity (no-op if the entity has no tag).
		void removeTag(Entity entity);
		// Return the tag assigned to an entity, or "" if none.
		std::string getTag(Entity entity) const;
		// Return the entity that owns a given tag, or MAX_ENTITIES if not found.
		Entity getEntityByTag(const std::string& name) const;
		// Save all current tags to a .weird file.
		void saveTagsToFile(const std::string& path);
		// Load tags from a .weird file and return them as a map.
		// Does NOT automatically apply them to the scene; call tag() as needed.
		std::map<std::string, Entity> loadTagsFromFile(const std::string& path);

	private:
		// Char lookup table
		std::array<int, 256> m_CharLookUpTable{};

		// Lookup function
		int getIndex(char c)
		{
			return m_CharLookUpTable[static_cast<unsigned char>(c)];
		}

		// Bidirectional tag maps: tag name <-> entity
		std::map<std::string, Entity> m_tagToEntity;
		std::map<Entity, std::string> m_entityToTag;

		void loadScene(std::string& sceneFileContent);

		bool m_runSimulationInThread;

		void updateCustomShapesShader(WeirdRenderer::Shader& shader);

		std::vector<WeirdRenderer::Light> m_lights;

		static void handleCollision(CollisionEvent& event, void* userData);
	};
}