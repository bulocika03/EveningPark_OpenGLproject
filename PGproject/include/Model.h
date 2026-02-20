#ifndef Model_h
#define Model_h

#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include "TextureLoader.h"
#include "Shader.h"

struct Material {
    std::string name;
    glm::vec3 diffuseColor;
    glm::vec3 emissionColor;
    GLuint textureID;
    bool hasTexture;
    bool hasEmission;

    Material() : diffuseColor(1.0f, 1.0f, 1.0f), emissionColor(0.0f, 0.0f, 0.0f), 
                 textureID(0), hasTexture(false), hasEmission(false) {}
};

struct MaterialGroup {
    std::string materialName;
    GLuint VAO, VBO;
    std::vector<float> vertices;
    int vertexCount;

    MaterialGroup() : VAO(0), VBO(0), vertexCount(0) {}
};

class Model
{
public:
    std::vector<MaterialGroup> materialGroups;
    std::map<std::string, Material> materials;
    std::string modelDirectory;
    bool hasTexture;

    Model() : hasTexture(false) {}

    bool loadOBJ(const std::string& path)
    {
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            modelDirectory = path.substr(0, lastSlash + 1);
        }

        std::vector<glm::vec3> temp_positions;
        std::vector<glm::vec2> temp_texcoords;
        std::vector<glm::vec3> temp_normals;

        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cout << "Failed to open OBJ file: " << path << std::endl;
            return false;
        }

        std::string mtlFile = "";
        std::string line;

        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "mtllib") {
                iss >> mtlFile;
                loadMTL(modelDirectory + mtlFile);
                break;
            }
        }

        file.clear();
        file.seekg(0, std::ios::beg);

        std::string currentMaterialName = "";
        std::map<std::string, std::vector<float>> materialVertices;

        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "usemtl")
            {
                iss >> currentMaterialName;
            }
            else if (prefix == "v")
            {
                glm::vec3 position;
                iss >> position.x >> position.y >> position.z;
                temp_positions.push_back(position);
            }
            else if (prefix == "vt")
            {
                glm::vec2 texcoord;
                iss >> texcoord.x >> texcoord.y;
                temp_texcoords.push_back(texcoord);
            }
            else if (prefix == "vn")
            {
                glm::vec3 normal;
                iss >> normal.x >> normal.y >> normal.z;
                temp_normals.push_back(normal);
            }
            else if (prefix == "f")
            {
                if (currentMaterialName.empty()) {
                    currentMaterialName = "default";
                }

                std::vector<std::string> faceVertices;
                std::string vertex;
                while (iss >> vertex) {
                    faceVertices.push_back(vertex);
                }

                for (size_t i = 1; i + 1 < faceVertices.size(); i++)
                {
                    processFaceVertex(faceVertices[0], temp_positions, temp_texcoords, temp_normals,
                        materialVertices[currentMaterialName]);
                    processFaceVertex(faceVertices[i], temp_positions, temp_texcoords, temp_normals,
                        materialVertices[currentMaterialName]);
                    processFaceVertex(faceVertices[i + 1], temp_positions, temp_texcoords, temp_normals,
                        materialVertices[currentMaterialName]);
                }
            }
        }

        file.close();

        // Creează VAO/VBO pentru fiecare material
        for (auto& pair : materialVertices)
        {
            MaterialGroup group;
            group.materialName = pair.first;
            group.vertices = pair.second;
            group.vertexCount = pair.second.size() / 11;

            setupMaterialGroup(group);
            materialGroups.push_back(group);

            std::cout << "  - Material group: " << group.materialName
                << " (" << group.vertexCount << " vertices)" << std::endl;
        }

        std::cout << "OBJ loaded successfully:  " << path
            << " (" << materialGroups.size() << " material groups)" << std::endl;

        return true;
    }

    void draw(Shader& shader)
    {
        static bool debugOnce = true;
        
        for (auto& group : materialGroups)
        {
            // Bind textura materialului
            glActiveTexture(GL_TEXTURE0);
            
            auto it = materials.find(group.materialName);
            if (it != materials.end())
            {
                Material& mat = it->second;
                
                // Setează emission
                shader.setBool("hasEmission", mat.hasEmission);
                shader.setVec3("emissionColor", mat.emissionColor);
                
                if (mat.hasTexture && mat.textureID != 0) {
                    if (debugOnce) {
                        std::cout << "Drawing " << group.materialName << " with texture ID " << mat.textureID << std::endl;
                    }
                    glBindTexture(GL_TEXTURE_2D, mat.textureID);
                    shader.setInt("diffuseTexture", 0);
                    shader.setBool("useTexture", true);
                    shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
                }
                else {
                    if (debugOnce) {
                        std::cout << "Drawing " << group.materialName << " without texture, color: " 
                                  << mat.diffuseColor.r << "," << mat.diffuseColor.g << "," << mat.diffuseColor.b 
                                  << " emission: " << mat.hasEmission << std::endl;
                    }
                    glBindTexture(GL_TEXTURE_2D, 0);
                    shader.setBool("useTexture", false);
                    shader.setVec3("objectColor", mat.diffuseColor);
                }
            }
            else {
                if (debugOnce) {
                    std::cout << "Drawing " << group.materialName << " - material NOT FOUND!" << std::endl;
                }
                glBindTexture(GL_TEXTURE_2D, 0);
                shader.setBool("useTexture", false);
                shader.setBool("hasEmission", false);
                shader.setVec3("emissionColor", glm::vec3(0.0f, 0.0f, 0.0f));
                shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
            }

            glBindVertexArray(group.VAO);
            glDrawArrays(GL_TRIANGLES, 0, group.vertexCount);
            glBindVertexArray(0);
        }
        
        debugOnce = false;
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    int vertexCount() const {
        int total = 0;
        for (const auto& group : materialGroups) {
            total += group.vertexCount;
        }
        return total;
    }

    // Desenează doar un anumit material group (pentru animații pe componente)
    void drawMaterialGroup(Shader& shader, const std::string& materialName)
    {
        for (auto& group : materialGroups)
        {
            if (group.materialName != materialName) continue;
            
            glActiveTexture(GL_TEXTURE0);
            
            auto it = materials.find(group.materialName);
            if (it != materials.end())
            {
                Material& mat = it->second;
                shader.setBool("hasEmission", mat.hasEmission);
                shader.setVec3("emissionColor", mat.emissionColor);
                
                if (mat.hasTexture && mat.textureID != 0) {
                    glBindTexture(GL_TEXTURE_2D, mat.textureID);
                    shader.setInt("diffuseTexture", 0);
                    shader.setBool("useTexture", true);
                    shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
                }
                else {
                    glBindTexture(GL_TEXTURE_2D, 0);
                    shader.setBool("useTexture", false);
                    shader.setVec3("objectColor", mat.diffuseColor);
                }
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
                shader.setBool("useTexture", false);
                shader.setBool("hasEmission", false);
                shader.setVec3("emissionColor", glm::vec3(0.0f, 0.0f, 0.0f));
                shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
            }

            glBindVertexArray(group.VAO);
            glDrawArrays(GL_TRIANGLES, 0, group.vertexCount);
            glBindVertexArray(0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Desenează toate componentele EXCEPTÂND un anumit material
    void drawExcept(Shader& shader, const std::string& excludeMaterial)
    {
        for (auto& group : materialGroups)
        {
            if (group.materialName == excludeMaterial) continue;
            
            glActiveTexture(GL_TEXTURE0);
            
            auto it = materials.find(group.materialName);
            if (it != materials.end())
            {
                Material& mat = it->second;
                shader.setBool("hasEmission", mat.hasEmission);
                shader.setVec3("emissionColor", mat.emissionColor);
                
                if (mat.hasTexture && mat.textureID != 0) {
                    glBindTexture(GL_TEXTURE_2D, mat.textureID);
                    shader.setInt("diffuseTexture", 0);
                    shader.setBool("useTexture", true);
                    shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
                }
                else {
                    glBindTexture(GL_TEXTURE_2D, 0);
                    shader.setBool("useTexture", false);
                    shader.setVec3("objectColor", mat.diffuseColor);
                }
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
                shader.setBool("useTexture", false);
                shader.setBool("hasEmission", false);
                shader.setVec3("emissionColor", glm::vec3(0.0f, 0.0f, 0.0f));
                shader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
            }

            glBindVertexArray(group.VAO);
            glDrawArrays(GL_TRIANGLES, 0, group.vertexCount);
            glBindVertexArray(0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

private:
void loadMTL(const std::string& mtlPath)
{
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        std::cout << "Could not open MTL file: " << mtlPath << std::endl;
        return;
    }

    std::cout << "Loading MTL file: " << mtlPath << std::endl;

    std::string line;
    Material currentMtl;

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "newmtl")
        {
            if (!currentMtl.name.empty()) {
                materials[currentMtl.name] = currentMtl;
                std::cout << "  Added material: " << currentMtl.name 
                          << " (hasTexture: " << currentMtl.hasTexture 
                          << ", textureID: " << currentMtl.textureID << ")" << std::endl;
            }
            currentMtl = Material();
            iss >> currentMtl.name;
        }
        else if (prefix == "Kd")
        {
            iss >> currentMtl.diffuseColor.r >> currentMtl.diffuseColor.g >> currentMtl.diffuseColor.b;
        }
        else if (prefix == "Ke")
        {
            iss >> currentMtl.emissionColor.r >> currentMtl.emissionColor.g >> currentMtl.emissionColor.b;
            // Marcăm ca emissiv doar dacă are emission și NU are textură 
            // (becurile de lampă au Kd=0 și Ke=1, nu au map_Kd)
            // Vom decide final după ce vedem dacă are map_Kd
        }
        else if (prefix == "map_Kd")
        {
            std::string textureName;
            iss >> textureName;

            std::string texturePath = modelDirectory + textureName;
            std::cout << "  Attempting to load texture: " << texturePath << std::endl;
            currentMtl.textureID = TextureLoader::LoadTexture(texturePath.c_str());

            if (currentMtl.textureID != 0) {
                currentMtl.hasTexture = true;
                hasTexture = true;
                std::cout << "  SUCCESS: Loaded texture ID: " << currentMtl.textureID << std::endl;
            }
            else {
                std::cout << "  FAILED: Could not load texture" << std::endl;
            }
        }
    }

    if (!currentMtl.name.empty()) {
        // Decide dacă materialul este cu adevărat emissiv (bec de lampă)
        // Emissiv = are Ke > 0 și NU are textură și Kd este foarte mic (aproape negru)
        float emissionStrength = currentMtl.emissionColor.r + currentMtl.emissionColor.g + currentMtl.emissionColor.b;
        float diffuseStrength = currentMtl.diffuseColor.r + currentMtl.diffuseColor.g + currentMtl.diffuseColor.b;
        if (emissionStrength > 0.1f && !currentMtl.hasTexture && diffuseStrength < 0.1f) {
            currentMtl.hasEmission = true;
            std::cout << "  Material " << currentMtl.name << " marked as EMISSIVE (light bulb)" << std::endl;
        }
        
        materials[currentMtl.name] = currentMtl;
        std::cout << "  Added material: " << currentMtl.name 
                  << " (hasTexture: " << currentMtl.hasTexture 
                  << ", textureID: " << currentMtl.textureID << ")" << std::endl;
    }

    file.close();
    
    // Post-procesare: verifică toate materialele pentru emission
    for (auto& pair : materials) {
        Material& mat = pair.second;
        float emissionStrength = mat.emissionColor.r + mat.emissionColor.g + mat.emissionColor.b;
        float diffuseStrength = mat.diffuseColor.r + mat.diffuseColor.g + mat.diffuseColor.b;
        if (emissionStrength > 0.1f && !mat.hasTexture && diffuseStrength < 0.1f) {
            mat.hasEmission = true;
        }
    }
    
    std::cout << "MTL loading complete. Total materials: " << materials.size() << std::endl;
}

    void processFaceVertex(const std::string& vertexStr,
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec2>& texcoords,
        const std::vector<glm::vec3>& normals,
        std::vector<float>& outVertices)
    {
        std::istringstream iss(vertexStr);
        std::string indexStr;

        int posIdx = -1, texIdx = -1, normIdx = -1;

        if (std::getline(iss, indexStr, '/')) {
            if (!indexStr.empty()) posIdx = std::stoi(indexStr) - 1;
        }
        if (std::getline(iss, indexStr, '/')) {
            if (!indexStr.empty()) texIdx = std::stoi(indexStr) - 1;
        }
        if (std::getline(iss, indexStr, '/')) {
            if (!indexStr.empty()) normIdx = std::stoi(indexStr) - 1;
        }

        // Position
        if (posIdx >= 0 && posIdx < positions.size()) {
            outVertices.push_back(positions[posIdx].x);
            outVertices.push_back(positions[posIdx].y);
            outVertices.push_back(positions[posIdx].z);
        }
        else {
            outVertices.push_back(0.0f);
            outVertices.push_back(0.0f);
            outVertices.push_back(0.0f);
        }

        // Normal
        if (normIdx >= 0 && normIdx < normals.size()) {
            outVertices.push_back(normals[normIdx].x);
            outVertices.push_back(normals[normIdx].y);
            outVertices.push_back(normals[normIdx].z);
        }
        else {
            outVertices.push_back(0.0f);
            outVertices.push_back(1.0f);
            outVertices.push_back(0.0f);
        }

        // Texture coords
        if (texIdx >= 0 && texIdx < texcoords.size()) {
            outVertices.push_back(texcoords[texIdx].x);
            outVertices.push_back(texcoords[texIdx].y);
        }
        else {
            outVertices.push_back(0.0f);
            outVertices.push_back(0.0f);
        }

        // Color (white - will use texture)
        outVertices.push_back(1.0f);
        outVertices.push_back(1.0f);
        outVertices.push_back(1.0f);
    }

    void setupMaterialGroup(MaterialGroup& group)
    {
        glGenVertexArrays(1, &group.VAO);
        glGenBuffers(1, &group.VBO);

        glBindVertexArray(group.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, group.VBO);
        glBufferData(GL_ARRAY_BUFFER, group.vertices.size() * sizeof(float),
            group.vertices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Texture coords
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // Color
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);
    }
};

#endif