#pragma once

#include "scene/Animation.hpp"
#include "resources/ResourcesManager.hpp"
#include "Codable.hpp"
#include "Common.hpp"

/**
 \brief Represent a 3D textured object.
 \ingroup Scene
 */
class Object {

public:

	/// \brief Type of shading/effects.
	enum Type : int {
		Common = 0, ///< Any type of shading.
		PBRRegular, ///< PBR shading. \see GLSL::Vert::Object_gbuffer, GLSL::Frag::Object_gbuffer
		PBRParallax, ///< PBR with parallax mapping. \see GLSL::Vert::Parallax_gbuffer, GLSL::Frag::Parallax_gbuffer
		PBRNoUVs
	};

	/** Constructor */
	Object();

	/** Construct a new object.
	 \param type the type of shading and effects to use when rendering this object
	 \param mesh the geometric mesh infos
	 \param castShadows denote if the object should cast shadows
	 */
	Object(const Object::Type type, const MeshInfos * mesh, bool castShadows);
	
	/** Register a texture.
	 \param infos the texture infos to add
	 */
	void addTexture(const TextureInfos * infos);
	
	/** Add an animation to apply at each frame.
	 \param anim the animation to add
	 */
	void addAnimation(std::shared_ptr<Animation> anim);
	
	/** Update the object transformation matrix.
	 \param model the new model matrix
	 */
	void set(const glm::mat4 & model){ _model = model; }
	
	/** Apply the animations for a frame duration.
	 \param fullTime the time since the launch of the application
	 \param frameTime the time elapsed since the last frame
	 */
	virtual void update(double fullTime, double frameTime);
	
	/** Query the bounding box of the object.
	 \return the bounding box
	 \note For mesh space bounding box, call boundingBox on mesh().
	 */
	BoundingBox boundingBox() const;
	
	/** Mesh getter.
	 \return the mesh infos
	 */
	const MeshInfos * mesh() const { return _mesh; }
	
	/** Textures array getter.
	 \return a vector containing the infos of the textures associated to the object
	 */
	const std::vector<const TextureInfos *> & textures() const { return _textures; }
	
	/** Object pose getter.
	 \return the model matrix
	 */
	const glm::mat4 & model() const { return _model; }
	
	/** Type getter.
	 \return the type of the object
	 \note This can be used in different way by different applications.
	 */
	const Type & type() const { return _material; }
	
	/** Is the object casting a shadow.
	 \return a boolean denoting if the object is a caster
	 */
	bool castsShadow() const { return _castShadow; }
	
	/** Setup an object parameters from a list of key-value tuples. The following keywords will be searched for:
	 \verbatim
	 type: objecttype
	 mesh: meshname
	 translation: X,Y,Z
	 scaling: scale
	 orientation: axisX,axisY,axisZ angle
	 shadows: bool
	 textures:
	 	- texturetype: ...
	 	- ...
	 animations:
	 	- animationtype: ...
	 	- ...
	 \endverbatim
	 \param params the parameters tuple
	 \param mode the storage mode (CPU, GPU, both)
	 */
	virtual void decode(const KeyValues& params, const Storage mode);
	
	virtual ~Object() = default;
	
protected:
	
	const MeshInfos * _mesh; ///< Geometry of the object.
	std::vector<const TextureInfos *> _textures; ///< Textures used by the object.
	std::vector<std::shared_ptr<Animation>> _animations; ///< Animations list (applied in order).
	glm::mat4 _model = glm::mat4(1.0f); ///< The transformation matrix of the 3D model.
	Type _material = Type::Common; ///< The material type.
	bool _castShadow = true; ///< Can the object casts shadows.
};
