#include "Mesh.h"
#include <utility>

Mesh::Mesh(const std::vector<Vertex>& v, const std::vector<unsigned>& i, GLuint diffuseTex)
{
    diffuse = diffuseTex;
    indexCount = (GLsizei)i.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(Vertex)), v.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(i.size() * sizeof(unsigned)), i.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));

    glEnableVertexAttribArray(1); // nrm
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nrm));

    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (diffuse) glDeleteTextures(1, &diffuse);
}

Mesh::Mesh(Mesh&& o) noexcept { *this = std::move(o); }
Mesh& Mesh::operator=(Mesh&& o) noexcept
{
    if (this == &o) return *this;
    std::swap(vao, o.vao);
    std::swap(vbo, o.vbo);
    std::swap(ebo, o.ebo);
    std::swap(indexCount, o.indexCount);
    std::swap(diffuse, o.diffuse);
    return *this;
}

void Mesh::Draw(GLuint program) const
{

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, diffuse);
    glUniform1i(glGetUniformLocation(program, "uDiffuse"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}
