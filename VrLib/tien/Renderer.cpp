#include "Renderer.h"
#include "Scene.h"
#include "components/ModelRenderer.h"
#include "components/Transform.h"
#include "components/Camera.h"
#include "components/Light.h"
#include "components/SkyBox.h"

#include <VrLib/gl/FBO.h>
#include <VrLib/gl/VAO.h>
#include <VrLib/Model.h>
#include <VrLib/Texture.h>
#include <VrLib/gl/Vertex.h>

#include <btBulletCollisionCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vrlib
{
	namespace tien
	{
		Renderer::Renderer()
		{
			drawPhysicsDebug = false;
			drawLightDebug = false;
		}
		

		void Renderer::init()
		{
			postLightingShader = new vrlib::gl::Shader<PostLightingUniform>("data/vrlib/tien/shaders/postLighting.vert", "data/vrlib/tien/shaders/postLighting.frag");
			postLightingShader->bindAttributeLocation("a_position", 0);
			postLightingShader->link();
			postLightingShader->bindFragLocation("fragColor", 0);
			postLightingShader->registerUniform(PostLightingUniform::windowSize, "windowSize");
			postLightingShader->registerUniform(PostLightingUniform::modelViewMatrix, "modelViewMatrix");
			postLightingShader->registerUniform(PostLightingUniform::projectionMatrix, "projectionMatrix");
			postLightingShader->registerUniform(PostLightingUniform::modelViewMatrixInv, "modelViewMatrixInv");
			postLightingShader->registerUniform(PostLightingUniform::projectionMatrixInv, "projectionMatrixInv");
			postLightingShader->registerUniform(PostLightingUniform::shadowMatrix, "shadowMatrix");

			postLightingShader->registerUniform(PostLightingUniform::s_color, "s_color");
			postLightingShader->registerUniform(PostLightingUniform::s_normal, "s_normal");
			postLightingShader->registerUniform(PostLightingUniform::s_depth, "s_depth");
			postLightingShader->registerUniform(PostLightingUniform::s_shadowmap, "s_shadowmap");
			postLightingShader->registerUniform(PostLightingUniform::s_shadowmapcube, "s_shadowmapcube");
			postLightingShader->registerUniform(PostLightingUniform::lightType, "lightType");
			postLightingShader->registerUniform(PostLightingUniform::lightPosition, "lightPosition");
			postLightingShader->registerUniform(PostLightingUniform::lightDirection, "lightDirection");
			postLightingShader->registerUniform(PostLightingUniform::lightRange, "lightRange");
			postLightingShader->registerUniform(PostLightingUniform::lightColor, "lightColor");
			postLightingShader->registerUniform(PostLightingUniform::lightCastShadow, "lightCastShadow");
			postLightingShader->use();
			postLightingShader->setUniform(PostLightingUniform::s_color, 0);
			postLightingShader->setUniform(PostLightingUniform::s_normal, 1);
			postLightingShader->setUniform(PostLightingUniform::s_depth, 2);
			postLightingShader->setUniform(PostLightingUniform::s_shadowmap, 3);
			postLightingShader->setUniform(PostLightingUniform::s_shadowmapcube, 4);
			gbuffers = nullptr;





			physicsDebugShader = new vrlib::gl::Shader<PhysicsDebugUniform>("data/vrlib/tien/shaders/physicsdebug.vert", "data/vrlib/tien/shaders/physicsdebug.frag");
			physicsDebugShader->bindAttributeLocation("a_position", 0);
			physicsDebugShader->bindAttributeLocation("a_color", 1);
			physicsDebugShader->link();
			physicsDebugShader->registerUniform(PhysicsDebugUniform::projectionMatrix, "projectionMatrix");
			physicsDebugShader->registerUniform(PhysicsDebugUniform::modelViewMatrix, "modelViewMatrix");




			buildOverlay();


			mHead.init("MainUserHead");

		}

		void Renderer::render(const Scene& scene, const glm::mat4 &projectionMatrix, const glm::mat4 &modelMatrix)
		{
			int viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);
			if(!gbuffers)
				gbuffers = new vrlib::gl::FBO(viewport[2] - viewport[0], viewport[3] - viewport[1], true, vrlib::gl::FBO::Color, vrlib::gl::FBO::Normal);


			if (!scene.cameraNode)
			{
				logger << "No camera found" << Log::newline;
				return;
			}
			components::Camera* camera = scene.cameraNode->getComponent<components::Camera>();

			glm::mat4 modelViewMatrix = modelMatrix * glm::inverse(scene.cameraNode->transform->globalTransform);


			for (auto l : scene.lights)
			{
				if (l->light->shadow == components::Light::Shadow::shadowmap)
				{
					l->light->generateShadowMap();
					//TODO: generate depthmap for light
				}
			}




			//TODO: use camera
			gbuffers->bind();
			int oldFBO = gbuffers->oldFBO;
			glViewport(0, 0, gbuffers->getWidth(), gbuffers->getHeight());
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_CULL_FACE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);


			for (components::Renderable::RenderContext* context : scene.renderContexts)
				context->frameSetup(projectionMatrix, modelViewMatrix);

			for (Node* c : scene.renderables)
				c->getComponent<components::Renderable>()->draw();
			gbuffers->unbind();

			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			glClearColor(0.1f, 0.1f, 0.1f, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffers->fboId);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldFBO);
			glBlitFramebuffer(0, 0, gbuffers->getWidth(), gbuffers->getHeight(),
				viewport[0], viewport[1], viewport[2], viewport[3],
				GL_DEPTH_BUFFER_BIT, GL_NEAREST);

			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4);
			glDisableVertexAttribArray(5);


			gbuffers->use();
			postLightingShader->use();
			postLightingShader->setUniform(PostLightingUniform::windowSize, glm::vec2(viewport[2] - viewport[0], viewport[3] - viewport[1]));
			postLightingShader->setUniform(PostLightingUniform::projectionMatrix, projectionMatrix);
			postLightingShader->setUniform(PostLightingUniform::projectionMatrixInv, glm::inverse(projectionMatrix));
			postLightingShader->setUniform(PostLightingUniform::modelViewMatrixInv, glm::inverse(modelViewMatrix));


			overlayVao->bind();
			glDisable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);

			glDepthMask(GL_FALSE);
			glEnable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			//if (!scene.lights.empty() && scene.lights.front()->getComponent<components::Light>()->type != components::Light::Type::directional)
			//	glEnable(GL_BLEND);

			for (Node* c : scene.lights)
			{
				components::Light* l = c->getComponent<components::Light>();
				components::Transform* t = c->getComponent<components::Transform>();
				glm::vec3 pos(t->globalTransform * glm::vec4(0, 0, 0, 1));

				//if(l->type == components::Light::Type::directional)
				//else
//					glEnable(GL_DEPTH_TEST);

				if (l->type == components::Light::Type::directional)
					glCullFace(GL_BACK);
				else
					glCullFace(GL_FRONT);

				if (l->shadow == components::Light::Shadow::shadowmap && l->shadowMapDirectional)
				{
					if(l->type == components::Light::Type::directional)
						l->shadowMapDirectional->use(3);
					else
						l->shadowMapDirectional->use(4);
					postLightingShader->setUniform(PostLightingUniform::shadowMatrix, l->projectionMatrix * l->modelViewMatrix);
					postLightingShader->setUniform(PostLightingUniform::lightCastShadow, true);
				}
				else
					postLightingShader->setUniform(PostLightingUniform::lightCastShadow, false);


				postLightingShader->setUniform(PostLightingUniform::modelViewMatrix, glm::scale(glm::translate(modelViewMatrix, pos), glm::vec3(l->range, l->range, l->range)));
				postLightingShader->setUniform(PostLightingUniform::lightType, (int)l->type);
				postLightingShader->setUniform(PostLightingUniform::lightPosition, pos);
				postLightingShader->setUniform(PostLightingUniform::lightRange, l->range);
				postLightingShader->setUniform(PostLightingUniform::lightColor, l->color);
				if(l->type == components::Light::Type::directional)
					glDrawArrays(GL_QUADS, 0, 4);
				else
					glDrawArrays(GL_TRIANGLES, sphere.x, sphere.y-sphere.x);
				glEnable(GL_BLEND);
			}
			glCullFace(GL_BACK);

			auto skybox = scene.cameraNode->getComponent<vrlib::tien::components::SkyBox>();
			if (skybox)
			{
				if (!skybox->initialized)
					skybox->initialize();
				skybox->render(projectionMatrix, modelViewMatrix);
			}

			if (drawPhysicsDebug)
			{
				scene.world->debugDrawWorld();
				if (scene.debugDrawer->verts.size() > 0)
				{
					glDisable(GL_DEPTH_TEST);
					glBindVertexArray(0);
					glBindBuffer(GL_ARRAY_BUFFER, 0);
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
					physicsDebugShader->use();
					physicsDebugShader->setUniform(PhysicsDebugUniform::modelViewMatrix, modelViewMatrix);
					physicsDebugShader->setUniform(PhysicsDebugUniform::projectionMatrix, projectionMatrix);

					gl::setAttributes<gl::VertexP3C4>(&scene.debugDrawer->verts[0]);
					glLineWidth(1.0f);
					glDrawArrays(GL_LINES, 0, scene.debugDrawer->verts.size());
					scene.debugDrawer->flush();
					glEnable(GL_DEPTH_TEST);
				}
			}

			if (drawLightDebug)
			{
				std::vector<vrlib::gl::VertexP3C4> verts;
				for (auto l : scene.lights)
				{
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));

					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));


					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2, 0,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4( 0,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2, 1, 1)), glm::vec4(1, 0, 1, 1)));
					verts.push_back(vrlib::gl::VertexP3C4(glm::vec3(glm::inverse(l->light->projectionMatrix * l->light->modelViewMatrix) * glm::vec4(-2,-2,-1, 1)), glm::vec4(1, 0, 1, 1)));



				}
				if (!verts.empty())
				{
					//glDisable(GL_DEPTH_TEST);
					glBindVertexArray(0);
					glBindBuffer(GL_ARRAY_BUFFER, 0);
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
					physicsDebugShader->use();
					physicsDebugShader->setUniform(PhysicsDebugUniform::modelViewMatrix, modelViewMatrix);
					physicsDebugShader->setUniform(PhysicsDebugUniform::projectionMatrix, projectionMatrix);


					gl::setAttributes<gl::VertexP3C4>(&verts[0]);
					glLineWidth(4.0f);
					glDrawArrays(GL_LINES, 0, verts.size());
					scene.debugDrawer->flush();
				}
				glEnable(GL_DEPTH_TEST);
			}




			//camera->target->bind();

			//camera->target->unbind();

			glDisable(GL_BLEND);

		}


		void Renderer::buildOverlay()
		{
			std::vector<vrlib::gl::VertexP3> verts;
			vrlib::gl::VertexP3 vert;
			vrlib::gl::setP3(vert, glm::vec3(-1, -1, 0));	verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(1, -1, 0));	verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(1, 1, 0));	verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-1, 1, 0));	verts.push_back(vert);
			sphere.x = verts.size();
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, -1.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.723607, -0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, -1.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, -1.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, -1.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.723607, -0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.276388, -0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.894426, 0.000000, -0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.276388, 0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.723607, 0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.723607, -0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.276388, -0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.894426, 0.000000, -0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.276388, 0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.723607, 0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.276388, -0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.723607, -0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.723607, 0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.276388, 0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.894426, 0.000000, 0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, 1.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, 0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, 1.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.162456, 0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, 0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, 1.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.425323, 0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, -0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, 1.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.425323, -0.309011, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, -0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, 1.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.162456, -0.499995, 0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.894426, 0.000000, 0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.525730, 0.000000, 0.850652)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.894426, 0.000000, 0.447216)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, 0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, 0.499997, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, 0.850649, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, 0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, 0.809012, 0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, 0.525725, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, -0.525725, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.850648, 0.000000, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, -0.525725, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, -0.850649, 0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.262869, -0.809012, 0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, -0.850649, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.894426, 0.000000, 0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.688189, -0.499997, 0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, 0.850649, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, 0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 1.000000, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, 0.525725, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.587786, 0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.894426, 0.000000, -0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.723607, -0.525725, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, -0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.276388, -0.850649, 0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.000000, -1.000000, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.723607, -0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.587786, -0.809017, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.894426, 0.000000, 0.447216)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.951058, -0.309013, 0.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.723607, 0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.951058, 0.309013, 0.000000)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.723607, 0.525725, -0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, 0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, 0.809012, -0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, 0.850649, -0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, 0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.894426, 0.000000, -0.447216)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, 0.499997, -0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.894426, 0.000000, -0.447216)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.525730, 0.000000, -0.850652)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, -0.850649, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.688189, -0.499997, -0.525736)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.723607, 0.525725, -0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.850648, 0.000000, -0.525736)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.000000, 0.000000, -1.000000)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, 0.309011, -0.850654)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(-0.276388, -0.850649, -0.447220)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(-0.162456, -0.499995, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);

			vrlib::gl::setP3(vert, glm::vec3(0.425323, -0.309011, -0.850654)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.723607, -0.525725, -0.447220)); verts.push_back(vert);
			vrlib::gl::setP3(vert, glm::vec3(0.262869, -0.809012, -0.525738)); verts.push_back(vert);

			sphere.y = verts.size();
			overlayVerts = new vrlib::gl::VBO<vrlib::gl::VertexP3>();
			overlayVerts->setData(verts.size(), verts.data(), GL_STATIC_DRAW);
			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			overlayVao = new vrlib::gl::VAO(overlayVerts);
		}


	}
}