#include "DeferredRenderer.hpp"
#include "input/Input.hpp"
#include "scene/lights/DirectionalLight.hpp"
#include "scene/lights/PointLight.hpp"
#include "scene/lights/SpotLight.hpp"
#include "helpers/InterfaceUtilities.hpp"
#include <chrono>

DeferredRenderer::DeferredRenderer(RenderingConfig & config) : Renderer(config) {
	
	// Setup camera parameters.
	_userCamera.projection(config.screenResolution[0]/config.screenResolution[1], 1.3f, 0.01f, 200.0f);
	_cameraFOV = _userCamera.fov() * 180.0f / float(M_PI);
	
	const int renderWidth = (int)_renderResolution[0];
	const int renderHeight = (int)_renderResolution[1];
	const int renderHalfWidth = (int)(0.5f * _renderResolution[0]);
	const int renderHalfHeight = (int)(0.5f * _renderResolution[1]);
	// Find the closest power of 2 size.
	const int renderPow2Size = (int)std::pow(2,(int)floor(log2(_renderResolution[0])));
	
	// G-buffer setup.
	const Descriptor albedoDesc = { GL_RGBA16F, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE };
	const Descriptor normalDesc = { GL_RGB32F, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE };
	const Descriptor effectsDesc = { GL_RGB8, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE };
	const Descriptor depthDesc = { GL_DEPTH_COMPONENT32F, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE };
	const std::vector<Descriptor> descs = {albedoDesc, normalDesc, effectsDesc, depthDesc};
	_gbuffer = std::unique_ptr<Framebuffer>(new Framebuffer(renderWidth, renderWidth, descs, false));
	
	// Other framebuffers.
	_ssaoPass = std::unique_ptr<SSAO>(new SSAO(renderHalfWidth, renderHalfHeight, 0.5f));
	_sceneFramebuffer = std::unique_ptr<Framebuffer>(new Framebuffer(renderWidth, renderHeight, GL_RGBA16F, false));
	_bloomFramebuffer = std::unique_ptr<Framebuffer>(new Framebuffer(renderPow2Size, renderPow2Size, GL_RGB16F, false));
	
	_toneMappingFramebuffer = std::unique_ptr<Framebuffer>(new Framebuffer(renderWidth, renderHeight, GL_RGBA8, false));
	_fxaaFramebuffer = std::unique_ptr<Framebuffer>(new Framebuffer(renderWidth, renderHeight, GL_RGBA8, false));
	
	_blurBuffer = std::unique_ptr<GaussianBlur>(new GaussianBlur(renderPow2Size, renderPow2Size, 2, GL_RGB16F));
	
	
	checkGLError();

	// GL options
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glBlendEquation (GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	
	_bloomProgram = Resources::manager().getProgram2D("bloom");
	_toneMappingProgram = Resources::manager().getProgram2D("tonemap");
	_fxaaProgram = Resources::manager().getProgram2D("fxaa");
	_finalProgram = Resources::manager().getProgram2D("final_screenquad");
	
	_skyboxProgram = Resources::manager().getProgram("skybox_gbuffer");
	_bgProgram = Resources::manager().getProgram("background_gbuffer");
	_parallaxProgram = Resources::manager().getProgram("parallax_gbuffer");
	_objectProgram = Resources::manager().getProgram("object_gbuffer");
	
	const std::vector<GLuint> ambientTextures = _gbuffer->textureIds();
	
	// Add the SSAO result.
	_ambientScreen.init(ambientTextures[0], ambientTextures[1], ambientTextures[2], _gbuffer->depthId(), _ssaoPass->textureId());
	
	checkGLError();
	
}

void DeferredRenderer::setScene(std::shared_ptr<Scene> scene){
	_scene = scene;
	if(!scene){
		return;
	}
	
	auto start = std::chrono::steady_clock::now();
	_scene->init();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	Log::Info() << "Loading took " << duration.count() << "ms." << std::endl;
	const BoundingBox & bbox = _scene->getBoundingBox();
	const float range = glm::length(bbox.getSize());
	_userCamera.frustum(0.01f*range, 5.0f*range);
	_userCamera.speed() = 0.2f*range;
	_ambientScreen.setSceneParameters(_scene->backgroundReflection->id, _scene->backgroundIrradiance);
	
	std::vector<GLuint> includedTextures = _gbuffer->textureIds();
	/// \todo clarify this.
	includedTextures.insert(includedTextures.begin()+2, _gbuffer->depthId());
	
	for(auto& light : _scene->lights){
		light->init(includedTextures);
	}
	checkGLError();
}

void DeferredRenderer::renderScene(){
	// Bind the full scene framebuffer.
	_gbuffer->bind();
	// Set screen viewport
	_gbuffer->setViewport();
	
	// Clear the depth buffer (we know we will draw everywhere, no need to clear color.
	glClear(GL_DEPTH_BUFFER_BIT);
	
	const glm::mat4 & view = _userCamera.view();
	const glm::mat4 & proj = _userCamera.projection();
	for(auto & object : _scene->objects){
		// Combine the three matrices.
		const glm::mat4 MV = view * object.model();
		const glm::mat4 MVP = proj * MV;
		// Compute the normal matrix
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(MV)));
		
		// Select the program (and shaders).
		switch (object.type()) {
			case Object::PBRParallax:
				glUseProgram(_parallaxProgram->id());
				// Upload the MVP matrix.
				glUniformMatrix4fv(_parallaxProgram->uniform("mvp"), 1, GL_FALSE, &MVP[0][0]);
				// Upload the projection matrix.
				glUniformMatrix4fv(_parallaxProgram->uniform("p"), 1, GL_FALSE, &proj[0][0]);
				// Upload the MV matrix.
				glUniformMatrix4fv(_parallaxProgram->uniform("mv"), 1, GL_FALSE, &MV[0][0]);
				// Upload the normal matrix.
				glUniformMatrix3fv(_parallaxProgram->uniform("normalMatrix"), 1, GL_FALSE, &normalMatrix[0][0]);
				break;
			case Object::PBRRegular:
			case Object::PBRNoNormal:
				glUseProgram(_objectProgram->id());
				// Upload the MVP matrix.
				glUniformMatrix4fv(_objectProgram->uniform("mvp"), 1, GL_FALSE, &MVP[0][0]);
				// Upload the normal matrix.
				glUniformMatrix3fv(_objectProgram->uniform("normalMatrix"), 1, GL_FALSE, &normalMatrix[0][0]);
				glUniform1i(_objectProgram->uniform("defaultNormal"), int(object.type() == Object::PBRNoNormal));
				break;
			default:
			
			break;
		}
		
		// Bind the textures.
		GLUtilities::bindTextures(object.textures());
		GLUtilities::drawMesh(*object.mesh());
		glUseProgram(0);
	}
	
	if(_debugVisualization){
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
		for(auto & light : _scene->lights){
			light->drawDebug(_userCamera.view(), _userCamera.projection());
		}
		
		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
	
	// No need to write the skybox depth to the framebuffer.
	glDepthMask(GL_FALSE);
	// Accept a depth of 1.0 (far plane).
	glDepthFunc(GL_LEQUAL);
	const Object & background = _scene->background;
	if(_scene->backgroundMode == Scene::Background::SKYBOX){
		const glm::mat4 backgroundMVP = proj * view * background.model();
		// draw background.
		glUseProgram(_skyboxProgram->id());
		// Upload the MVP matrix.
		glUniformMatrix4fv(_skyboxProgram->uniform("mvp"), 1, GL_FALSE, &backgroundMVP[0][0]);
		GLUtilities::bindTextures(background.textures());
		GLUtilities::drawMesh(*background.mesh());
	} else {
		glUseProgram(_bgProgram->id());
		if(_scene->backgroundMode == Scene::Background::IMAGE){
			glUniform1i(_bgProgram->uniform("useTexture"), 1);
			GLUtilities::bindTextures(background.textures());
		} else {
			glUniform1i(_bgProgram->uniform("useTexture"), 0);
			glUniform3fv(_bgProgram->uniform("bgColor"), 1, &_scene->backgroundColor[0]);
		}
		GLUtilities::drawMesh(*background.mesh());
	}
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	
	
	// Unbind the full scene framebuffer.
	_gbuffer->unbind();
}

