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
			void BindColorTextureToFrameBuffer(const Texture& texture);
			void BindDepthTextureToFrameBuffer(const Texture& texture);

			unsigned int GetFrameBuffer() const;

		private:
			GLuint FBO;
		};
	}
}
