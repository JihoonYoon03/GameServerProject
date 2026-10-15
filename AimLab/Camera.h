#pragma once
#include "pch.h"

class Camera
{
public:
	glm::vec3 position{0.0f, 1.75f, 1.0f};
	float	  yaw = 0.0f;
	float	  pitch = 0.0f;
	float	  fieldOfView = 60.0f;
	float	  nearPlane = 0.1f;
	float	  farPlane = 200.0f;
	glm::mat4 GetRotation() const;
	glm::vec3 GetForward() const;
	glm::mat4 GetView() const;
	glm::mat4 GetProjection(float aspect) const;
};
