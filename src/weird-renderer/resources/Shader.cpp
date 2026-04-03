#include "weird-renderer/resources/Shader.h"

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

			// Use string_view for read-only scanning (Zero-copy)
			std::string_view codeView(fragmentCode);

			// Estimate size: Original Code + (Number of includes * Average include size ~2KB)
			// Adjust the multiplier based on your typical shader include size.
			std::string fragmentCodeAfterIncludes;
			fragmentCodeAfterIncludes.reserve(fragmentCode.size() + m_includedFragmentContents.size() * 2048);

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

			// 3. Process Includes (Manual parsing replaces Regex)
			size_t includeIndex = 0;
			const size_t numIncludes = m_includedFragmentContents.size();

			// Start searching from lastPos (after #version) so we don't need the safety check
			while (includeIndex < numIncludes)
			{
				// Find "#include"
				size_t incStart = codeView.find("#include", lastPos);

				if (incStart == std::string_view::npos)
					break;

				// Verify format: #include\s+"..."
				// 1. Check for whitespace after #include
				size_t cursor = incStart + 8; // Length of "#include"

				// Scan past whitespace
				bool hasWhitespace = false;
				while (cursor < codeView.size() && (codeView[cursor] == ' ' || codeView[cursor] == '\t'))
				{
					hasWhitespace = true;
					cursor++;
				}

				// Regex equivalent of \s+: Must have at least one space/tab
				if (!hasWhitespace)
				{
					// False positive (e.g. "#include_something"), advance lastPos slightly and retry
					// We append the text up to here to ensure we don't skip it in the final output,
					// but since we haven't substituted, we just move the search start.
					// Actually, for simplicity, we just continue searching from incStart + 1
					// (This part of logic is rarely hit in valid shaders)
					size_t nextSearch = incStart + 1;
					// Don't update lastPos or append yet
					// A tricky case without regex, but assuming valid shader code:
					// We force the loop to search again from next char, essentially ignoring this hit.
					// To do this efficiently, we'd need to modify the find call.
					// For this snippet, we will assume if it finds "#include" it's intended.
					// But strictly adhering to regex logic:
					// If checks fail, we manually look for the next one inside the loop logic.
					// Let's implement the 'continue' logic by adjusting the search offset for the next iteration
					// without appending anything yet.

					// *However*, to keep the code linear and simple:
					// If syntax is wrong, treat it as raw text.
					// We only "consume" it if it matches perfectly.
					// Since we are iterating via `find`, we just need to advance `lastPos` past this point ONLY if we
					// replace. If we don't replace, we leave `lastPos` alone, but we need to tell `find` to skip this.
					// Optimization: Just allow loose matching or strict? Let's stay strict.

					// Simple fix: if invalid, we create a temp search offset.
					// Since this manual parsing is complex to inline, let's assume valid syntax
					// OR simply ensure the quote exists.
				}

				// 2. Check for opening quote
				if (cursor >= codeView.size() || codeView[cursor] != '"')
				{
					// Not a match (e.g. <vector> or malformed), skip this #include token
					// We rely on the next loop iteration to find the next one,
					// but we must advance the search start pos manually.
					// Since we can't easily jump back in this structure, let's just
					// assume it wasn't a match and append everything later?
					// No, we need to find the NEXT valid one.

					// Simpler approach: Look for the next one immediately
					size_t nextInc = codeView.find("#include", incStart + 1);
					if (nextInc != std::string_view::npos)
					{
						// Hacky way to skip current iteration logic without goto
						// Ideally we restructure, but let's just break/continue with a specialized search offset
						// variable For performance in 99% of cases, valid shaders work.
					}
					// If we hit here, we skip replacing this instance.
					// We treat it as part of the normal string.
					// To handle this correctly: We update a "searchOffset" separate from "lastPos".
					// To keep this readable:
				}
				else
				{
					// 3. Find closing quote
					size_t quoteStart = cursor;
					size_t quoteEnd = codeView.find('"', quoteStart + 1);

					if (quoteEnd != std::string_view::npos)
					{
						// Valid Match Found!

						// Append everything before this #include
						fragmentCodeAfterIncludes.append(codeView.substr(lastPos, incStart - lastPos));

						// Append the replacement content
						fragmentCodeAfterIncludes.append(m_includedFragmentContents[includeIndex]);

						// Advance index
						includeIndex++;

						// Move processed cursor to after the closing quote
						lastPos = quoteEnd + 1;

						// Continue loop to find next
						continue;
					}
				}

				// If we reached here, the #include was malformed or not a string include.
				// We must advance the search to avoid infinite loop, but NOT advance lastPos
				// because we haven't appended the text yet.
				// However, finding a specific way to skip just this occurrence while using 'lastPos'
				// as both append-cursor and search-cursor is hard.
				//
				// Revised loop logic to handle skipping invalid includes gracefully:
				// We break the loop here because `find` takes `lastPos`.
				// If we have an invalid include, we must effectively "eat" it into the buffer
				// or use a separate search cursor.

				// Simplest robust solution: Use a separate search_offset.
				break; // (See logic below for the robust implementation)
			}

			// Code cleanup: The loop above is slightly messy due to error handling logic.
			// Here is the CLEAN manual parsing loop replacing step 3:

			// 3. Process Includes (Clean Version)
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

			// Convert the shader source strings into character arrays
			const char* vertexSource = vertexCode.c_str();
			const char* fragmentSource = fragmentCodeAfterIncludes.c_str();

#if !defined(NDEBUG) && defined(LOG_SHADER_COMPILATION)
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

#if !defined(NDEBUG) && defined(LOG_SHADER_COMPILATION)
			auto endTime = std::chrono::high_resolution_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
			std::cout << "Compiling program: \n   V -> " << m_vertexFile << "\n   F -> " << m_fragmentFile << "\n";
			for (const auto& define : m_activeDefines)
			{
				std::cout << define << "\n";
			}
			std::cout << "Elapsed time:" << ms << " ms \n\n";
#endif

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