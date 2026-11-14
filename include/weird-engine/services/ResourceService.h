#pragma once

#include <algorithm>
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

		const std::string& getAssetsBasePath() const
		{
			return m_assetsBasePath;
		}

		std::string assetPath(const std::string& relative) const
		{
			return m_assetsBasePath + relative;
		}

		// Resolves relative, context-sibling, or legacy machine-specific asset paths.
		std::string resolveAssetPath(const std::string& rawPath, const std::string& contextFilePath = "") const
		{
			if (rawPath.empty())
			{
				return {};
			}

			std::string clean = rawPath;
			std::replace(clean.begin(), clean.end(), '\\', '/');

			std::error_code ec;
			if (std::filesystem::exists(rawPath, ec))
			{
				return rawPath;
			}
			if (std::filesystem::exists(clean, ec))
			{
				return clean;
			}

			std::filesystem::path p(clean);
			if (!contextFilePath.empty())
			{
				std::filesystem::path nextToContext = std::filesystem::path(contextFilePath).parent_path() / p;
				if (std::filesystem::exists(nextToContext, ec))
				{
					return nextToContext.lexically_normal().generic_string();
				}
			}

			if (!m_assetsBasePath.empty())
			{
				std::filesystem::path fromAssets(assetPath(clean));
				if (std::filesystem::exists(fromAssets, ec))
				{
					return fromAssets.lexically_normal().generic_string();
				}

				auto assetsPos = clean.find("assets/");
				if (assetsPos != std::string::npos)
				{
					std::string relAfterAssets = clean.substr(assetsPos + 7);
					std::filesystem::path fromRel(assetPath(relAfterAssets));
					if (std::filesystem::exists(fromRel, ec))
					{
						return fromRel.lexically_normal().generic_string();
					}
				}

				std::string filename = p.filename().string();
				if (!filename.empty())
				{
					std::filesystem::path byName(assetPath(filename));
					if (std::filesystem::exists(byName, ec))
					{
						return byName.lexically_normal().generic_string();
					}
				}
			}

			return rawPath;
		}

		// Converts an asset path to a relative path under the asset root or scene directory when possible.
		std::string makePortableAssetPath(const std::string& rawPath, const std::string& contextFilePath = "") const
		{
			if (rawPath.empty())
			{
				return {};
			}

			std::error_code ec;
			std::filesystem::path resolved(resolveAssetPath(rawPath, contextFilePath));
			std::filesystem::path absPath = std::filesystem::weakly_canonical(resolved, ec);
			if (ec)
			{
				absPath = resolved.lexically_normal();
			}

			auto tryRelativeUnder = [&](const std::filesystem::path& baseDir, std::string& outRel) -> bool
			{
				if (baseDir.empty())
				{
					return false;
				}
				std::filesystem::path canonBase = std::filesystem::weakly_canonical(baseDir, ec);
				if (ec)
				{
					canonBase = baseDir.lexically_normal();
				}
				std::filesystem::path rel = absPath.lexically_relative(canonBase);
				if (!rel.empty() && rel.native() != std::filesystem::path(".").native() && *rel.begin() != "..")
				{
					outRel = rel.lexically_normal().generic_string();
					return true;
				}
				return false;
			};

			std::string rel;
			if (tryRelativeUnder(std::filesystem::path(m_assetsBasePath), rel))
			{
				return rel;
			}
			if (!contextFilePath.empty())
			{
				if (tryRelativeUnder(std::filesystem::path(contextFilePath).parent_path(), rel))
				{
					return rel;
				}
			}

			std::string normalized = resolved.generic_string();
			std::replace(normalized.begin(), normalized.end(), '\\', '/');
			return normalized;
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
