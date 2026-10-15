#pragma once
#include "pch.h"
#include "Mesh.h"
#include "Material.h"

struct Transform
{
	glm::vec3 position{0.0f};
	glm::vec3 rotation{0.0f}; // 도 단위: X(pitch), Y(yaw), Z(roll)
	glm::vec3 scale{1.0f};	  // 각 축은 0이 아닌 값을 사용합니다.
	glm::mat4 GetMatrix() const;
};

// Scene가 Object를 소유하고, Object는 Mesh와 Material을 공유합니다.
class Object
{
public:
	Object(std::shared_ptr<Mesh> mesh = nullptr, std::shared_ptr<Material> material = nullptr);
	virtual ~Object() = default;
	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	virtual void Update(float deltaTime) { (void)deltaTime; }
	uint32_t GetId() const { return id_; }
	const std::shared_ptr<Mesh>& GetMesh() const { return mesh_; }
	const std::shared_ptr<Material>& GetMaterial() const { return material_; }
	virtual glm::mat4 GetModelMatrix() const { return transform.GetMatrix(); }

	Box GetWorldBox() const;
	Sphere GetWorldSphere() const;

	std::string	 name;
	Transform	 transform;
	ColliderType collider = ColliderType::None;
	bool		 active = true;	 // false이면 업데이트, 렌더링, 충돌 모두 제외
	bool		 visible = true; // 로컬 플레이어처럼 업데이트만 할 객체에 사용
	bool		 blocksMovement = false;

private:
	friend class Scene;
	uint32_t				  id_ = 0; // Scene 내부 식별자. 네트워크 ID 매핑은 서버 계층에서 관리합니다.
	std::shared_ptr<Mesh>	  mesh_;
	std::shared_ptr<Material> material_;
};
