#include "weird-renderer/Shader.h"

#include <regex>
#include <sys/stat.h>
#include <sys/types.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// Constructor that build the Shader Program from 2 different shaders
		Shader::Shader(const char* vertexFile, const char* fragmentFile)
		{
			m_vertexFile = vertexFile;
			m_fragmentFile = fragmentFile;

			std::string source = get_file_contents(fragmentFile);

			const std::filesystem::path baseDir = std::filesystem::path(fragmentFile).parent_path();
			static const std::regex includeRegex("#include\\s+\"([^\"]+)\"");

			std::smatch match;
			std::string src = source;
			auto searchStart = src.cbegin();

			while (std::regex_search(searchStart, src.cend(), match, includeRegex))
			{
				std::string includeFile = match[1].str();
				std::filesystem::path includePath = baseDir / includeFile;

				// Ignore procedural includes, those that are generated at runtime instead of loaded from a file
				if (includePath.has_extension())
				{
					m_includedFragmentContents.push_back(get_file_contents(includePath.string().c_str()));
				}
				else
				{
					m_includedFragmentContents.push_back("// includeFile");
				}

				searchStart = match.suffix().first;
			}

			recompile();
		}

		static bool isFileModified(const char* filename, time_t& lastModifiedTime)
		{

			struct stat result;
			if (stat(filename, &result) == 0)
			{
				if (lastModifiedTime != result.st_mtime)
				{
					lastModifiedTime = result.st_mtime;
					return true;
				}
			}
			return false;
		}

		// Activates the Shader Program
		void Shader::use()
		{
			if (isFileModified(m_fragmentFile, m_lastModifiedTime))
			{
				m_needsRecompile = true;
			}

			if (m_needsRecompile)
			{
				m_needsRecompile = false;
				recompile();
			}

			glUseProgram(ID);
		}

		// Deletes the Shader Program
		void Shader::free()
		{
			glDeleteProgram(ID);
		}

		std::string Shader::getVertexCode()
		{
			std::string v = get_file_contents(m_vertexFile);
			return v;
		}

		std::string Shader::getFragmentCode()
		{
			std::string code = get_file_contents(m_fragmentFile);

			return code;
		}

		void Shader::setFragmentIncludeCode(int i, std::string& code)
		{
			m_includedFragmentContents[i] = code;
			recompile();
		}

		void Shader::addDefine(const std::string& name)
		{
			for (const auto& define : m_activeDefines)
			{
				if (define == name)
					return;
			}
			m_activeDefines.push_back(name);
			m_needsRecompile = true;
		}

		void Shader::removeDefine(const std::string& name)
		{
			auto it = std::remove(m_activeDefines.begin(), m_activeDefines.end(), name);
			if (it != m_activeDefines.end())
			{
				m_activeDefines.erase(it, m_activeDefines.end());
			}
			m_needsRecompile = true;
		}

		void Shader::toggleDefine(const std::string& name)
		{
			for (const auto& define : m_activeDefines)
			{
				if (define == name)
				{
					removeDefine(name);
					return;
				}
			}

			m_activeDefines.push_back(name);
			m_needsRecompile = true;
		}

		GLint Shader::getUniformLocation(const std::string& name) const
		{
			// Check if we already found this location
			auto it = m_uniformLocationCache.find(name);
			if (it != m_uniformLocationCache.end())
			{
				return it->second;
			}

			// If not found, ask OpenGL
			GLint location = glGetUniformLocation(ID, name.c_str());

			// Cache it (even if it's -1, so we don't keep asking for invalid names)
			m_uniformLocationCache[name] = location;
			return location;
		}

		void Shader::recompile()
		{
			auto root = fs::current_path().string(); // TODO

			std::string v = getVertexCode();
			std::string f = getFragmentCode();

			// Read vertexFile and fragmentFile and store the strings
			recompile(v, f);
		}

		void Shader::recompile(std::string& vertexCode, std::string& fragmentCode)
		{
			if (ID != -1)
				free();

			// Add includes
			static const std::regex includeRegex("#include\\s+\"([^\"]+)\"");
			std::string fragmentCodeAfterIncludes;
			fragmentCodeAfterIncludes.reserve(fragmentCode.size() + m_includedFragmentContents.size() * 200);

			std::sregex_iterator it(fragmentCode.begin(), fragmentCode.end(), includeRegex);
			std::sregex_iterator end;

			size_t includeIndex = 0;
			size_t lastPos = 0;

			// 1. Handle the first line (e.g. #version) so defines come AFTER it
			size_t firstLineEnd = fragmentCode.find('\n');

			if (firstLineEnd != std::string::npos)
			{
				firstLineEnd += 1; // Include the newline character
				// Append the first line to the buffer
				fragmentCodeAfterIncludes.append(fragmentCode.substr(lastPos, firstLineEnd));
				// Update lastPos so the include loop knows we've already processed this chunk
				lastPos = firstLineEnd;
			}

			// 2. Insert Defines
			for (auto& define : m_activeDefines)
			{
				fragmentCodeAfterIncludes.append("#define ");
				fragmentCodeAfterIncludes.append(define);
				fragmentCodeAfterIncludes.append("\n");
			}

			// 3. Process Includes
			for (; it != end && includeIndex < m_includedFragmentContents.size(); ++it, ++includeIndex)
			{
				// Safety Check: If an include matches on the first line (which we already copied),
				// skip it to avoid string index underflows.
				if (static_cast<size_t>(it->position()) < lastPos)
				{
					continue;
				}

				fragmentCodeAfterIncludes.append(fragmentCode.substr(lastPos, it->position() - lastPos));
				fragmentCodeAfterIncludes.append(m_includedFragmentContents[includeIndex]);
				lastPos = it->position() + it->length();
			}

			fragmentCodeAfterIncludes.append(fragmentCode.substr(lastPos));

			// Convert the shader source strings into character arrays
			const char* vertexSource = vertexCode.c_str();
			const char* fragmentSource = fragmentCodeAfterIncludes.c_str();

			// Create Vertex Shader Object and get its reference
			GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
			// Attach Vertex Shader source to the Vertex Shader Object
			glShaderSource(vertexShader, 1, &vertexSource, NULL);
			// Compile the Vertex Shader into machine code
			glCompileShader(vertexShader);
			// Checks if Shader compiled succesfully
			compileErrors(vertexShader, "VERTEX");

			// const char* fragmentSource = InsertDependencies(fragmentSource);

			// Create Fragment Shader Object and get its reference
			GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			// Attach Fragment Shader source to the Fragment Shader Object
			glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
			// Compile the Fragment Shader into machine code
			glCompileShader(fragmentShader);
			// Checks if Shader compiled succesfully
			compileErrors(fragmentShader, "FRAGMENT");

			// Create Shader Program Object and get its reference
			ID = glCreateProgram();
			// Attach the Vertex and Fragment Shaders to the Shader Program
			glAttachShader(ID, vertexShader);
			glAttachShader(ID, fragmentShader);
			// Wrap-up/Link all the shaders together into the Shader Program
			glLinkProgram(ID);
			// Checks if Shaders linked succesfully
			compileErrors(ID, "PROGRAM");

			// Delete the now useless Vertex and Fragment Shader objects
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);

			m_uniformLocationCache.clear();
		}

		// Checks if the different Shaders have compiled properly
		void Shader::compileErrors(unsigned int shader, const std::string& type)
		{
			// Stores status of compilation
			GLint hasCompiled;
			// Character array to store error message in
			char infoLog[1024];
			if (type != "PROGRAM")
			{
				glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
				if (hasCompiled == GL_FALSE)
				{
					std::cout << "Compiling Shader Program:" << "\n	VS: " << m_vertexFile
							  << "\n	FS: " << m_fragmentFile << std::endl;
					glGetShaderInfoLog(shader, 1024, NULL, infoLog);
					std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
				}
			}
			else
			{
				glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
				if (hasCompiled == GL_FALSE)
				{
					std::cout << "Compiling Shader Program:" << "\n	VS: " << m_vertexFile
							  << "\n	FS: " << m_fragmentFile << std::endl;
					glGetProgramInfoLog(shader, 1024, NULL, infoLog);
					std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
				}
			}
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine