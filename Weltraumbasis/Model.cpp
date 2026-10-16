#include "Model.h"
#include "Texture.h"

#include <iostream>
#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


static std::string DirOf(const std::string& p)
{
    return std::filesystem::path(p).parent_path().string();
}


static GLuint MakeFallbackWhite()
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    unsigned char px[4] = { 255,255,255,255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}


static const aiTexture* GetEmbeddedTexture(const aiScene* scene, const aiString& texPath)
{
    if (!scene) return nullptr;

    const char* s = texPath.C_Str();
    if (!s || s[0] != '*') return nullptr;

    int idx = -1;
    try { idx = std::stoi(std::string(s + 1)); }
    catch (...) { return nullptr; }

    if (idx < 0 || (unsigned)idx >= scene->mNumTextures) return nullptr;
    return scene->mTextures[idx];
}

bool Model::Load(const std::string& path)
{
    meshes.clear(); 

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |                 
        aiProcess_GenNormals |                 
        aiProcess_FlipUVs |                     
        aiProcess_JoinIdenticalVertices         
    );

    if (!scene || !scene->mRootNode) {
        std::cout << "Assimp load failed: " << path << "\n"
            << importer.GetErrorString() << "\n";
        return false;
    }

    const std::string dir = DirOf(path);        
    ProcessNode(scene->mRootNode, scene, dir);  

    if (meshes.empty()) {
        std::cout << "Model loaded but no meshes: " << path << "\n";
        return false;
    }
    return true;
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, const std::string& dir)
{
    
    for (unsigned i = 0; i < node->mNumMeshes; i++) {
        aiMesh* m = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(ProcessMesh(m, scene, dir));
    }

    
    for (unsigned i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, dir);
    }
}

Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& dir)
{
    
    std::vector<Vertex> verts;
    verts.reserve(mesh->mNumVertices);

    for (unsigned i = 0; i < mesh->mNumVertices; i++) {
        Vertex v{};

        
        v.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

        
        if (mesh->HasNormals())
            v.nrm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
        else
            v.nrm = { 0,1,0 };

        
        if (mesh->mTextureCoords[0])
            v.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        else
            v.uv = { 0,0 };

        verts.push_back(v);
    }

    
    std::vector<unsigned> idx;
    idx.reserve(mesh->mNumFaces * 3);

    for (unsigned f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        for (unsigned j = 0; j < face.mNumIndices; j++)
            idx.push_back(face.mIndices[j]);
    }

    
    GLuint diffuseTex = 0;

    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {

            
            if (const aiTexture* emb = GetEmbeddedTexture(scene, texPath)) {

                
                if (emb->mHeight == 0 && emb->mWidth > 0 && emb->pcData) {
                    diffuseTex = LoadTexture2D_FromMemory(
                        (const unsigned char*)emb->pcData,
                        (size_t)emb->mWidth
                    );
                }
            }
            else {
                
                std::string full = dir + "/" + std::string(texPath.C_Str());
                diffuseTex = LoadTexture2D_RGBA(full);
            }
        }
    }

    
    if (!diffuseTex) diffuseTex = MakeFallbackWhite();

    
    return Mesh(verts, idx, diffuseTex);
}

void Model::Draw(GLuint program) const
{
    
    for (const auto& m : meshes)
        m.Draw(program);
}
