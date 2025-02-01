#include "weird-renderer/RenderPlane.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		RenderPlane::RenderPlane(bool shapeRenderer)
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

			if (shapeRenderer)
			{

				// Uniform buffer to store shapes
				glGenBuffers(1, &UBO);
				glBindBuffer(GL_UNIFORM_BUFFER, UBO);
				// Bind UBO to shader program (assuming location 0 for UBO)
				glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);


				// Generate shape buffer and texture
				glGenBuffers(1, &m_shapeBuffer);
				glGenTextures(1, &m_shapeTexture);

			}

			// Frame buffer to store render output
			glGenFramebuffers(1, &FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);


		}



		void RenderPlane::Draw(Shader& shader) const
		{

			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);


			//// Read color texture
			//glActiveTexture(GL_TEXTURE0);
			//glBindTexture(GL_TEXTURE_2D, m_colorTexture);
			//glUniform1i(m_colorTextureLocation, 0);

			//// Read depth texture
			//glActiveTexture(GL_TEXTURE1);
			//glBindTexture(GL_TEXTURE_2D, m_depthTexture);
			//glUniform1i(m_depthTextureLocation, 1);






			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		}

		void RenderPlane::Draw(Shader& shader, Shape* shapes, size_t size) const
		{
			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);

			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}

		void RenderPlane::Draw(Shader& shader, Dot2D* shapes, size_t size) const
		{



			glBindBuffer(GL_TEXTURE_BUFFER, m_shapeBuffer);
			glBufferData(GL_TEXTURE_BUFFER, sizeof(Dot2D) * size, shapes, GL_STREAM_DRAW);
			//glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(Dot2D) * size, shapes);

			// Bind the buffer to the buffer texture
			glBindTexture(GL_TEXTURE_BUFFER, m_shapeTexture);
			glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_shapeBuffer);

			// Bind texture to slot 1
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_BUFFER, m_shapeTexture);

			// Set uniforms
			GLuint myBufferTextureLocation = glGetUniformLocation(shader.ID, "u_shapeBuffer");
			glUniform1i(myBufferTextureLocation, 1); // Tell the shader the texture is in slot 1

			GLuint loadedObjectsLocation = glGetUniformLocation(shader.ID, "u_loadedObjects");
			glUniform1i(loadedObjectsLocation, size);


			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);
			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);


		}

		void RenderPlane::Delete()
		{
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
		}

		void RenderPlane::BindTextureToFrameBuffer(Texture texture, GLenum attachment)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);

			texture.bind();
			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture.ID, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				std::cerr << "Framebuffer is not complete!" << std::endl;
				throw;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void RenderPlane::BindColorTextureToFrameBuffer(Texture texture)
		{
			BindTextureToFrameBuffer(texture, GL_COLOR_ATTACHMENT0);
		}

		void RenderPlane::BindDepthTextureToFrameBuffer(Texture texture)
		{
			BindTextureToFrameBuffer(texture, GL_DEPTH_ATTACHMENT);
		}




		unsigned int RenderPlane::GetFrameBuffer() const
		{
			return FBO;
		}
	}
}