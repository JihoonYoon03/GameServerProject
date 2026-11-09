#pragma once
#include "pch.h"
#include "Mesh.h"
#include "Material.h"

//Scene이 Object를 소유, Object는 Mesh와 Material을 공유
class Object
{
public:
	Object(std::shared_ptr<Mesh> mesh = nullptr, std::shared_ptr<Material> material = nullptr);
	virtual ~Object() = default;
	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	virtual void Update(float deltaTime) { (void)deltaTime; }
	uint32_t GetId() const { return m_nId; }
	const std::shared_ptr<Mesh>& GetMesh() const { return m_spMesh; }
	const std::shared_ptr<Material>& GetMaterial() const { return m_spMaterial; }
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
	uint32_t				  m_nId = 0; // Scene 내부 식별자. 네트워크 ID 매핑은 서버에서 관리
	std::shared_ptr<Mesh>	  m_spMesh;
	std::shared_ptr<Material> m_spMaterial;
};
