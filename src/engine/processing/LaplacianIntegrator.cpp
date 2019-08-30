#include "processing/LaplacianIntegrator.hpp"
#include "graphics/ScreenQuad.hpp"
#include "graphics/GLUtilities.hpp"

LaplacianIntegrator::LaplacianIntegrator(unsigned int width, unsigned int height, unsigned int downscaling) :
	_pyramid(width / downscaling, height / downscaling, 1),
	_scale(int(downscaling)) {

	// Pre and post process helpers.
	_prepare   = Resources::manager().getProgram2D("laplacian");
	_composite = Resources::manager().getProgram2D("passthrough");

	const Descriptor desc = {Layout::RGBA32F, Filter::NEAREST_NEAREST, Wrap::CLAMP};
	_preproc			  = std::unique_ptr<Framebuffer>(new Framebuffer(_pyramid.width(), _pyramid.height(), desc, false));
	_compo				  = std::unique_ptr<Framebuffer>(new Framebuffer(width, height, Layout::RGBA8, false));

	const float h1[5] = {0.15f, 0.5f, 0.7f, 0.5f, 0.15f};
	const float h2	= 1.0f;
	const float g[3]  = {0.175f, 0.547f, 0.175f};
	_pyramid.setFilters(h1, h2, g);

	checkGLError();
}

void LaplacianIntegrator::process(const Texture * textureId) {

	// First, compute the laplacian of each color channel (adding a 1px zero margin).
	glDisable(GL_DEPTH_TEST);

	_preproc->bind();
	_preproc->setViewport();
	GLUtilities::clearColor(glm::vec4(0.0f));
	_prepare->use();
	_prepare->uniform("scale", _scale);
	ScreenQuad::draw(textureId);
	_preproc->unbind();

	// Run the convolutional pyramid filter.
	_pyramid.process(_preproc->textureId());

	// Upscale to the final resolution.
	_compo->bind();
	_compo->setViewport();
	_composite->use();
	ScreenQuad::draw(_pyramid.textureId());
	_compo->unbind();
}

void LaplacianIntegrator::clean() const {
	_pyramid.clean();
	_compo->clean();
	_preproc->clean();
}

void LaplacianIntegrator::resize(unsigned int width, unsigned int height) {
	_pyramid.resize(width / _scale, height / _scale);
	_preproc->resize(_pyramid.width(), _pyramid.height());
	_compo->resize(width, height);
}
