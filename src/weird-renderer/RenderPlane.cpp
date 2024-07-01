#include "RenderPlane.h"



RenderPlane::RenderPlane(int width, int height, Shader& shader, bool shapeRenderer)
{
	float f = 1.0f;
	// Vertices coordinates
	GLfloat vertices[] =
	{
		-f, -f , 0.0f, // Lower left corner
		-f,  f , 0.0f, // Upper corner
		 f,  f , 0.0f, // Lower right corner
		 f, -f , 0.0f // Lower right corner
	};

	// Indices for vertices order
	GLuint indices[] =
	{
		0, 1, 2, // Lower left triangle
		0, 2, 3 // Upper triangle
	};

	// Generate the VAO, VBO, and EBO with only 1 object each
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);


	// Make the VAO the current Vertex Array Object by binding it
	glBindVertexArray(VAO);

	// Bind the VBO specifying it's infinityLoop GL_ARRAY_BUFFER
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// Introduce the vertices into the VBO
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Bind the EBO specifying it's infinityLoop GL_ELEMENT_ARRAY_BUFFER
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	// Introduce the indices into the EBO
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


	// Configure the Vertex Attribute so that OpenGL knows how to read the VBO
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	// Enable the Vertex Attribute so that OpenGL knows to use it
	glEnableVertexAttribArray(0);

	// Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	// Bind the EBO to 0 so that we don't accidentally modify it
	// MAKE SURE TO UNBIND IT AFTER UNBINDING THE VAO, as the EBO is linked in the VAO
	// This does not apply to the VBO because the VBO is already linked to the VAO during glVertexAttribPointer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (shapeRenderer) {

		// Uniform buffer to store shapes
		glGenBuffers(1, &UBO);
		glBindBuffer(GL_UNIFORM_BUFFER, UBO);
		// Bind UBO to shader program (assuming location 0 for UBO)
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}



	// Frame buffer to store normal render and apply ray marching as a post process, using depth to blend
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	// Create a texture for the color attachment

	glGenTextures(1, &m_colorTexture);
	glBindTexture(GL_TEXTURE_2D, m_colorTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

	// Create a depth texture
	glGenTextures(1, &m_depthTexture);
	glBindTexture(GL_TEXTURE_2D, m_depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Framebuffer is not complete!" << std::endl;
		throw;
	}

	m_colorTextureLocation = glGetUniformLocation(shader.ID, "u_colorTexture");
	m_depthTextureLocation = glGetUniformLocation(shader.ID, "u_depthTexture");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void RenderPlane::Draw(Shader& shader) const
{
	glDisable(GL_DEPTH_TEST);
	// Tell OpenGL which Shader Program we want to use
	glUseProgram(shader.ID);
	// Bind the VAO so OpenGL knows to use it
	glBindVertexArray(VAO);


	// Read color texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colorTexture);
	glUniform1i(m_colorTextureLocation, 0);

	// Read depth texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_depthTexture);
	glUniform1i(m_depthTextureLocation, 1);



	glDisable(GL_DEPTH_TEST);


	// Draw the triangle using the GL_TRIANGLES primitive
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	glEnable(GL_DEPTH_TEST);
}

void RenderPlane::Draw(Shader& shader, Shape* shapes, size_t size) const
{



	glDisable(GL_DEPTH_TEST);
	// Tell OpenGL which Shader Program we want to use
	glUseProgram(shader.ID);
	// Bind the VAO so OpenGL knows to use it
	glBindVertexArray(VAO);


	// Read color texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colorTexture);
	glUniform1i(m_colorTextureLocation, 0);

	// Read depth texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_depthTexture);
	glUniform1i(m_depthTextureLocation, 1);



	glDisable(GL_DEPTH_TEST);


	// Draw the triangle using the GL_TRIANGLES primitive
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);



	// Bind UBO
	glBindBuffer(GL_UNIFORM_BUFFER, UBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4 * size, shapes, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);



	shader.setUniform("u_loadedObjects", (int)size);
	glEnable(GL_DEPTH_TEST);
}

void RenderPlane::Delete()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

unsigned int RenderPlane::GetFrameBuffer() const
{
	return FBO;
}
