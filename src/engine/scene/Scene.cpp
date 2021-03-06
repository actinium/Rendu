#include "scene/Scene.hpp"
#include "scene/Sky.hpp"
#include "system/TextUtilities.hpp"
#include "system/Query.hpp"

#include <map>
#include <sstream>

Scene::Scene(const std::string & name) {
	// Append the extension if needed.
	std::string fullName = name;
	if(!TextUtilities::hasSuffix(name, ".scene")) {
		fullName += ".scene";
	}
	_name = fullName;
}

void printToken(const KeyValues & tk, const std::string & shift) {
	Log::Info() << shift << tk.key << ": " << std::endl;
	if(!tk.values.empty()) {
		Log::Info() << shift << "\t";
		for(const auto & val : tk.values) {
			Log::Info() << val << " | ";
		}
		Log::Info() << std::endl;
	}
	for(const auto & subtk : tk.elements) {
		printToken(subtk, shift + "\t");
	}
}

bool Scene::init(Storage options) {
	if(_loaded) {
		return true;
	}

	Query timer;
	timer.begin();
	
	// Define loaders for each keyword.
	std::map<std::string, void (Scene::*)(const KeyValues &, Storage)> loaders = {
		{"scene", &Scene::loadScene}, {"object", &Scene::loadObject}, {"point", &Scene::loadLight}, {"directional", &Scene::loadLight}, {"spot", &Scene::loadLight}, {"camera", &Scene::loadCamera}, {"background", &Scene::loadBackground}, {"probe", &Scene::loadProbe}};

	// Parse the file.
	const std::string sceneFile			   = Resources::manager().getString(_name);
	if(sceneFile.empty()) {
		return false;
	}
	const std::vector<KeyValues> allParams = Codable::decode(sceneFile);

	// Process each group of keyvalues.
	for(const auto & element : allParams) {
		const std::string key = element.key;
		// By construction (see above), all keys should have a loader.
		(this->*loaders[key])(element, options);
	}

	// Update all objects poses.
	for(auto & object : objects) {
		const glm::mat4 newModel = _sceneModel * object.model();
		object.set(newModel);
	}
	// The scene model matrix has been applied to all objects, we can reset it.
	_sceneModel = glm::mat4(1.0f);
	// Update all lights bounding box infos.
	_bbox = computeBoundingBox(true);
	for(auto & light : lights) {
		light->setScene(_bbox);
	}

	// Check if the environment has been setup.
	if(environment.type() == LightProbe::Type::DEFAULT){
		KeyValues defaultKv("probe");
		environment.decode(defaultKv, options);
	}
	_loaded = true;

	// Check if the scene is static.
	for(const auto & obj : objects){
		if(obj.animated()){
			_animated = true;
			break;
		}
	}
	for(const auto & light : lights){
		if(light->animated()){
			_animated = true;
			break;
		}
	}

	timer.end();
	Log::Info() << Log::Resources << "Loading took " << (float(timer.value())/1000000.0f) << "ms." << std::endl;
	return true;
}

void Scene::loadObject(const KeyValues & params, Storage options) {
	objects.emplace_back();
	objects.back().decode(params, options);
}

void Scene::loadLight(const KeyValues & params, Storage) {
	const auto light = Light::decode(params);
	if(light) {
		lights.push_back(light);
	}
}

void Scene::loadCamera(const KeyValues & params, Storage) {
	_camera.decode(params);
}

