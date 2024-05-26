#include"Shader.h"

// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char* filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

// Constructor that build the Shader Program from 2 different shaders
Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	m_vertexFile = vertexFile;
	m_fragmentFile = fragmentFile;

	
	Recompile();
}

static bool isFileModified(const char* filename, time_t& lastModifiedTime) {
	struct stat result;
	if (stat(filename, &result) == 0) {
		if (lastModifiedTime != result.st_mtime) {
			lastModifiedTime = result.st_mtime;
			return true;
		}
	}
	return false;
}

// Activates the Shader Program
void Shader::Activate()
{
	if (isFileModified(m_fragmentFile, m_lastModifiedTime)) {
		Recompile();
	}

	glUseProgram(ID);
}

// Deletes the Shader Program
void Shader::Delete()
{
	glDeleteProgram(ID);
}

void Shader::Recompile()
{
	if (ID != NULL)
		Delete();

	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(m_vertexFile);
	std::string fragmentCode = get_file_contents(m_fragmentFile);

	/*std::string INCLUDE = "#include";
	size_t pos;
	while (true) {

		pos = fragmentCode.find(INCLUDE);

		if (pos == std::string::npos)
			break;

		//TODO: find if INCLUDE is in a comment

		// Length of the substring to be replaced
		size_t len = INCLUDE.length() + 1; // Length of "brown"

		std::string fileName;
		size_t i = pos + len;
		while (true)
		{
			char c = fragmentCode[i];

			if (c == ' ' || c == '\r' || c == '\n')
				break;

			fileName += fragmentCode[i];
			len++;
			i++;
		}

		len += 2;

		std::string filePath = "src/shaders/" + fileName;

		// Try to read included code
		std::string includedCode;
		try {
			includedCode = get_file_contents(filePath.c_str());
		}
		catch (...) {
			break;
		}

		// Replace the substring starting at position 'pos' with the new substring
		fragmentCode.replace(pos, len, includedCode);

		break;
	}*/


	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(vertexShader);
	// Checks if Shader compiled succesfully
	compileErrors(vertexShader, "VERTEX");


	//const char* fragmentSource = InsertDependencies(fragmentSource);

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	// Attach Fragment Shader source to the Fragment Shader Object
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	// Compile the Vertex Shader into machine code
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
}

// Checks if the different Shaders have compiled properly
void Shader::compileErrors(unsigned int shader, const char* type)
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
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
}