GLuint DeferredRenderer::renderPostprocess(const glm::vec2 & invRenderSize){
	
	if(_applyBloom){
		// --- Bloom selection pass ------
		_bloomFramebuffer->bind();
		_bloomFramebuffer->setViewport();
		glUseProgram(_bloomProgram->id());
		ScreenQuad::draw(_sceneFramebuffer->textureId());
		_bloomFramebuffer->unbind();
		
		// --- Bloom blur pass ------
		_blurBuffer->process(_bloomFramebuffer->textureId());
		
		// Draw the blurred bloom back into the scene framebuffer.
		_sceneFramebuffer->bind();
		_sceneFramebuffer->setViewport();
		glEnable(GL_BLEND);
		_blurBuffer->draw();
		glDisable(GL_BLEND);
		_sceneFramebuffer->unbind();
	}
	
	GLuint currentResult = _sceneFramebuffer->textureId();
	
	if(_applyTonemapping){
		// --- Tonemapping pass ------
		_toneMappingFramebuffer->bind();
		_toneMappingFramebuffer->setViewport();
		glUseProgram(_toneMappingProgram->id());
		glUniform1f(_toneMappingProgram->uniform("customExposure"), _exposure);
		ScreenQuad::draw(currentResult);
		_toneMappingFramebuffer->unbind();
		currentResult = _toneMappingFramebuffer->textureId();
	}
	
	if(_applyFXAA){
		// --- FXAA pass -------
		// Bind the post-processing framebuffer.
		_fxaaFramebuffer->bind();
		_fxaaFramebuffer->setViewport();
		glUseProgram(_fxaaProgram->id());
		glUniform2fv(_fxaaProgram->uniform("inverseScreenSize"), 1, &(invRenderSize[0]));
		ScreenQuad::draw(currentResult);
		_fxaaFramebuffer->unbind();
		currentResult = _fxaaFramebuffer->textureId();
	}
	
	return currentResult;
}

