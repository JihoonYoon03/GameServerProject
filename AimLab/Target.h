#pragma once
#include "pch.h"
#include "Object.h"

class Target : public Object
{
public:
	using Object::Object;

	void Update(float deltaTime) override
	{
		if (pendingDestroy_ || deltaTime <= 0.0f) return;

		elapsedTime_ += deltaTime;

		if (elapsedTime_ >= lifetime_)
		{
			pendingDestroy_ = true;
		}
	}

	bool IsPendingDestroy() const { return pendingDestroy_; }

	bool Hit()
	{
		if (!active || pendingDestroy_) return false;

		pendingDestroy_ = true;
		return true;
	}

private:
	// 삭제 여부는 Target이 결정하고, 실제 해제는 Scene의 순회가 끝난 뒤 처리합니다.
	float elapsedTime_ = 0.0f;
	float lifetime_ = 3.0f;
	bool pendingDestroy_ = false;
};
