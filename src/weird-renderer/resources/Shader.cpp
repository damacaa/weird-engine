#include "weird-renderer/resources/Shader.h"

#include "weird-engine/Logger.h"
#include <fstream>
#include <regex>
#include <sys/stat.h>
#include <sys/types.h>

// #define LOG_SHADER_COMPILATION

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
			m_hasRecompiled = false;

			if (isFileModified(m_fragmentFile, m_lastModifiedTime))
			{
				m_needsRecompile = true;
			}

			if (m_needsRecompile && !m_isCompilingAsync)
			{
				m_needsRecompile = false;
				recompile();
				m_hasRecompiled = true;
			}

			if (ID != 0 && ID != (GLuint)-1)
			{
				glUseProgram(ID);
			}
		}

		// Deletes the Shader Program
		void Shader::free()
		{
			if (m_isCompilingAsync)
			{
				if (m_pendingVertexShader != 0)
				{
					glDeleteShader(m_pendingVertexShader);
					m_pendingVertexShader = 0;
				}
				if (m_pendingFragmentShader != 0)
				{
					glDeleteShader(m_pendingFragmentShader);
					m_pendingFragmentShader = 0;
				}
				if (m_pendingProgram != 0)
				{
					glDeleteProgram(m_pendingProgram);
					m_pendingProgram = 0;
				}
				m_isCompilingAsync = false;
			}

			if (ID != 0 && ID != (GLuint)-1)
			{
				glDeleteProgram(ID);
				ID = -1;
			}
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

		void Shader::setFragmentIncludeCode(int i, const std::string& code, bool shouldRecompile)
		{
			m_includedFragmentContents[i] = code;
			if (shouldRecompile)
			{
				recompile();
			}
		}

		bool Shader::setFragmentIncludeCodeAsync(int i, const std::string& code)
		{
			m_includedFragmentContents[i] = code;
			std::string v = getVertexCode();
			std::string f = getFragmentCode();
			return recompileAsync(v, f);
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

		std::string Shader::buildFragmentSource(std::string_view codeView)
		{
			// Estimate size: Original Code + (Number of includes * Average include size ~2KB)
			// Adjust the multiplier based on your typical shader include size.
			std::string fragmentCodeAfterIncludes;
			fragmentCodeAfterIncludes.reserve(codeView.size() + m_includedFragmentContents.size() * 2048);

			size_t lastPos = 0;

			// 1. Handle the first line (e.g. #version)
			// We scan for the first newline.
			size_t firstLineEnd = codeView.find('\n');

			if (firstLineEnd != std::string_view::npos)
			{
				firstLineEnd += 1; // Include the newline character
				fragmentCodeAfterIncludes.append(codeView.substr(0, firstLineEnd));
				lastPos = firstLineEnd;
			}

			// 2. Insert Defines
			// Direct append is faster than string concatenation
			for (const auto& define : m_activeDefines)
			{
				fragmentCodeAfterIncludes.append("#define ");
				fragmentCodeAfterIncludes.append(define);
				fragmentCodeAfterIncludes.append("\n");
			}

			// 3. Process Includes
			size_t includeIndex = 0;
			const size_t numIncludes = m_includedFragmentContents.size();
			size_t searchPos = lastPos; // Cursor for searching
			while (includeIndex < numIncludes)
			{
				size_t incStart = codeView.find("#include", searchPos);
				if (incStart == std::string_view::npos)
					break;

				// Check syntax: #include "..."
				size_t cursor = incStart + 8;
				bool valid = false;
				size_t quoteEnd = std::string_view::npos;

				// Whitespace check
				if (cursor < codeView.size() && std::isspace(codeView[cursor]))
				{
					while (cursor < codeView.size() && std::isspace(codeView[cursor]))
						cursor++;

					// Quote check
					if (cursor < codeView.size() && codeView[cursor] == '"')
					{
						quoteEnd = codeView.find('"', cursor + 1);
						if (quoteEnd != std::string_view::npos)
						{
							valid = true;
						}
					}
				}

				if (valid)
				{
					// Append text from last processed position up to the start of #include
					fragmentCodeAfterIncludes.append(codeView.substr(lastPos, incStart - lastPos));
					// Append new content
					fragmentCodeAfterIncludes.append(m_includedFragmentContents[includeIndex]);
					includeIndex++;
					// Advance
					lastPos = quoteEnd + 1;
					searchPos = lastPos;
				}
				else
				{
					// It looked like an include but wasn't (e.g. inside a comment or malformed).
					// Skip over the "#include" text in the search, but don't append yet.
					searchPos = incStart + 1;
				}
			}

			// Append remaining code
			fragmentCodeAfterIncludes.append(codeView.substr(lastPos));

			m_lastCompleteFragmentCode = fragmentCodeAfterIncludes;

#if defined(WEIRD_DEBUG) && defined(LOG_SHADER_COMPILATION)
			if (m_fragmentFile && std::string(m_fragmentFile).find("sdf_") != std::string::npos)
			{
				bool isUI = false;
				for (const auto& d : m_activeDefines)
				{
					if (d == "UI_PIPELINE")
						isUI = true;
				}
				std::string filename = isUI ? "ui_frag_dump.glsl" : "world_frag_dump.glsl";
				std::ofstream dumpFile(filename);
				if (dumpFile.is_open())
				{
					dumpFile << fragmentCodeAfterIncludes;
					dumpFile.close();
				}
			}
#endif

			return fragmentCodeAfterIncludes;
		}

		void Shader::recompile(std::string& vertexCode, std::string& fragmentCode)
		{
			if (m_isCompilingAsync)
			{
				if (m_pendingVertexShader != 0)
				{
					glDeleteShader(m_pendingVertexShader);
					m_pendingVertexShader = 0;
				}
				if (m_pendingFragmentShader != 0)
				{
					glDeleteShader(m_pendingFragmentShader);
					m_pendingFragmentShader = 0;
				}
				if (m_pendingProgram != 0)
				{
					glDeleteProgram(m_pendingProgram);
					m_pendingProgram = 0;
				}
				m_isCompilingAsync = false;
			}

			if (ID != 0 && ID != (GLuint)-1)
				free();

			std::string fragmentCodeAfterIncludes = buildFragmentSource(fragmentCode);

			// Convert the shader source strings into character arrays
			const char* vertexSource = vertexCode.c_str();
			const char* fragmentSource = fragmentCodeAfterIncludes.c_str();

#if defined(WEIRD_DEBUG) && defined(LOG_SHADER_COMPILATION)
			auto startTime = std::chrono::high_resolution_clock::now();
#endif

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

#if defined(WEIRD_DEBUG) && defined(LOG_SHADER_COMPILATION)
			auto endTime = std::chrono::high_resolution_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

			std::string logMsg =
				std::string("Compiling program: \n   V -> ") + m_vertexFile + "\n   F -> " + m_fragmentFile + "\n";
			for (const auto& define : m_activeDefines)
			{
				logMsg += define + "\n";
			}
			logMsg += "Elapsed time:" + std::to_string(ms) + " ms \n\n";
			WeirdEngine::Logger::log(logMsg);
#endif

			m_uniformLocationCache.clear();
			m_hasRecompiled = true;
		}

		bool Shader::recompileAsync(std::string& vertexCode, std::string& fragmentCode)
		{
#if defined(WEIRD_DISABLE_PARALLEL_SHADER_COMPILE)
			recompile(vertexCode, fragmentCode);
			return false;
#else
			if (!GLAD_GL_KHR_parallel_shader_compile)
			{
				recompile(vertexCode, fragmentCode);
				return false;
			}

			if (m_isCompilingAsync)
			{
				if (m_pendingVertexShader != 0)
				{
					glDeleteShader(m_pendingVertexShader);
					m_pendingVertexShader = 0;
				}
				if (m_pendingFragmentShader != 0)
				{
					glDeleteShader(m_pendingFragmentShader);
					m_pendingFragmentShader = 0;
				}
				if (m_pendingProgram != 0)
				{
					glDeleteProgram(m_pendingProgram);
					m_pendingProgram = 0;
				}
				m_isCompilingAsync = false;
			}

			std::string fragmentCodeAfterIncludes = buildFragmentSource(fragmentCode);

			const char* vertexSource = vertexCode.c_str();
			const char* fragmentSource = fragmentCodeAfterIncludes.c_str();

			auto glCallStartTime = std::chrono::high_resolution_clock::now();
			m_asyncCompileStartTime = glCallStartTime;

			m_pendingVertexShader = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(m_pendingVertexShader, 1, &vertexSource, NULL);
			glCompileShader(m_pendingVertexShader);

			m_pendingFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(m_pendingFragmentShader, 1, &fragmentSource, NULL);
			glCompileShader(m_pendingFragmentShader);

			m_pendingProgram = glCreateProgram();
			glAttachShader(m_pendingProgram, m_pendingVertexShader);
			glAttachShader(m_pendingProgram, m_pendingFragmentShader);
			glLinkProgram(m_pendingProgram);

#if defined(WEIRD_DEBUG) && defined(LOG_SHADER_COMPILATION)
			auto glCallEndTime = std::chrono::high_resolution_clock::now();
			double glCallMs = std::chrono::duration<double, std::milli>(glCallEndTime - glCallStartTime).count();
			WeirdEngine::Logger::log("Shader::recompileAsync GL calls (main thread): " + std::to_string(glCallMs) +
									 " ms (" + m_fragmentFile + ")");
#endif

			m_isCompilingAsync = true;
			return true;
#endif
		}

		bool Shader::pollAsyncCompile()
		{
			if (!m_isCompilingAsync)
			{
				return false;
			}

#if !defined(WEIRD_DISABLE_PARALLEL_SHADER_COMPILE)
			if (GLAD_GL_KHR_parallel_shader_compile)
			{
				GLint completed = GL_FALSE;
				glGetProgramiv(m_pendingProgram, GL_COMPLETION_STATUS_KHR, &completed);
				if (completed == GL_FALSE)
				{
					return false;
				}
			}
#endif

			compileErrors(m_pendingVertexShader, "VERTEX");
			compileErrors(m_pendingFragmentShader, "FRAGMENT");
			compileErrors(m_pendingProgram, "PROGRAM");

			GLint linked = GL_FALSE;
			glGetProgramiv(m_pendingProgram, GL_LINK_STATUS, &linked);

			glDetachShader(m_pendingProgram, m_pendingVertexShader);
			glDetachShader(m_pendingProgram, m_pendingFragmentShader);
			glDeleteShader(m_pendingVertexShader);
			glDeleteShader(m_pendingFragmentShader);
			m_pendingVertexShader = 0;
			m_pendingFragmentShader = 0;

			if (linked == GL_TRUE)
			{
				auto endTime = std::chrono::high_resolution_clock::now();
				double totalBgMs = std::chrono::duration<double, std::milli>(endTime - m_asyncCompileStartTime).count();

				if (ID != 0 && ID != (GLuint)-1)
				{
					glDeleteProgram(ID);
				}
				ID = m_pendingProgram;
				m_pendingProgram = 0;
				m_isCompilingAsync = false;
				m_uniformLocationCache.clear();
				m_hasRecompiled = true;

				glUseProgram(ID);

#if defined(WEIRD_DEBUG) && defined(LOG_SHADER_COMPILATION)
				WeirdEngine::Logger::log("Async shader background compile ready: " + std::to_string(totalBgMs) +
										 " ms (" + m_fragmentFile + ")");
#endif

				return true;
			}
			else
			{
				std::string errorMsg = std::string("Async shader compilation/linking failed:\n   V -> ") +
									   m_vertexFile + "\n   F -> " + m_fragmentFile + "\n";
				for (const auto& define : m_activeDefines)
				{
					errorMsg += "   Define: " + define + "\n";
				}
				WeirdEngine::Logger::error(errorMsg);

				glDeleteProgram(m_pendingProgram);
				m_pendingProgram = 0;
				m_isCompilingAsync = false;
				return false;
			}
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
					std::string logMsg =
						std::string("Compiling Shader Program:\n	VS: ") + m_vertexFile + "\n	FS: " + m_fragmentFile;
					glGetShaderInfoLog(shader, 1024, NULL, infoLog);
					WeirdEngine::Logger::error(logMsg + "\nSHADER_COMPILATION_ERROR for:" + type + "\n" + infoLog);
				}
			}
			else
			{
				glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
				if (hasCompiled == GL_FALSE)
				{
					std::string logMsg =
						std::string("Compiling Shader Program:\n	VS: ") + m_vertexFile + "\n	FS: " + m_fragmentFile;
					glGetProgramInfoLog(shader, 1024, NULL, infoLog);
					WeirdEngine::Logger::error(logMsg + "\nSHADER_LINKING_ERROR for:" + type + "\n" + infoLog);
				}
			}
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine