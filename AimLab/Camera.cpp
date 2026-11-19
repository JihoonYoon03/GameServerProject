#include "pch.h"
#include "Camera.h"

glm::mat4 Camera::GetRotation() const
{
	glm::quat pitchRotation = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));
	glm::quat yawRotation = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));

	return glm::mat4_cast(yawRotation * pitchRotation);
}

glm::vec3 Camera::GetForward() const
{
	return glm::vec3(GetRotation() * glm::vec4(0, 0, -1, 0));
}

glm::mat4 Camera::GetView(bool rotationOnly) const
{
	glm::mat4 retval = glm::lookAt(position, position + GetForward(), glm::vec3(0, 1, 0));
	if (rotationOnly)
	{
		//회전 성분만 남기기 위해 이동 역변환을 먼저 곱함
		retval = retval * glm::translate(glm::mat4(1.0f), -position);
	}

	return retval;
}

glm::mat4 Camera::GetProjection(float aspect) const
{
	return glm::perspective(glm::radians(fieldOfView), aspect, nearPlane, farPlane);
}
