#include "pch.h"
#include "Scene.h"

void Scene::Update(float deltaTime, const Input&)
{
	// 순회 중 추가/삭제하지 않습니다. 목록 변경은 이 함수 호출 전후에 처리합니다.

	for (std::unique_ptr<Object>& object : m_vObjects)
	{
		if (object->active)
		{
			object->Update(deltaTime);
		}
	}
}

Object* Scene::AddObject(std::unique_ptr<Object> object)
{
	if (!object)
	{
		throw std::invalid_argument("Cannot add a null object");
	}

	object->m_nId = nextObjectId_++;
	Object* result = object.get();
	m_vObjects.push_back(std::move(object));

	return result;
}

Object* Scene::AddUIObject(std::unique_ptr<Object> object)
{
	if (!object)
	{
		throw std::invalid_argument("Cannot add a null object");
	}

	object->m_nId = nextUIObjectId_++;
	Object* result = object.get();
	m_vUIObjects.push_back(std::move(object));

	return result;
}

Object* Scene::FindObject(uint32_t id) const
{
	for (const std::unique_ptr<Object>& object : m_vObjects)
	{
		if (object->GetId() == id) return object.get();
	}

	return nullptr;
}

Object* Scene::FindUIObject(uint32_t id) const
{
	for (const std::unique_ptr<Object>& object : m_vUIObjects)
	{
		if (object->GetId() == id) return object.get();
	}

	return nullptr;
}

bool Scene::RemoveObject(uint32_t id)
{
	auto found = std::find_if(m_vObjects.begin(), m_vObjects.end(), [id](const std::unique_ptr<Object>& object)
		{ return object->GetId() == id; });

	if (found == m_vObjects.end()) return false;

	m_vObjects.erase(found);

	return true;
}

bool Scene::RemoveUIObject(uint32_t id)
{
	auto found = std::find_if(m_vUIObjects.begin(), m_vUIObjects.end(), [id](const std::unique_ptr<Object>& object)
		{ return object->GetId() == id; });

	if (found == m_vUIObjects.end()) return false;

	m_vUIObjects.erase(found);

	return true;
}
