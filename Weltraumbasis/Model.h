#pragma once

#include <vector>
#include <string>
#include "Mesh.h"


struct aiNode;
struct aiScene;
struct aiMesh;

class Model {
public:
    bool Load(const std::string& path);          
    void Draw(GLuint program) const;             
    bool IsLoaded() const { return !meshes.empty(); }

private:
    std::vector<Mesh> meshes;                   

    void ProcessNode(aiNode* node, const aiScene* scene, const std::string& dir);
    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& dir);
};
