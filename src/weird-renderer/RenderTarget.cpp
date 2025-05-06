#include "weird-renderer/RenderTarget.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		RenderTarget::RenderTarget(bool shapeRenderer)
		{
			// Frame buffer to store render output
			glGenFramebuffers(1, &FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		}


		void RenderTarget::Bind() const
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		}


		void RenderTarget::Delete()
		{
			// glDeleteFramebuffers(FBO);
		}


		void RenderTarget::BindTextureToFrameBuffer(const Texture& texture, GLenum attachment)
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


		void RenderTarget::BindColorTextureToFrameBuffer(const Texture& texture)
		{
			BindTextureToFrameBuffer(texture, GL_COLOR_ATTACHMENT0);
		}


		void RenderTarget::BindDepthTextureToFrameBuffer(const Texture& texture)
		{
			BindTextureToFrameBuffer(texture, GL_DEPTH_ATTACHMENT);
		}


		unsigned int RenderTarget::GetFrameBuffer() const
		{
			return FBO;
		}
	}
}