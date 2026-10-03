#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "weird-engine/Material2D.h"

namespace WeirdEngine
{
	class Material2DService
	{
	public:
		Material2DService(Material2D (&materials)[16], uint16_t& count,
						  std::unordered_map<std::string, uint16_t>& nameToId)
			: m_materials(materials)
			, m_count(count)
			, m_nameToId(nameToId)
		{
		}

		Material2D& createMaterial(const std::string& name = "")
		{
			if (!name.empty())
			{
				auto it = m_nameToId.find(name);
				if (it != m_nameToId.end())
				{
					return m_materials[it->second];
				}
			}

			// If slots below 16 are available, use them, otherwise use the last slot
			uint16_t slot = m_count < 16 ? m_count++ : 15;
			Material2D& mat = m_materials[slot];
			mat.id = slot;
			mat.name = name;
			if (!name.empty())
			{
				m_nameToId[name] = slot;
			}
			return mat;
		}

		Material2D& getOrCreate(const std::string& name, std::function<void(Material2D&)> init = nullptr)
		{
			auto it = m_nameToId.find(name);
			if (it != m_nameToId.end())
			{
				return m_materials[it->second];
			}
			Material2D& mat = createMaterial(name);
			if (init)
			{
				init(mat);
			}
			return mat;
		}

		Material2D& get(const std::string& name)
		{
			auto it = m_nameToId.find(name);
			if (it != m_nameToId.end())
			{
				return m_materials[it->second];
			}
			return m_materials[0];
		}

		const Material2D& get(const std::string& name) const
		{
			auto it = m_nameToId.find(name);
			if (it != m_nameToId.end())
			{
				return m_materials[it->second];
			}
			return m_materials[0];
		}

		Material2DHandle getHandle(const std::string& name) const
		{
			auto it = m_nameToId.find(name);
			if (it != m_nameToId.end())
			{
				return Material2DHandle(it->second);
			}
			return Material2DHandle(0);
		}

		Material2D& get(uint16_t id)
		{
			return m_materials[id < 16 ? id : 15];
		}

		const Material2D& get(uint16_t id) const
		{
			return m_materials[id < 16 ? id : 15];
		}

		Material2D& getMaterial(int index)
		{
			return get(static_cast<uint16_t>(index));
		}

		const Material2D* getMaterials() const
		{
			return m_materials;
		}

		uint16_t getMaterialCount() const
		{
			return m_count;
		}

		bool has(const std::string& name) const
		{
			return m_nameToId.find(name) != m_nameToId.end();
		}

	private:
		Material2D (&m_materials)[16];
		uint16_t& m_count;
		std::unordered_map<std::string, uint16_t>& m_nameToId;
	};
} // namespace WeirdEngine
