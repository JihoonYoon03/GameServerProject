#include "pch.h"
#include "Light.h"

Light Light::Directional(const glm::vec3& direction, const glm::vec3& color, float intensity)
{
	Light light;
	light.direction = direction;
	light.color = color;
	light.intensity = intensity;

	return light;
}

Light Light::Point(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
{
	Light light;
	light.type = LightType::Point;
	light.position = position;
	light.color = color;
	light.intensity = intensity;
	light.range = range;

	return light;
}

Light Light::Spot(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float range, float innerAngle, float outerAngle)
{
	Light light = Point(position, color, intensity, range);
	light.type = LightType::Spot;
	light.direction = direction;
	light.outerAngle = std::clamp(outerAngle, 0.1f, 89.0f);
	light.innerAngle = std::clamp(innerAngle, 0.0f, light.outerAngle - 0.1f);

	return light;
}
