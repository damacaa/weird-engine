#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "weird-engine/Utils.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		using ShaderID = std::uint32_t;

		class Shader
		{
		public:
			// Reference ID of the Shader Program
			GLuint ID = -1;
			// Constructor that build the Shader Program from 2 different shaders
			Shader(const char* vertexFile, const char* fragmentFile);
			Shader() {};

			// Activates the Shader Program
			void use();
			// Deletes the Shader Program
			void free();

			std::string getVertexCode();
			std::string getFragmentCode();
			void setFragmentIncludeCode(int i, std::string& code);
			void addDefine(const std::string& name);
			void removeDefine(const std::string& name);
			void toggleDefine(const std::string& name);

			GLint getUniformLocation(const std::string& name) const;

			// Utility uniform functions
			void setUniform(const std::string& name, float value) const
			{
				glUniform1f(getUniformLocation(name), value);
			}

			void setUniform(const std::string& name, double value) const
			{
				glUniform1f(getUniformLocation(name), value);
			}

			void setUniform(const std::string& name, int value) const
			{
				glUniform1i(getUniformLocation(name), value);
			}

			void setUniform(const std::string& name, const glm::vec2& value) const
			{
				glUniform2fv(getUniformLocation(name), 1, &value[0]);
			}

			void setUniform(const std::string& name, const glm::vec3& value) const
			{
				glUniform3fv(getUniformLocation(name), 1, &value[0]);
			}

			void setUniform(const std::string& name, const glm::vec3* value, unsigned int size) const
			{
				glUniform3fv(getUniformLocation(name), size, &value[0].x);
			}

			void setUniform(const std::string& name, const glm::vec4* value, unsigned int size) const
			{
				glUniform4fv(getUniformLocation(name), size, &value[0].x);
			}

			void setUniform(const std::string& name, const glm::vec4& value) const
			{
				glUniform4fv(getUniformLocation(name), 1, &value[0]);
			}

			void setUniform(const std::string& name, const glm::mat4& value) const
			{
				glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
			}

			void setUniform(const std::string& name, const glm::mat4* value, unsigned int size = 1) const
			{
				glUniformMatrix4fv(getUniformLocation(name), size, GL_FALSE, &value[0][0][0]);
			}

		private:
			const char* m_vertexFile;
			const char* m_fragmentFile;
			time_t m_lastModifiedTime;

			bool m_needsRecompile = false;

			void recompile();
			void recompile(std::string& vertexCode, std::string& fragmentCode);

			// Checks if the different Shaders have compiled properly
			void compileErrors(unsigned int shader, const std::string& type);

			std::vector<std::string> m_includedFragmentContents;
			std::vector<std::string> m_activeDefines;

			// This MUST be mutable because setUniform is const, but we need to update the cache
			mutable std::unordered_map<std::string, GLint> m_uniformLocationCache;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif