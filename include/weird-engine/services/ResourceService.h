#pragma once

#include <filesystem>
#include <string>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/ResourceManager.h"
#include "weird-engine/Utils.h"

namespace WeirdEngine
{
	class ResourceService
	{
	public:
		ResourceService(ResourceManager& resourceManager, std::string assetsBasePath = "")
			: m_resourceManager(resourceManager)
			, m_assetsBasePath(std::move(assetsBasePath))
		{
		}

		ResourceManager& resources()
		{
			return m_resourceManager;
		}

		void setAssetsBasePath(const std::string& path)
		{
			m_assetsBasePath = path;
		}

		std::string assetPath(const std::string& relative) const
		{
			return m_assetsBasePath + relative;
		}

		MeshID getMeshId(const std::string& path, Entity entity, bool instancing = false)
		{
			return m_resourceManager.getMeshId(assetPath(path).c_str(), entity, instancing);
		}

		std::string readTextFile(const std::string& path) const
		{
			return get_file_contents(path.c_str());
		}

		void writeTextFile(const std::string& path, const std::string& content) const
		{
			saveToFile(path.c_str(), content);
		}

		bool fileExists(const std::string& path) const
		{
			return checkIfFileExists(path.c_str());
		}

		void ensureDirectory(const std::string& path) const
		{
			if (!std::filesystem::exists(path))
			{
				std::filesystem::create_directory(path);
			}
		}

	private:
		ResourceManager& m_resourceManager;
		std::string m_assetsBasePath;
	};
} // namespace WeirdEngine
