#ifndef VEC_H
#define VEC_H

#include <glm/glm.hpp>

namespace WeirdEngine
{
	using vec2 = glm::vec2;
	using vec3 = glm::vec3;

	struct vec4 : public glm::vec4
	{
		vec4()
			: glm::vec4(0.0f)
		{
		}
		vec4(float s)
			: glm::vec4(s)
		{
		}
		vec4(float x, float y, float z, float w)
			: glm::vec4(x, y, z, w)
		{
		}
		vec4(const glm::vec4& v)
			: glm::vec4(v)
		{
		}
		vec4(const glm::vec3& v, float a = 1.0f)
			: glm::vec4(v.x, v.y, v.z, a)
		{
		}
		vec4(const glm::vec2& v, float z, float w)
			: glm::vec4(v.x, v.y, z, w)
		{
		}
		vec4(float x, const glm::vec3& v)
			: glm::vec4(x, v.x, v.y, v.z)
		{
		}
		vec4(const glm::vec2& v1, const glm::vec2& v2)
			: glm::vec4(v1.x, v1.y, v2.x, v2.y)
		{
		}

		vec4& operator=(const glm::vec4& v)
		{
			glm::vec4::operator=(v);
			return *this;
		}

		vec4& operator=(const glm::vec3& v)
		{
			this->x = v.x;
			this->y = v.y;
			this->z = v.z;
			this->w = 1.0f;
			return *this;
		}
	};
} // namespace WeirdEngine

#endif // VEC_H
