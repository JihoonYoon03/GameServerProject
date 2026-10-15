#pragma once
#include "pch.h"
#include "Assets.h"
#include "Object.h"
#include "Camera.h"
#include "Light.h"
#include "Input.h"

struct Hud
{
	std::vector<std::string> lines;
	std::string				 message;
	bool					 crosshair = true;
	float					 messageScale = 0.0015f;
};

class Scene
{
public:
	virtual ~Scene() = default;
	virtual void Initialize() = 0;
	virtual void Update(float deltaTime, const Input& input);

	virtual Hud GetHud() const { return {}; }

	Object* AddObject(std::unique_ptr<Object> object);
	Object* FindObject(uint32_t id) const;
	bool RemoveObject(uint32_t id);

	const std::vector<std::unique_ptr<Object>>& GetObjects() const { return objects_; }
	Camera& GetCamera() { return camera_; }
	const Camera& GetCamera() const { return camera_; }
	const std::vector<Light>& GetLights() const { return lights_; }

	bool	  debugColliders = false;
	glm::vec3 ambientLight{0.3f};

protected:
	// Scene가 파괴될 때 Object, 캐시 순으로 소멸합니다.
	Assets								 assets_;
	std::vector<std::unique_ptr<Object>> objects_;
	Camera								 camera_;
	std::vector<Light>					 lights_;

private:
	uint32_t nextObjectId_ = 1;
};
