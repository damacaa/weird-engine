#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include<glad/glad.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>
#include <glm/glm.hpp>

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
		void activate();
		// Deletes the Shader Program
		void Delete();

		std::string getVertexCode();
		std::string getFragmentCode();
		void setFragmentCode(std::string& code);

		// Utility uniform functions
		void setUniform(const std::string& name, float value) const {
			glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
		}

		void setUniform(const std::string& name, double value) const {
			glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
		}

		void setUniform(const std::string& name, int value) const {
			glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
		}

		void setUniform(const std::string& name, const glm::vec2& value) const {
			glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
		}

		void setUniform(const std::string& name, const glm::vec3& value) const {
			glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
		}

		void setUniform(const std::string& name, const glm::vec3* value, unsigned int size) const {
			glUniform3fv(glGetUniformLocation(ID, name.c_str()), size, &value[0].x);
		}


		void setUniform(const std::string& name, const glm::vec4& value) const {
			glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
		}

		void setUniform(const std::string& name, const glm::mat4& value) const {
			glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &value[0][0]);
		}

		void setUniform(const std::string& name, const glm::mat4* value, unsigned int size = 1) const {
			glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), size, GL_FALSE, &value[0][0][0]);
		}

	private:
		const char* m_vertexFile;
		const char* m_fragmentFile;
		time_t m_lastModifiedTime;

		void recompile();
		void recompile(std::string& vertexCode, std::string& fragmentCode);

		// Checks if the different Shaders have compiled properly
		void compileErrors(unsigned int shader, const char* type);

		
	};
}

#endif