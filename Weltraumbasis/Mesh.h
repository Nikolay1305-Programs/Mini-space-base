#pragma once
#include <vector>
#include <cstddef>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 nrm;
    glm::vec2 uv;
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& v, const std::vector<unsigned>& i, GLuint diffuseTex);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

    void Draw(GLuint program) const;

private:
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    GLuint diffuse = 0;
};
