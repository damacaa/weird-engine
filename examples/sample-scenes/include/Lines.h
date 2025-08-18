#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class LinesScene : public Scene
{
private:
    Texture *m_colorTextureCopy;

    RenderPlane *m_renderPlane;
    Shader *m_lineShader;

    RenderTarget *m_lineRender;
    Texture *m_lineTexture;

    Shader *m_combinationShader;

    Entity m_monkey;

    void onCreate() override
    {
        m_renderPlane = new RenderPlane();
        m_colorTextureCopy = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);
        m_lineShader = new Shader(SHADERS_PATH "renderPlane.vert", ASSETS_PATH "lines/lines.frag");

        m_lineRender = new RenderTarget(false);
        m_lineTexture = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);
        m_lineRender->bindColorTextureToFrameBuffer(*m_lineTexture);

        m_combinationShader = new Shader(SHADERS_PATH "renderPlane.vert", ASSETS_PATH "lines/combination.frag");
    }

    // Inherited via Scene
    void onStart() override
    {
        m_renderMode = RenderMode::RayMarching3D;
        m_debugFly = false;

        {
            Entity entity = m_ecs.createEntity();
            Transform &t = m_ecs.addComponent<Transform>(entity);
            t.position = vec3(0, 0, 0);

            // MeshRenderer &mr = m_ecs.addComponent<MeshRenderer>(entity);

            // auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
            // mr.mesh = id;

            auto& sdf = m_ecs.addComponent<SDFRenderer>(entity);
			sdf.materialId = 0;

            m_monkey = entity;
        }
    }

    void onUpdate(float delta) override
    {
        Transform &cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
        cameraTransform.position.y = 5.0f;
        cameraTransform.position.z -= 10.0f * delta;

        Transform& monkeyTransform = m_ecs.getComponent<Transform>(m_monkey);
        monkeyTransform.position = cameraTransform.position;
        monkeyTransform.position.z -= 5.0f;
    }

    void onRender(WeirdRenderer::RenderTarget &renderTarget) override
    {
        m_lineRender->bind();
        glClearColor(0, 0, 0, 0);                           // Set clear color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear both buffers

        // Copy
        glCopyImageSubData(
            renderTarget.getColorAttachment()->ID, GL_TEXTURE_2D, 0, 0, 0, 0, // 2 = scene texture
            m_colorTextureCopy->ID, GL_TEXTURE_2D, 0, 0, 0, 0,
            Screen::rWidth, Screen::rHeight, 1);

        // --- Line detection ---
        m_lineShader->use();

        m_lineShader->setUniform("t_sceneDepth", 0);
        renderTarget.getDepthAttachment()->bind(0);

        m_lineShader->setUniform("u_pixelSize", vec2(1.0f / Screen::rWidth, 1.0f / Screen::rHeight));

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        m_renderPlane->draw(*m_lineShader);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        // --- Final draw ---
        renderTarget.bind();

        m_combinationShader->use();

        m_combinationShader->setUniform("t_scene", 0);
        m_colorTextureCopy->bind(0);

        m_combinationShader->setUniform("t_lines", 1);
        m_lineTexture->bind(1);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        m_renderPlane->draw(*m_combinationShader);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
};