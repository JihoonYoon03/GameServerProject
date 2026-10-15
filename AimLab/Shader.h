#pragma once
#include "pch.h"

class Shader
{
public:
	Shader(const std::string& vertexPath, const std::string& fragmentPath);
	~Shader();
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	void Use() const;
	void Set(const std::string& name, int value) const;
	void Set(const std::string& name, float value) const;
	void Set(const std::string& name, const glm::vec3& value) const;
	void Set(const std::string& name, const glm::mat4& value) const;

private:
	GLint Location(const std::string& name) const;
	GLuint										   program_ = 0;
	mutable std::unordered_map<std::string, GLint> locations_;
};
