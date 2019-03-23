#ifndef AmbientQuad_h
#define AmbientQuad_h
#include "graphics/ScreenQuad.hpp"
#include "Common.hpp"


/**
 \brief Renders the ambient lighting contribution of a scene, including irradiance and ambient occlusion.
 \see GLSL::Frag::Ambient, GLSL::Frag::Ssao
 \ingroup DeferredRendering
 */
class AmbientQuad {

public:

	/** Constructor. */
	AmbientQuad();
	
	/** Setup against the graphics API, register the textures needed.
	 \param texAlbedo the index of the texture containing the albedo
	 \param texNormals the index of the texture containing the surface normals
	 \param texEffects the index of the texture containing the material properties
	 \param texDepth the index of the texture containing the depth
	 \param texSSAO the index of the texture containing the SSAO result
	 */
	void init(const GLuint texAlbedo, const GLuint texNormals, const GLuint texEffects, const GLuint texDepth, const GLuint texSSAO);
	
	/** Register the scene-specific lighting informations.
	 \param reflectionMap the ID of the background cubemap, containing radiance convolved with increasing roughness lobes in the mipmap levels
	 \param irradiance the SH coefficients of the background irradiance
	 */
	void setSceneParameters(const GLuint reflectionMap, const std::vector<glm::vec3> & irradiance);
	
	/** Draw the ambient lighting contribution to the scene.
	 \param viewMatrix the current camera view matrix
	 \param projectionMatrix the current camera projection matrix
	 */
	void draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
	
	
	/** Clean internal resources. */
	void clean() const;
	
private:
	
	
	std::shared_ptr<ProgramInfos> _program; ///< The ambient lighting program.

	std::vector<GLuint> _textures; ///< The input textures for the ambient pass.
	GLuint _textureEnv; ///< The environment radiance cubemap.
	GLuint _textureBrdf; ///< The linearized approximate BRDF components. \see BRDFEstimator
	
};

#endif
