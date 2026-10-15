#include "pch.h"
#include "Scene.h"

void Scene::Update(float deltaTime, const Input&)
{
	// 순회 중 추가/삭제하지 않습니다. 목록 변경은 이 함수 호출 전후에 처리합니다.

	for (auto& object : objects_)
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

	object->id_ = nextObjectId_++;
	Object* result = object.get();
	objects_.push_back(std::move(object));

	return result;
}

Object* Scene::FindObject(uint32_t id) const
{
	for (const auto& object : objects_)
	{
		if (object->GetId() == id) return object.get();
	}

	return nullptr;
}

bool Scene::RemoveObject(uint32_t id)
{
	const auto found = std::find_if(objects_.begin(), objects_.end(), [id](const auto& object)
		{ return object->GetId() == id; });

	if (found == objects_.end()) return false;

	objects_.erase(found);

	return true;
}
