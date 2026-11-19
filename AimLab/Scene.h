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
	Object* AddUIObject(std::unique_ptr<Object> object);
	Object* FindObject(uint32_t id) const;
	Object* FindUIObject(uint32_t id) const;
	bool RemoveObject(uint32_t id);
	bool RemoveUIObject(uint32_t id);

	const std::vector<std::unique_ptr<Object>>& GetObjects() const { return m_vObjects; }
	const std::vector<std::unique_ptr<Object>>& GetUIObjects() const { return m_vUIObjects; }
	Camera& GetCamera() { return camera_; }
	const Camera& GetCamera() const { return camera_; }
	const std::vector<Light>& GetLights() const { return lights_; }

	bool	  debugColliders = false;
	glm::vec3 ambientLight{0.3f};

protected:
	// Scene가 파괴될 때 Object, 캐시 순으로 소멸합니다.
	Assets	assets_;

	//일반 오브젝트
	std::vector<std::unique_ptr<Object>> m_vObjects;
	
	//UI 오브젝트
	std::vector<std::unique_ptr<Object>> m_vUIObjects;
	
	Camera								 camera_;
	std::vector<Light>					 lights_;

private:
	uint32_t nextObjectId_ = 1;
	uint32_t nextUIObjectId_ = 1;
};
