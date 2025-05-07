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

			void Bind() const;
			void Delete();

			void BindTextureToFrameBuffer(const Texture& texture, GLenum attachment);
			void BindColorTextureToFrameBuffer(const Texture& texture, int attachment = 0);
			void BindDepthTextureToFrameBuffer(const Texture& texture);

			unsigned int GetFrameBuffer() const;

			const Texture* getColorAttachment(int attachment = 0);

		private:
			GLuint FBO;

			std::vector<const Texture*> m_colorAttachments;
		};
	}
}