void Scene::loadBackground(const KeyValues & params, Storage options) {
	background = std::unique_ptr<Object>(new Object(Object::Type::Common, Resources::manager().getMesh("plane", options), false));

	for(const auto & param : params.elements) {
		if(param.key == "color") {
			backgroundMode = Background::COLOR;
			// Background is a plane, store the color.
			backgroundColor = Codable::decodeVec3(param);

		} else if(param.key == "image" && !param.elements.empty()) {
			backgroundMode = Background::IMAGE;
			// Load image described as sub-element.
			const auto texInfos = Codable::decodeTexture(param.elements[0]);
			const Texture * tex = Resources::manager().getTexture(texInfos.first, texInfos.second, options);
			background->addTexture(tex);

		} else if(param.key == "cube" && !param.elements.empty()) {
			backgroundMode = Background::SKYBOX;
			// Object is a textured skybox.
			background = std::unique_ptr<Object>(new Object(Object::Type::Common, Resources::manager().getMesh("skybox", options), false));
			background->decode(params, options);
			// Load cubemap described as subelement.
			const auto texInfos = Codable::decodeTexture(param.elements[0]);
			const Texture * tex = Resources::manager().getTexture(texInfos.first, texInfos.second, options);
			background->addTexture(tex);

		} else if(param.key == "sun") {
			// In that case the background is a sky object.
			backgroundMode = Background::ATMOSPHERE;
			background	 = std::unique_ptr<Sky>(new Sky(options));
			background->decode(params, options);
			// Load the scattering table.
			const Texture * tex = Resources::manager().getTexture("scattering-precomputed", {Layout::RGB32F, Filter::LINEAR_LINEAR, Wrap::CLAMP}, options);
			background->addTexture(tex);
		}
	}
}

void Scene::loadProbe(const KeyValues & params, Storage options) {
	environment.decode(params, options);
}

void Scene::loadScene(const KeyValues & params, Storage) {
	// Update matrix, there is at most one transformation in the scene object.
	_sceneModel = Codable::decodeTransformation(params.elements);
}

std::vector<KeyValues> Scene::encode() const {
	std::vector<KeyValues> tokens;
	
	// Encode the scene
	tokens.emplace_back("scene");
	auto & scnNode = tokens.back();
	scnNode.elements.push_back(environment.encode());
	
	// Encode the scene transformation.
	const auto modelKeys = Codable::encode(_sceneModel);
	for(const auto & key : modelKeys){
		scnNode.elements.push_back(key);
	}
	
	// Encode the background.
	KeyValues bgNode("background");
	
	switch (backgroundMode) {
		case Background::COLOR:
			bgNode.elements.emplace_back("color");
			bgNode.elements.back().values = { Codable::encode(backgroundColor) };
			break;
		case Background::IMAGE:
			bgNode.elements.emplace_back("image");
			bgNode.elements.back().elements = { Codable::encode(background->textures()[0]) };
			break;
		case Background::SKYBOX:
			bgNode.elements.emplace_back("cube");
			bgNode.elements.back().elements = { Codable::encode(background->textures()[0]) };
		break;
		case Background::ATMOSPHERE:
			bgNode = background->encode();
			bgNode.key = "background";
		break;
		default:
			break;
	}
	tokens.push_back(bgNode);
	// Encode the objects
	for(const auto & obj : objects){
		tokens.push_back(obj.encode());
	}
	// Encode the lights
	for(const auto & light : lights){
		tokens.push_back(light->encode());
	}
	tokens.push_back(_camera.encode());
	return tokens;
}

BoundingBox Scene::computeBoundingBox(bool onlyShadowCasters) {
	BoundingBox bbox;
	if(objects.empty()) {
		return bbox;
	}

	for(Object & obj : objects) {
		if(onlyShadowCasters && !obj.castsShadow()) {
			continue;
		}
		bbox.merge(obj.boundingBox());
	}
	Log::Info() << Log::Resources << "Scene bounding box:" << std::endl
				<< "\t\tmini: " << bbox.minis << std::endl
				<< "\t\tmaxi: " << bbox.maxis << "." << std::endl;
	return bbox;
}

void Scene::update(double fullTime, double frameTime) {
	for(auto & light : lights) {
		light->update(fullTime, frameTime);
	}
	for(auto & object : objects) {
		object.update(fullTime, frameTime);
	}
	background->update(fullTime, frameTime);
}
