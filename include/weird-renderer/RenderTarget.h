#pragma once
#include"Mesh.h"
#include "Shape.h"
#include "Shape2D.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class RenderTarget
		{
		public:
			RenderTarget() = default;
			RenderTarget(bool shapeRenderer);

			void bind() const;
			void free();

			void bindTextureToFrameBuffer(const Texture& texture, GLenum attachment);
			void bindColorTextureToFrameBuffer(const Texture& texture, int attachment = 0);
			void bindDepthTextureToFrameBuffer(const Texture& texture);

			unsigned int getFrameBuffer() const;

			const Texture* getColorAttachment(int attachment = 0);
			const Texture* getDepthAttachment();

		private:
			GLuint FBO;

			std::vector<const Texture*> m_colorAttachments;
			const Texture* m_depthAttachement;
		};
	}
}
