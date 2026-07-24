#pragma once

#include <glad/glad.h>
#include <vector>
#include <weird-engine.h>

namespace WeirdEngine
{
	class WaterPlane
	{
	public:
		WaterPlane() = default;
		~WaterPlane()
		{
			free();
		}

		void build()
		{
			const int verts = GRID_SIZE + 1;
			const float step = GRID_WORLD / static_cast<float>(GRID_SIZE);
			const float half = GRID_WORLD * 0.5f;

			std::vector<WaterVertex> vertices;
			vertices.reserve(static_cast<size_t>(verts * verts));

			for (int z = 0; z < verts; ++z)
			{
				for (int x = 0; x < verts; ++x)
				{
					WaterVertex v{};
					v.position = glm::vec3(x * step - half, 0.0f, z * step - half);
					v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
					v.color = glm::vec3(1.0f);
					v.texCoord = glm::vec2(static_cast<float>(x) / GRID_SIZE, static_cast<float>(z) / GRID_SIZE);
					vertices.push_back(v);
				}
			}

			std::vector<GLuint> indices;
			indices.reserve(static_cast<size_t>(GRID_SIZE * GRID_SIZE * 6));

			for (int z = 0; z < GRID_SIZE; ++z)
			{
				for (int x = 0; x < GRID_SIZE; ++x)
				{
					GLuint tl = static_cast<GLuint>(z * verts + x);
					GLuint tr = static_cast<GLuint>(z * verts + x + 1);
					GLuint bl = static_cast<GLuint>((z + 1) * verts + x);
					GLuint br = static_cast<GLuint>((z + 1) * verts + x + 1);

					indices.push_back(tl);
					indices.push_back(bl);
					indices.push_back(tr);
					indices.push_back(tr);
					indices.push_back(bl);
					indices.push_back(br);
				}
			}

			m_waterIndexCount = static_cast<GLsizei>(indices.size());

			glGenVertexArrays(1, &m_waterVAO);
			glGenBuffers(1, &m_waterVBO);
			glGenBuffers(1, &m_waterEBO);

			glBindVertexArray(m_waterVAO);

			glBindBuffer(GL_ARRAY_BUFFER, m_waterVBO);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(WaterVertex)),
						 vertices.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_waterEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
						 indices.data(), GL_STATIC_DRAW);

			// layout(location = 0) in vec3 in_position
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
								  reinterpret_cast<void*>(offsetof(WaterVertex, position)));

			// layout(location = 1) in vec3 in_normal
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
								  reinterpret_cast<void*>(offsetof(WaterVertex, normal)));

			// layout(location = 2) in vec3 in_color
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
								  reinterpret_cast<void*>(offsetof(WaterVertex, color)));

			// layout(location = 3) in vec2 in_texCoord
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(WaterVertex),
								  reinterpret_cast<void*>(offsetof(WaterVertex, texCoord)));

			glBindVertexArray(0);
		}

		void free()
		{
			if (m_waterVAO)
			{
				glDeleteVertexArrays(1, &m_waterVAO);
				m_waterVAO = 0;
			}
			if (m_waterVBO)
			{
				glDeleteBuffers(1, &m_waterVBO);
				m_waterVBO = 0;
			}
			if (m_waterEBO)
			{
				glDeleteBuffers(1, &m_waterEBO);
				m_waterEBO = 0;
			}
		}

		void draw(WeirdRenderer::Shader& shader, const glm::mat4& model)
		{
			shader.setUniform("u_model", model);

			glBindVertexArray(m_waterVAO);
			glDrawElements(GL_TRIANGLES, m_waterIndexCount, GL_UNSIGNED_INT, nullptr);
			glBindVertexArray(0);
		}

		static glm::vec2 rot2(glm::vec2 p, float a)
		{
			return {p.x * cosf(a) - p.y * sinf(a), p.x * sinf(a) + p.y * cosf(a)};
		}

		static float gerstnerY(glm::vec2 xz, glm::vec2 dir, float wavelength, float amplitude, float t)
		{
			const float k = 6.28318f / wavelength;
			const float c = sqrtf(9.81f / k);
			return amplitude * sinf(k * glm::dot(dir, xz) - c * t);
		}

		float waterHeightAt(glm::vec2 xz, float t) const
		{
			// Domain warp (matches shader)
			glm::vec2 warp = {sinf(xz.x * 0.1731f + xz.y * 0.0893f + t * 0.071f) * 1.6f +
								  cosf(xz.x * 0.2473f - xz.y * 0.1337f + t * 0.043f) * 0.9f,
							  cosf(xz.x * 0.0971f + xz.y * 0.2011f + t * 0.059f) * 1.4f +
								  sinf(xz.x * 0.1619f - xz.y * 0.3001f + t * 0.037f) * 0.7f};
			glm::vec2 xzW = xz + warp;
			glm::vec2 xzR = rot2(xzW, 1.41421356f);

			float y = 0.0f;
			// Cascade A
			y += gerstnerY(xzW, glm::normalize(glm::vec2(1.0f, 0.4f)), 8.09f, 0.110f, t);
			y += gerstnerY(xzW, glm::normalize(glm::vec2(-0.5f, 1.0f)), 5.00f, 0.075f, t);
			y += gerstnerY(xzW, glm::normalize(glm::vec2(0.4f, -1.0f)), 3.09f, 0.040f, t);
			// Cascade B (rotated frame)
			y += gerstnerY(xzR, glm::normalize(glm::vec2(1.0f, 0.3f)), 4.72f, 0.055f, t);
			y += gerstnerY(xzR, glm::normalize(glm::vec2(-0.7f, 1.0f)), 2.62f, 0.030f, t);
			y += gerstnerY(xzR, glm::normalize(glm::vec2(0.9f, -0.6f)), 1.62f, 0.015f, t);
			y += gerstnerY(xzR, glm::normalize(glm::vec2(-1.0f, -0.3f)), 1.00f, 0.008f, t);
			return y;
		}

	private:
		struct WaterVertex
		{
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec3 color;
			glm::vec2 texCoord;
		};

		GLuint m_waterVAO = 0;
		GLuint m_waterVBO = 0;
		GLuint m_waterEBO = 0;
		GLsizei m_waterIndexCount = 0;

		static constexpr int GRID_SIZE = 512;
		static constexpr float GRID_WORLD = 200.0f;
	};
} // namespace WeirdEngine
