#pragma once
#include "pch.h"
#include "Collision.h"

struct Vertex
{
	glm::vec3 position{0.0f};
	glm::vec3 normal{0.0f, 1.0f, 0.0f};
	glm::vec2 texCoord{0.0f};
};

// 정점과 GPU 버퍼를 소유합니다. 위치, 색, 게임 규칙은 저장하지 않습니다.
class Mesh
{
public:
	explicit Mesh(const std::string& filename);
	explicit Mesh(std::vector<Vertex> vertices);
	~Mesh();
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	void Draw() const;

	const std::vector<Vertex>& GetVertices() const { return vertices_; }
	const Box& GetBounds() const { return bounds_; }

private:
	void Upload();

	std::vector<Vertex> vertices_;
	Box					bounds_;
	GLuint				vao_ = 0;
	GLuint				vbo_ = 0;
};
