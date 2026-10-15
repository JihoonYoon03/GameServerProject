#pragma once
#include "pch.h"

enum class LightType
{
	Directional = 0,
	Point = 1,
	Spot = 2
};

// 세 종류의 공통 데이터입니다. 상속 없이 type으로 셰이더 계산을 선택합니다.
struct Light
{
	static constexpr int MaxCount = 8; // Shaders/Lit.frag의 MAX_LIGHTS와 일치
	LightType			 type = LightType::Directional;
	glm::vec3			 color{1.0f};
	float				 intensity = 1.0f;
	glm::vec3			 position{0.0f};
	glm::vec3			 direction{0.0f, -1.0f, 0.0f}; // 빛이 진행하는 방향
	float				 range = 10.0f;				   // 0이면 거리 감쇠 없음 (원본 흰색 광원)
	float				 innerAngle = 15.0f;		   // Spot 중심각의 절반, 도 단위
	float				 outerAngle = 25.0f;
	bool				 enabled = true;

	static Light Directional(const glm::vec3& direction, const glm::vec3& color, float intensity);
	static Light Point(const glm::vec3& position, const glm::vec3& color, float intensity, float range);
	static Light Spot(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float range, float innerAngle, float outerAngle);
};
