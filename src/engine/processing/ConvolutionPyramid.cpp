#include "processing/ConvolutionPyramid.hpp"
#include "graphics/ScreenQuad.hpp"
#include "graphics/GLUtilities.hpp"

ConvolutionPyramid::ConvolutionPyramid(unsigned int width, unsigned int height, unsigned int inoutPadding) :
	_padding(int(inoutPadding)) {

	// Convolution pyramids filters and scaling operations.
	_downscale = Resources::manager().getProgram2D("downscale");
	_upscale   = Resources::manager().getProgram2D("upscale");
	_filter	= Resources::manager().getProgram2D("filter");
	_padder	= Resources::manager().getProgram2D("passthrough-shift");

	// Pre and post process framebuffers.
	const Descriptor desc = {Layout::RGBA32F, Filter::NEAREST_NEAREST, Wrap::CLAMP};
	const Descriptor descSrc = {Layout::RGBA32F, Filter::LINEAR_NEAREST, Wrap::CLAMP};
	// Output is as the basic required size.
	_shifted = std::unique_ptr<Framebuffer>(new Framebuffer(width, height, descSrc, false, "Conv. pyramid shift"));
	// Resolution of the pyramid takes into account the filter padding.
	_resolution = glm::ivec2(width + 2 * _padding, height + 2 * _padding);

	// Create a series of framebuffers smaller and smaller.
	const int depth = int(std::ceil(std::log2(std::min(_resolution[0], _resolution[1]))));
	_levelsIn		= std::vector<std::unique_ptr<Framebuffer>>(depth);
	_levelsOut		= std::vector<std::unique_ptr<Framebuffer>>(depth);
	// Initial padded size.
	int levelWidth  = _resolution[0] + 2 * _size;
	int levelHeight = _resolution[1] + 2 * _size;
	// Generate framebuffer pyramids.
	for(size_t i = 0; i < size_t(depth); ++i) {
		_levelsIn[i]  = std::unique_ptr<Framebuffer>(new Framebuffer(levelWidth, levelHeight, desc, false, "Conv. pyramid in " + std::to_string(i)));
		_levelsOut[i] = std::unique_ptr<Framebuffer>(new Framebuffer(levelWidth, levelHeight, desc, false, "Conv. pyramid out " + std::to_string(i)));
		// Downscaling and padding.
		levelWidth /= 2;
		levelHeight /= 2;
		levelWidth += 2 * _size;
		levelHeight += 2 * _size;
	}
}

void ConvolutionPyramid::process(const Texture * texture) {

	// Pad by the size of the filter.
	_levelsIn[0]->bind();
	// Shift the viewport and fill the padded region with 0s.
	GLUtilities::setViewport(_size, _size, int(_levelsIn[0]->width()) - 2 * _size, int(_levelsIn[0]->height()) - 2 * _size);
	GLUtilities::clearColor(glm::vec4(0.0f));
	// Transfer the boundary content.
	_padder->use();
	_padder->uniform("padding", _size);
	ScreenQuad::draw(texture);
	_levelsIn[0]->unbind();

	// Then iterate over all framebuffers, cascading down the filtered results.
	/// \note Those filters are separable, and could be applied in two passes (vertical and horizontal) to reduce the texture fetches count.
	// Send parameters.
	_downscale->use();
	_downscale->uniform("h1[0]", 5, &_h1[0]);

	// Do: l[i] = downscale(filter(l[i-1], h1))
	for(size_t i = 1; i < _levelsIn.size(); ++i) {
		_levelsIn[i]->bind();
		// Shift the viewport and fill the padded region with 0s.
		GLUtilities::clearColor(glm::vec4(0.0f));
		GLUtilities::setViewport(_size, _size, int(_levelsIn[i]->width()) - 2 * _size, int(_levelsIn[i]->height()) - 2 * _size);
		// Filter and downscale.
		ScreenQuad::draw(_levelsIn[i - 1]->texture());
		_levelsIn[i]->unbind();
	}

	// Filter the last level with g.
	// Send parameters.
	_filter->use();
	_filter->uniform("g[0]", 3, &_g[0]);
	// Do:  f[end] = filter(l[end], g)
	const auto & lastLevel = _levelsOut.back();
	lastLevel->bind();
	lastLevel->setViewport();
	ScreenQuad::draw(_levelsIn.back()->texture());
	lastLevel->unbind();

	// Flatten the pyramid from the bottom, combining the filtered current result and the next level.
	_upscale->use();
	_upscale->uniform("h1[0]", 5, &_h1[0]);
	_upscale->uniform("g[0]", 3, &_g[0]);
	_upscale->uniform("h2", _h2);

	// Do: f[i] = filter(l[i], g) + filter(upscale(f[i+1], h2)
	for(int i = int(_levelsOut.size() - 2); i >= 0; --i) {
		_levelsOut[i]->bind();
		_levelsOut[i]->setViewport();
		// Upscale with zeros, filter and combine.
		ScreenQuad::draw({_levelsIn[i]->texture(), _levelsOut[i + 1]->texture()});
		_levelsOut[i]->unbind();
	}

	// Compensate the initial padding.
	_shifted->bind();
	_shifted->setViewport();
	_padder->use();
	// Need to also compensate for the potential extra padding.
	_padder->uniform("padding", -_size - _padding);
	ScreenQuad::draw(_levelsOut[0]->texture());
	_shifted->unbind();
}

void ConvolutionPyramid::setFilters(const float h1[5], float h2, const float g[3]) {
	_h1[0] = h1[0];
	_h1[1] = h1[1];
	_h1[2] = h1[2];
	_h1[3] = h1[3];
	_h1[4] = h1[4];
	_h2	= h2;
	_g[0]  = g[0];
	_g[1]  = g[1];
	_g[2]  = g[2];
}

void ConvolutionPyramid::resize(unsigned int width, unsigned int height) {
	_shifted->resize(width, height);
	// Resolution of the pyramid takes into account the filter padding.
	_resolution = glm::ivec2(width + 2 * _padding, height + 2 * _padding);

	const int currentDepth = int(_levelsIn.size());

	const int newDepth = int(std::ceil(std::log2(std::min(_resolution[0], _resolution[1]))));
	// Create a series of framebuffers smaller and smaller.
	const Descriptor desc = {Layout::RGBA32F, Filter::NEAREST_NEAREST, Wrap::CLAMP};
	_levelsIn.resize(newDepth);
	_levelsOut.resize(newDepth);
	// Initial padded size.
	int levelWidth  = _resolution[0] + 2 * _size;
	int levelHeight = _resolution[1] + 2 * _size;

	// Generate framebuffer pyramids.
	for(int i = 0; i < newDepth; ++i) {
		if(i < currentDepth) {
			_levelsIn[i]->resize(levelWidth, levelHeight);
			_levelsOut[i]->resize(levelWidth, levelHeight);
		} else {
			_levelsIn[i]  = std::unique_ptr<Framebuffer>(new Framebuffer(levelWidth, levelHeight, desc, false, "Conv. pyramid in " + std::to_string(i)));
			_levelsOut[i] = std::unique_ptr<Framebuffer>(new Framebuffer(levelWidth, levelHeight, desc, false, "Conv. pyramid out " + std::to_string(i)));
		}
		// Downscaling and padding.
		levelWidth /= 2;
		levelHeight /= 2;
		levelWidth += 2 * _size;
		levelHeight += 2 * _size;
	}
}