void DeferredRenderer::draw() {
	
	if(!_scene){
		glClearColor(0.2f,0.2,0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}
	const glm::vec2 invRenderSize = 1.0f / _renderResolution;
	
	// --- Light pass -------
	if(_updateShadows){
		for(auto& light : _scene->lights){
			light->drawShadow(_scene->objects);
		}
	}
	
	// --- Scene pass -------
	renderScene();
	
	// --- SSAO pass
	glDisable(GL_DEPTH_TEST);
	if(_applySSAO){
		_ssaoPass->process(_userCamera.projection(), _gbuffer->depthId(), _gbuffer->textureId(int(TextureType::Normal)));
	} else {
		_ssaoPass->clear();
	}
	
	// --- Gbuffer composition pass
	_sceneFramebuffer->bind();
	_sceneFramebuffer->setViewport();
	_ambientScreen.draw(_userCamera.view(), _userCamera.projection());
	glEnable(GL_BLEND);
	for(auto& light : _scene->lights){
		light->draw(_userCamera.view(), _userCamera.projection(), invRenderSize);
	}
	glDisable(GL_BLEND);
	_sceneFramebuffer->unbind();
	
	// --- Post process passes -----
	const GLuint currentResult = renderPostprocess(invRenderSize);
	
	// --- Final pass -------
	// We now render a full screen quad in the default framebuffer, using sRGB space.
	glEnable(GL_FRAMEBUFFER_SRGB);
	glViewport(0, 0, GLsizei(_config.screenResolution[0]), GLsizei(_config.screenResolution[1]));
	glUseProgram(_finalProgram->id());
	ScreenQuad::draw(currentResult);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST);
	
	checkGLError();
}

void DeferredRenderer::update(){
	Renderer::update();
	// If no scene, no need to udpate the camera or the scene-specific UI.
	if(!_scene){
		return;
	}
	_userCamera.update();
	
	if(ImGui::Begin("Renderer")){
		ImGui::PushItemWidth(100);
		ImGui::InputFloat("Camera speed", &_userCamera.speed(), 0.1f, 1.0f);
		if(ImGui::InputFloat("Camera FOV", &_cameraFOV, 1.0f, 10.0f)){
			_userCamera.fov(_cameraFOV*float(M_PI)/180.0f);
		}
		ImGui::Combo("Camera mode", (int*)(&_userCamera.mode()), "FPS\0Turntable\0Joystick\0\0", 3);
		ImGui::Separator();
		if(ImGui::InputInt("Vertical res.", &_config.internalVerticalResolution, 50, 200)){
			resize(int(_config.screenResolution[0]), int(_config.screenResolution[1]));
		}
		ImGui::PopItemWidth();
		ImGui::Checkbox("SSAO", &_applySSAO); ImGui::SameLine(120);
		ImGui::Checkbox("Bloom", &_applyBloom);
		ImGui::Checkbox("Tonemapping ", &_applyTonemapping); ImGui::SameLine(120);
		ImGui::Checkbox("FXAA", &_applyFXAA);
		ImGui::SliderFloat("Exposure", &_exposure, 0.1f, 10.0f);
		ImGui::Separator();
		ImGui::ColorEdit3("Background color", &_scene->backgroundColor[0]);
		ImGui::Checkbox("Show debug lights", &_debugVisualization);
		ImGui::Checkbox("Update shadows", &_updateShadows);
		
	}
	ImGui::End();
}

void DeferredRenderer::physics(double fullTime, double frameTime){
	_userCamera.physics(frameTime);
	if(_scene){
		_scene->update(fullTime, frameTime);
	}
}


void DeferredRenderer::clean() const {
	Renderer::clean();
	// Clean objects.
	_gbuffer->clean();
	_blurBuffer->clean();
	_ssaoPass->clean();
	_bloomFramebuffer->clean();
	_sceneFramebuffer->clean();
	_toneMappingFramebuffer->clean();
	_fxaaFramebuffer->clean();
	if(_scene){
		_scene->clean();
	}
}


void DeferredRenderer::resize(unsigned int width, unsigned int height){
	Renderer::updateResolution(width, height);
	// Resize the framebuffers.
	_gbuffer->resize(_renderResolution);
	_ssaoPass->resize(_renderResolution[0] / 2.0f, _renderResolution[1] / 2.0f);
	_toneMappingFramebuffer->resize(_renderResolution);
	_fxaaFramebuffer->resize(_renderResolution);
	if(_scene){
		_sceneFramebuffer->resize(_renderResolution);
	}
	checkGLError();
}



