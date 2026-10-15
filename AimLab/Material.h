#pragma once
#include "pch.h"
#include "Shader.h"
#include "Texture.h"

// 여러 Object가 공유할 수 있습니다. 개별 색상이 필요하면 Material을 따로 만듭니다.
class Material
{
public:
	explicit Material(std::shared_ptr<Shader> shaderProgram) : shader(std::move(shaderProgram)) {}

	void Apply() const;

	std::shared_ptr<Shader>	 shader;
	std::shared_ptr<Texture> texture;
	glm::vec3				 color{1.0f};
	float					 shininess = 32.0f;
	bool					 useTexture = true;
	bool					 unlit = false;
	bool					 ambientOnly = false;
	bool					 doubleSided = false;
};
