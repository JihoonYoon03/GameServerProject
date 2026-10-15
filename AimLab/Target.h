#pragma once
#include "pch.h"
#include "Object.h"

class Target : public Object
{
public:
	using Object::Object;

	bool Hit()
	{
		if (!active) return false;

		active = false;
		return true;
	}
};
