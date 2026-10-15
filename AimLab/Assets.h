#pragma once
#include "pch.h"
#include "Mesh.h"
#include "Texture.h"

// Scene가 소유하는 작은 캐시입니다. 같은 파일은 한 번만 로드합니다.
class Assets
{
public:
	std::shared_ptr<Mesh> LoadMesh(const std::string& path);
	std::shared_ptr<Texture> LoadTexture(const std::string& path);

private:
	std::unordered_map<std::string, std::shared_ptr<Mesh>>	  meshes_;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
};
