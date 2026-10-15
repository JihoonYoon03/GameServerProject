#include "pch.h"
#include "Assets.h"

std::shared_ptr<Mesh> Assets::LoadMesh(const std::string& path)
{
	const auto found = meshes_.find(path);

	if (found != meshes_.end()) return found->second;

	auto mesh = std::make_shared<Mesh>(path);
	meshes_[path] = mesh;

	return mesh;
}

std::shared_ptr<Texture> Assets::LoadTexture(const std::string& path)
{
	const auto found = textures_.find(path);

	if (found != textures_.end()) return found->second;

	auto texture = std::make_shared<Texture>(path);
	textures_[path] = texture;

	return texture;
}
