#pragma once
#include "pch.h"

class Texture
{
public:
	explicit Texture(const std::string& filename);
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	void Bind() const;

private:
	GLuint id_ = 0;
};
