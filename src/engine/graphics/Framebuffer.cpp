#include "graphics/Framebuffer.hpp"
#include "graphics/GPUObjects.hpp"
#include "graphics/GLUtilities.hpp"

Framebuffer::Framebuffer(){
	
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, const Layout typedFormat, bool depthBuffer) : Framebuffer(width, height, { Descriptor(typedFormat, Filter::LinearMipNearest, Wrap::Clamp) }, depthBuffer) {
	
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, const Descriptor & descriptor, bool depthBuffer) : Framebuffer(width, height, std::vector<Descriptor>(1, descriptor) , depthBuffer){
	
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, const std::vector<Descriptor> & descriptors, bool depthBuffer) {
	
	_width = width;
	_height = height;
	_depthUse = Depth::NONE;
	
	// Create a framebuffer.
	glGenFramebuffers(1, &_id);
	glBindFramebuffer(GL_FRAMEBUFFER, _id);
	
	for(size_t i = 0; i < descriptors.size(); ++i){
		// Create the color texture to store the result.
		const auto & descriptor = descriptors[i];
		
		const Layout & format = descriptor.typedFormat();
		const bool isDepthComp = format == DEPTH_COMPONENT16 || format == DEPTH_COMPONENT24 || format == DEPTH_COMPONENT32F;
		const bool isDepthStencilComp = format == DEPTH24_STENCIL8 || format == DEPTH32F_STENCIL8;
		
		if(isDepthComp || isDepthStencilComp){
			_depthUse = Depth::TEXTURE;
			_idDepth.width = _width;
			_idDepth.height = _height;
			_idDepth.depth = 1;
			_idDepth.levels = 1;
			_idDepth.shape = TextureShape::D2;
			GLUtilities::setupTexture(_idDepth, descriptor);
			
			// Link the texture to the depth attachment of the framebuffer.
			glBindTexture(GL_TEXTURE_2D, _idDepth.gpu->id);
			glFramebufferTexture2D(GL_FRAMEBUFFER, (isDepthStencilComp ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT), GL_TEXTURE_2D, _idDepth.gpu->id, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		} else {
			_idColors.emplace_back();
			Texture & tex = _idColors.back();
			tex.width = _width;
			tex.height = _height;
			tex.depth = 1;
			tex.levels = 1;
			tex.shape = TextureShape::D2;
			GLUtilities::setupTexture(tex, descriptor);
			
			// Link the texture to the color attachment (ie output) of the framebuffer.
			glBindTexture(GL_TEXTURE_2D, tex.gpu->id);
			const GLuint slot = GLuint(int(_idColors.size())-1);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D, tex.gpu->id, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		}
	}
	
	// If the depth buffer has not been setup yet but we require it, use a renderbuffer.
	if(_depthUse == Depth::NONE && depthBuffer){
		_idDepth.width = _width;
		_idDepth.height = _height;
		_idDepth.levels = 1;
		_idDepth.shape = TextureShape::D2;
		_idDepth.gpu.reset(new GPUTexture(Descriptor(), _idDepth.shape));
		// Create the renderbuffer (depth buffer).
		glGenRenderbuffers(1, &_idDepth.gpu->id);
		glBindRenderbuffer(GL_RENDERBUFFER, _idDepth.gpu->id);
		// Setup the depth buffer storage.
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, (GLsizei)_width, (GLsizei)_height);
		// Link the renderbuffer to the framebuffer.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _idDepth.gpu->id);
		_depthUse = Depth::RENDERBUFFER;
	}
	
	//Register which color attachments to draw to.
	std::vector<GLenum> drawBuffers(_idColors.size());
	for(size_t i = 0; i < _idColors.size(); ++i){
		drawBuffers[i] = GL_COLOR_ATTACHMENT0 + GLuint(i);
	}
	glDrawBuffers(GLsizei(drawBuffers.size()), &drawBuffers[0]);
	checkGLFramebufferError();
	checkGLError();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void Framebuffer::bind() const {
	glBindFramebuffer(GL_FRAMEBUFFER, _id);
}

void Framebuffer::bind(Mode mode) const {
	glBindFramebuffer(mode == Mode::READ ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER, _id);
}

void Framebuffer::setViewport() const
{
	glViewport(0, 0, (GLsizei)_width, (GLsizei)_height);
}

void Framebuffer::unbind() const {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::resize(unsigned int width, unsigned int height){
	_width = width;
	_height = height;
	
	// Resize the renderbuffer.
	if (_depthUse == Depth::RENDERBUFFER) {
		_idDepth.width = _width;
		_idDepth.height = _height;
		glBindRenderbuffer(GL_RENDERBUFFER, _idDepth.gpu->id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, (GLsizei)_width, (GLsizei)_height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		
	} else if(_depthUse == Depth::TEXTURE){
		_idDepth.width = _width;
		_idDepth.height = _height;
		GLUtilities::allocateTexture(_idDepth);
		
	}
	
	// Resize the textures.
	for(size_t i = 0; i < _idColors.size(); ++i){
		_idColors[i].width = _width;
		_idColors[i].height = _height;
		GLUtilities::allocateTexture(_idColors[i]);
	}
}

void Framebuffer::resize(glm::vec2 size){
	resize((unsigned int)size[0], (unsigned int)size[1]);
}

void Framebuffer::clean() {
	if (_depthUse == Depth::RENDERBUFFER) {
		glDeleteRenderbuffers(1, &_idDepth.gpu->id);
	} else if(_depthUse == Depth::TEXTURE){
		_idDepth.clean();
	}
	for(Texture & idColor : _idColors){
		idColor.clean();
	}
	glDeleteFramebuffers(1, &_id);
}


Framebuffer * Framebuffer::defaultFramebuffer = nullptr;

const Framebuffer & Framebuffer::backbuffer(){
	// Initialize a dummy framebuffer representing the backbuffer.
	if(!defaultFramebuffer){
		defaultFramebuffer = new Framebuffer();
		defaultFramebuffer->_idColors.emplace_back();
		// We don't really need to allocate the texture, just setup its descriptor.
		Texture & tex = defaultFramebuffer->_idColors.back();
		tex.shape = TextureShape::D2;
		tex.levels = 1;
		tex.depth = 1;
		tex.gpu.reset(new GPUTexture(Descriptor(RGBA8, Filter::Nearest, Wrap::Clamp), tex.shape));
	}
	return *defaultFramebuffer;
}

