#include "pch.h"
#include "Object.h"

Object::Object(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
	: m_spMesh(std::move(mesh)), m_spMaterial(std::move(material))
{
}

Box Object::GetWorldBox() const
{
	const Box local = m_spMesh ? m_spMesh->GetBounds() : Box{glm::vec3(-0.5f), glm::vec3(0.5f)};
	return Collision::GetWorldBox(local, transform);
}

Sphere Object::GetWorldSphere() const
{
	const Box local = m_spMesh ? m_spMesh->GetBounds() : Box{glm::vec3(-0.5f), glm::vec3(0.5f)};
	return Collision::GetWorldSphere(local, transform);
}
