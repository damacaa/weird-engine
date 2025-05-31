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


		void RenderTarget::bind() const
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		}


		void RenderTarget::free()
		{
			// glDeleteFramebuffers(FBO);
		}


		void RenderTarget::bindTextureToFrameBuffer(const Texture& texture, GLenum attachment)
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


		void RenderTarget::bindColorTextureToFrameBuffer(const Texture& texture, int attachment)
		{
			if (attachment >= m_colorAttachments.size())
			{
				m_colorAttachments.resize(attachment + 1);
			}

			m_colorAttachments[attachment] = &texture;

			bindTextureToFrameBuffer(texture, GL_COLOR_ATTACHMENT0 + attachment);
		}


		void RenderTarget::bindDepthTextureToFrameBuffer(const Texture& texture)
		{
			m_depthAttachement = &texture;
			bindTextureToFrameBuffer(texture, GL_DEPTH_ATTACHMENT);
		}


		unsigned int RenderTarget::getFrameBuffer() const
		{
			return FBO;
		}


		const Texture* RenderTarget::getColorAttachment(int attachment)
		{
			if (attachment >= m_colorAttachments.size())
			{
				return nullptr;
			}

			return m_colorAttachments[attachment];
		}

		const Texture* RenderTarget::getDepthAttachment()
		{
			return m_depthAttachement;
		}
	}
}