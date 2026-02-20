#if defined (__APPLE__)
    #define GLFW_INCLUDE_GLCOREARB
#else
    #define GLEW_STATIC
    #include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "include/Shader.h"
#include "include/Camera.h"
#include "include/TextureLoader.h"
#include "include/Model.h"
#include "include/Skybox.h"
#include <vector>
#include <cstdlib>

const int GL_WINDOW_WIDTH = 1280;
const int GL_WINDOW_HEIGHT = 720;

GLFWwindow* glWindow = NULL;

Camera camera(glm::vec3(0.0f, 5.0f, 15.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -20.0f);
float lastX = GL_WINDOW_WIDTH / 2.0f;
float lastY = GL_WINDOW_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

Shader basicShader;
Shader shadowShader;
GLuint groundVAO, groundVBO, groundEBO;
GLuint pavementTexture;

GLuint shadowMapFBO;
GLuint shadowMapTexture;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
glm::mat4 lightSpaceMatrix;

Model* benchModel;
Model* lampModel;
Model* spruceTreeModel;
Model* pineTreeModel;
Model* oakTreeModel;
Model* lindenTreeModel;
Model* angelStatueModel;
Model* lamp12Model;
Model* bunnyTruckModel;
Model* moonModel;

Skybox* skybox;


float moonRotation = 0.0f;

// Animation variables
float lampFlickerTime = 0.0f;
float treeSwayTime = 0.0f;
float cottonCandyRotation = 0.0f; // Rotația paharului de vată de zahăr

// Render mode: 0=solid, 1=wireframe, 2=points, 3=smooth
int renderMode = 0;

// Weather effects
bool fogEnabled = false;
bool rainEnabled = false;
float windStrength = 2.0f; // Puterea vântului
float windTime = 0.0f;

// Rain particle system
const int MAX_RAIN_PARTICLES = 5000;
struct RainParticle {
    glm::vec3 position;
    float speed;
};
std::vector<RainParticle> rainParticles;
GLuint rainVAO, rainVBO;
Shader rainShader;

// Collision detection
std::vector<AABB> sceneColliders;


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void createGround();
void initShadowMap();
void renderSceneDepth(Shader& shader);
void initRainSystem();
void updateRainParticles(float deltaTime);
void renderRain(glm::mat4 view, glm::mat4 projection);
void initColliders();

bool initOpenGLWindow()
{
    if (!glfwInit()) {
        fprintf(stderr, "ERROR: could not start GLFW3\n");
        return false;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glWindow = glfwCreateWindow(GL_WINDOW_WIDTH, GL_WINDOW_HEIGHT, "Parc de Distractii - Seara", NULL, NULL);
    if (!glWindow) {
        fprintf(stderr, "ERROR: could not open window with GLFW3\n");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(glWindow);
    glfwSetFramebufferSizeCallback(glWindow, framebuffer_size_callback);
    glfwSetCursorPosCallback(glWindow, mouse_callback);
    glfwSetScrollCallback(glWindow, scroll_callback);
    
    glfwSetInputMode(glWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

#if ! defined (__APPLE__)
    glewExperimental = GL_TRUE;
    glewInit();
#endif
    
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    printf("Renderer: %s\n", renderer);
    printf("OpenGL version supported %s\n", version);
    
    glEnable(GL_DEPTH_TEST);
    
    glEnable(GL_CULL_FACE);

    return true;
}

void initShadowMap()
{
    // Creează framebuffer pentru shadow map
    glGenFramebuffers(1, &shadowMapFBO);
    
    // Creează textura pentru depth map
    glGenTextures(1, &shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    // Atașează textura la framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    std::cout << "Shadow map initialized (" << SHADOW_WIDTH << "x" << SHADOW_HEIGHT << ")" << std::endl;
}


void renderScene() {

// Calculează light space matrix (din perspectiva lunii)
glm::vec3 lightPosition = glm::vec3(15.0f, 35.0f, -30.0f); // Poziția lunii
float near_plane = 1.0f, far_plane = 100.0f;
glm::mat4 lightProjection = glm::ortho(-60.0f, 60.0f, -60.0f, 60.0f, near_plane, far_plane);
glm::mat4 lightView = glm::lookAt(lightPosition, glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
lightSpaceMatrix = lightProjection * lightView;

// Render to shadow map
shadowShader.useShaderProgram();
shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
glClear(GL_DEPTH_BUFFER_BIT);
glCullFace(GL_FRONT);
renderSceneDepth(shadowShader);
glCullFace(GL_BACK);
glBindFramebuffer(GL_FRAMEBUFFER, 0);

glViewport(0, 0, GL_WINDOW_WIDTH, GL_WINDOW_HEIGHT);

glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // Dark blue evening sky
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

glm::mat4 projection = glm::perspective(glm::radians(camera.Fov),
    (float)GL_WINDOW_WIDTH / (float)GL_WINDOW_HEIGHT,
    0.1f,
    500.0f);
glm::mat4 view = camera.GetViewMatrix();
glm::mat4 model = glm::mat4(1.0f);

// Draw skybox
if (skybox) {
    skybox->draw(view, projection);
}

basicShader.useShaderProgram();
basicShader.setMat4("projection", projection);
basicShader.setMat4("view", view);

// Fog uniforms
basicShader.setBool("fogEnabled", fogEnabled);
basicShader.setVec3("fogColor", glm::vec3(0.2f, 0.2f, 0.25f));
basicShader.setFloat("fogDensity", 0.004f);
basicShader.setVec3("cameraPos", camera.Position);

// Apply render mode
switch (renderMode) {
    case 0: // Solid
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        basicShader.setBool("smoothShading", false);
        break;
    case 1: // Wireframe
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        basicShader.setBool("smoothShading", false);
        break;
    case 2: // Points
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glPointSize(3.0f);
        basicShader.setBool("smoothShading", false);
        break;
    case 3: // Smooth
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        basicShader.setBool("smoothShading", true);
        break;
}

    // Set lighting uniforms
    basicShader.setVec3("lightPos", glm::vec3(15.0f, 35.0f, -30.0f)); // Poziția lunii pentru umbre
    basicShader.setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.9f)); // Lumină ambientală mai slabă
    basicShader.setVec3("viewPos", camera.Position);
    
    // Shadow mapping uniforms
    basicShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    basicShader.setBool("shadowsEnabled", true);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    basicShader.setInt("shadowMap", 1);
    
    // Set up point lights from lamps
    float lampHeight = 2.5f;
    
    // Lampă 1 pâlpâie
    float flicker = 0.7f + 0.3f * sin(lampFlickerTime) * sin(lampFlickerTime * 3.7f);
    
    glm::vec3 pointLightPositions[8] = {
        glm::vec3(-8.0f, lampHeight, -20.0f),  // Lampă 1 - pâlpâie
        glm::vec3(8.0f, lampHeight, -20.0f),   // Lampă 2
        glm::vec3(-8.0f, lampHeight, 0.0f),    // Lampă 3
        glm::vec3(8.0f, lampHeight, 0.0f),     // Lampă 4
        glm::vec3(8.0f, lampHeight, 20.0f),    // Lampă lângă Bunny Truck
        glm::vec3(-2.0f, 2.5f, -42.0f),        // Lamp12 stânga statuie (mai jos, lămpi mici)
        glm::vec3(2.0f, 2.5f, -42.0f),         // Lamp12 dreapta statuie
        glm::vec3(15.0f, 35.0f, -30.0f)        // Luna (lumină slabă)
    };
    
    glm::vec3 pointLightColors[8] = {
        glm::vec3(1.0f, 0.8f, 0.4f) * flicker * 4.0f,  // Lampă 1 - pâlpâie, galben cald
        glm::vec3(1.0f, 0.85f, 0.5f) * 4.0f,           // Lampă 2 - galben cald
        glm::vec3(1.0f, 0.85f, 0.5f) * 4.0f,           // Lampă 3
        glm::vec3(1.0f, 0.85f, 0.5f) * 4.0f,           // Lampă 4
        glm::vec3(1.0f, 0.85f, 0.5f) * 4.0f,           // Lampă Bunny Truck
        glm::vec3(1.0f, 0.7f, 0.3f) * 3.0f,            // Lamp12 stânga - lumină portocalie intensă
        glm::vec3(1.0f, 0.7f, 0.3f) * 3.0f,            // Lamp12 dreapta - lumină portocalie intensă
        glm::vec3(0.7f, 0.8f, 1.0f) * 4.0f             // Luna - lumină albăstruie puternică
    };
    
    // Trimite luminile punctuale la shader
    basicShader.setInt("numPointLights", 8);
    for (int i = 0; i < 8; i++) {
        std::string posName = "pointLightPos[" + std::to_string(i) + "]";
        std::string colorName = "pointLightColor[" + std::to_string(i) + "]";
        glUniform3fv(glGetUniformLocation(basicShader.shaderProgram, posName.c_str()), 1, glm::value_ptr(pointLightPositions[i]));
        glUniform3fv(glGetUniformLocation(basicShader.shaderProgram, colorName.c_str()), 1, glm::value_ptr(pointLightColors[i]));
    }

    glDisable(GL_CULL_FACE);
    
    model = glm::mat4(1.0f);
    basicShader.setMat4("model", model);
    basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
    basicShader.setBool("useTexture", true);
    basicShader.setBool("hasEmission", false);
    basicShader.setVec3("emissionColor", glm::vec3(0.0f, 0.0f, 0.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pavementTexture);
    basicShader.setInt("diffuseTexture", 0);

    glBindVertexArray(groundVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    
    glEnable(GL_CULL_FACE);

    // BUNNY COTTON CANDY TRUCK
    if (bunnyTruckModel && bunnyTruckModel->vertexCount() > 0) {
       
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 28.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
        model = glm::scale(model, glm::vec3(2.5f, 2.5f, 2.5f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        bunnyTruckModel->drawExcept(basicShader, "Material.003"); 
        
        float bobbing = sin(cottonCandyRotation * 2.0f) * 0.02f; 
        
        glm::mat4 bunnyModel = glm::mat4(1.0f);
        bunnyModel = glm::translate(bunnyModel, glm::vec3(8.0f, bobbing, 28.0f)); 
        bunnyModel = glm::rotate(bunnyModel, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
        bunnyModel = glm::scale(bunnyModel, glm::vec3(2.5f, 2.5f, 2.5f)); 
        
        basicShader.setMat4("model", bunnyModel);
        bunnyTruckModel->drawMaterialGroup(basicShader, "Material.003");
    }

    // Lampă lângă Bunny Truck
    if (lampModel && lampModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 20.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lampModel->draw(basicShader);
    }

    //BENCHES
    if (benchModel && benchModel->vertexCount() > 0) {

        // Bancă 1
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-8.0f, 0.0f, -30.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

        // Bancă 2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-8.0f, 0.0f, -10.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

        // Bancă 3
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, -30.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

        // Bancă 4
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, -10.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

        // Bancă 5
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-8.0f, 0.0f, 10.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

        // Bancă 6
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 10.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        benchModel->draw(basicShader);

    }

    // STREET LAMPS
    if (lampModel && lampModel->vertexCount() > 0) {

        // Lampă 1
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-8.0f, 0.0f, -20.0f));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lampModel->draw(basicShader);

        // Lampă 2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, -20.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lampModel->draw(basicShader);

        // Lampă 3
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-8.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lampModel->draw(basicShader);

        // Lampă 4
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lampModel->draw(basicShader);

    }

    //TREES (în spatele băncilor) 
    
    // 
    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 0.0f) * 2.0f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, -42.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 1.0f) * 1.8f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, -34.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 2.0f) * 1.5f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, -26.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 3.0f) * 2.2f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, -18.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 4.0f) * 1.7f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -44.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 5.0f) * 2.0f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -38.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        float sway = sin(treeSwayTime + 6.0f) * 1.6f;
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -30.0f));
        model = glm::rotate(model, glm::radians(sway), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -22.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -14.0f));
        model = glm::scale(model, glm::vec3(0.78f, 0.78f, 0.78f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-26.0f, 0.0f, -40.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-26.0f, 0.0f, -28.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-26.0f, 0.0f, -18.0f));
        model = glm::scale(model, glm::vec3(0.82f, 0.82f, 0.82f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, -42.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, -34.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, -26.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, -18.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -44.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -38.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -30.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -22.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -14.0f));
        model = glm::scale(model, glm::vec3(0.75f, 0.75f, 0.75f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, -40.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, -28.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, -18.0f));
        model = glm::scale(model, glm::vec3(0.82f, 0.82f, 0.82f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, 4.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, -6.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, -8.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-26.0f, 0.0f, -4.0f));
        model = glm::scale(model, glm::vec3(0.78f, 0.78f, 0.78f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, 4.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, -6.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, -8.0f));
        model = glm::scale(model, glm::vec3(0.82f, 0.82f, 0.82f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, -4.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(18.0f, 0.0f, 14.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(18.0f, 0.0f, 6.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, 18.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(24.0f, 0.0f, 10.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, 16.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, 4.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(22.0f, 0.0f, 20.0f));
        model = glm::scale(model, glm::vec3(0.78f, 0.78f, 0.78f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(16.0f, 0.0f, 18.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, 24.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(16.0f, 0.0f, 32.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(20.0f, 0.0f, 28.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(24.0f, 0.0f, 24.0f));
        model = glm::scale(model, glm::vec3(0.82f, 0.82f, 0.82f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(26.0f, 0.0f, 30.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(14.0f, 0.0f, 36.0f));
        model = glm::scale(model, glm::vec3(0.78f, 0.78f, 0.78f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(22.0f, 0.0f, 34.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, 20.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-18.0f, 0.0f, 16.0f));
        model = glm::scale(model, glm::vec3(0.85f, 0.85f, 0.85f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-20.0f, 0.0f, 24.0f));
        model = glm::scale(model, glm::vec3(0.92f, 0.92f, 0.92f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-16.0f, 0.0f, 30.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    if (oakTreeModel && oakTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-22.0f, 0.0f, 20.0f));
        model = glm::scale(model, glm::vec3(0.88f, 0.88f, 0.88f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        oakTreeModel->draw(basicShader);
    }

    if (spruceTreeModel && spruceTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-24.0f, 0.0f, 28.0f));
        model = glm::scale(model, glm::vec3(0.78f, 0.78f, 0.78f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        spruceTreeModel->draw(basicShader);
    }

    if (pineTreeModel && pineTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-14.0f, 0.0f, 34.0f));
        model = glm::scale(model, glm::vec3(0.95f, 0.95f, 0.95f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        pineTreeModel->draw(basicShader);
    }

    if (lindenTreeModel && lindenTreeModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-26.0f, 0.0f, 14.0f));
        model = glm::scale(model, glm::vec3(0.82f, 0.82f, 0.82f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lindenTreeModel->draw(basicShader);
    }

    //  ANGEL STATUE 
    if (angelStatueModel && angelStatueModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -45.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        angelStatueModel->draw(basicShader);
    }

    if (lamp12Model && lamp12Model->vertexCount() > 0) {
        // Lampă stânga statuii - în față
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-2.0f, 0.0f, -42.0f));
        model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lamp12Model->draw(basicShader);

        // Lampă dreapta statuii - în față
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, -42.0f));
        model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
        lamp12Model->draw(basicShader);
    }

    //  MOON
    if (moonModel && moonModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(15.0f, 35.0f, -30.0f)); 
        model = glm::rotate(model, glm::radians(moonRotation), glm::vec3(0.0f, 1.0f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.002f, 0.002f, 0.002f)); 
        basicShader.setMat4("model", model);
        basicShader.setVec3("objectColor", glm::vec3(1.0f, 1.0f, 0.9f)); 
        moonModel->draw(basicShader);
    }
    
    // RAIN
    if (rainEnabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_PROGRAM_POINT_SIZE);
        renderRain(view, projection);
        glDisable(GL_BLEND);
    }
}

void cleanup() {
delete benchModel;
delete lampModel;
delete spruceTreeModel;
delete pineTreeModel;
delete oakTreeModel;
delete lindenTreeModel;
delete angelStatueModel;
delete lamp12Model;
delete bunnyTruckModel;
delete moonModel;
delete skybox;

glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteTextures(1, &pavementTexture);
    
    glDeleteFramebuffers(1, &shadowMapFBO);
    glDeleteTextures(1, &shadowMapTexture);
    
    glDeleteVertexArrays(1, &rainVAO);
    glDeleteBuffers(1, &rainVBO);

    glfwDestroyWindow(glWindow);
    glfwTerminate();
}

void createGround() {
float groundSize = 50.0f; 
float texRepeat = 10.0f;  

float groundVertices[] = {
    -groundSize, 0.0f, -groundSize,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,          1.0f, 1.0f, 1.0f,
     groundSize, 0.0f, -groundSize,  0.0f, 1.0f, 0.0f,  texRepeat, 0.0f,     1.0f, 1.0f, 1.0f,
     groundSize, 0.0f,  groundSize,  0.0f, 1.0f, 0.0f,  texRepeat, texRepeat, 1.0f, 1.0f, 1.0f,
    -groundSize, 0.0f,  groundSize,  0.0f, 1.0f, 0.0f,  0.0f, texRepeat,     1.0f, 1.0f, 1.0f
};

    unsigned int groundIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    glBindVertexArray(groundVAO);

    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndices), groundIndices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Color attribute
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

// RAIN PARTICLE SYSTEM
void initRainSystem() {
    rainParticles.resize(MAX_RAIN_PARTICLES);
    
    // Inițializează particulele de ploaie
    for (int i = 0; i < MAX_RAIN_PARTICLES; i++) {
        rainParticles[i].position = glm::vec3(
            (rand() % 100 - 50),           
            5.0f + (rand() % 45),          
            (rand() % 100 - 50)           
        );
        rainParticles[i].speed = 25.0f + (rand() % 15);
    }
    
    // Creează VAO/VBO pentru ploaie
    glGenVertexArrays(1, &rainVAO);
    glGenBuffers(1, &rainVBO);
    
    glBindVertexArray(rainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_RAIN_PARTICLES * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    std::cout << "Rain system initialized with " << MAX_RAIN_PARTICLES << " particles" << std::endl;
}

void updateRainParticles(float deltaTime) {
    float windOffset = sin(windTime) * windStrength * 0.5f; 
    
    for (int i = 0; i < MAX_RAIN_PARTICLES; i++) {
        rainParticles[i].position.y -= rainParticles[i].speed * deltaTime;
        rainParticles[i].position.x += windOffset * deltaTime;
        rainParticles[i].position.z += windOffset * 0.2f * deltaTime;
        
        if (rainParticles[i].position.y < 0.0f) {
            rainParticles[i].position.y = 45.0f + (rand() % 10);
            rainParticles[i].position.x = (rand() % 100 - 50);
            rainParticles[i].position.z = (rand() % 100 - 50);
            rainParticles[i].speed = 25.0f + (rand() % 15);
        }
    }
}

void renderRain(glm::mat4 view, glm::mat4 projection) {
    if (!rainEnabled) return;
    
    // Pregătește datele pentru GPU
    std::vector<glm::vec3> positions(MAX_RAIN_PARTICLES);
    for (int i = 0; i < MAX_RAIN_PARTICLES; i++) {
        positions[i] = rainParticles[i].position;
    }
    
    // Actualizează buffer-ul
    glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());
    
    // Randează ploaia
    rainShader.useShaderProgram();
    rainShader.setMat4("projection", projection);
    rainShader.setMat4("view", view);
    
    glBindVertexArray(rainVAO);
    glDrawArrays(GL_POINTS, 0, MAX_RAIN_PARTICLES);
    glBindVertexArray(0);
}

void renderSceneDepth(Shader& shader) {
glm::mat4 model;
    
glDisable(GL_CULL_FACE);
model = glm::mat4(1.0f);
shader.setMat4("model", model);
glBindVertexArray(groundVAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
glBindVertexArray(0);
glEnable(GL_CULL_FACE);
    
    // Bunny Truck
    if (bunnyTruckModel && bunnyTruckModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(8.0f, 0.0f, 28.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(2.5f, 2.5f, 2.5f));
        shader.setMat4("model", model);
        bunnyTruckModel->draw(shader);
    }
    
    // Benches
    if (benchModel && benchModel->vertexCount() > 0) {
        glm::vec3 benchPositions[] = {
            glm::vec3(-8.0f, 0.0f, -30.0f), glm::vec3(-8.0f, 0.0f, -10.0f),
            glm::vec3(8.0f, 0.0f, -30.0f), glm::vec3(8.0f, 0.0f, -10.0f),
            glm::vec3(-8.0f, 0.0f, 10.0f), glm::vec3(8.0f, 0.0f, 10.0f)
        };
        float benchRotations[] = { 90.0f, 90.0f, -90.0f, -90.0f, 90.0f, -90.0f };
        for (int i = 0; i < 6; i++) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, benchPositions[i]);
            model = glm::rotate(model, glm::radians(benchRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
            shader.setMat4("model", model);
            benchModel->draw(shader);
        }
    }
    
    // Street Lamps
    if (lampModel && lampModel->vertexCount() > 0) {
        glm::vec3 lampPositions[] = {
            glm::vec3(-8.0f, 0.0f, -20.0f), glm::vec3(8.0f, 0.0f, -20.0f),
            glm::vec3(-8.0f, 0.0f, 0.0f), glm::vec3(8.0f, 0.0f, 0.0f),
            glm::vec3(8.0f, 0.0f, 20.0f)
        };
        float lampRotations[] = { 180.0f, 0.0f, 180.0f, 0.0f, 0.0f };
        for (int i = 0; i < 5; i++) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, lampPositions[i]);
            model = glm::rotate(model, glm::radians(lampRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
            shader.setMat4("model", model);
            lampModel->draw(shader);
        }
    }
    
    // Angel Statue
    if (angelStatueModel && angelStatueModel->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -45.0f));
        model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
        shader.setMat4("model", model);
        angelStatueModel->draw(shader);
    }
    
    // Lamp12
    if (lamp12Model && lamp12Model->vertexCount() > 0) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-2.0f, 0.0f, -42.0f));
        model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));
        shader.setMat4("model", model);
        lamp12Model->draw(shader);
        
        model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(2.0f, 0.0f, -42.0f));
                model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));
                shader.setMat4("model", model);
                lamp12Model->draw(shader);
            }
        }

        void initColliders() {
            sceneColliders.clear();
    
            //  BENCHES (bănci) 
            // Dimensiuni aproximative pentru o bancă - înălțime mărită pentru a acoperi camera
            glm::vec3 benchSize(2.0f, 8.0f, 1.5f); // Înălțime mare pentru a bloca camera
    
            // Băncile din stânga
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-8.0f, 4.0f, -30.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-8.0f, 4.0f, -10.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-8.0f, 4.0f, 10.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
    
            // Băncile din dreapta
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 4.0f, -30.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 4.0f, -10.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 4.0f, 10.0f), glm::vec3(1.5f, 8.0f, 2.0f)));
    
            //  STREET LAMPS (lămpi stradale) 
            // Lămpile sunt subțiri dar înalte
            glm::vec3 lampSize(1.0f, 10.0f, 1.0f);
    
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-8.0f, 5.0f, -20.0f), lampSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 5.0f, -20.0f), lampSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-8.0f, 5.0f, 0.0f), lampSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 5.0f, 0.0f), lampSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 5.0f, 20.0f), lampSize));
    
            //  ANGEL STATUE (statuia îngerului) 
            // Statuia este mai mare
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(0.0f, 5.0f, -45.0f), glm::vec3(3.0f, 10.0f, 3.0f)));
    
            //  LAMP_12 (lămpile mici lângă statuie) 
            glm::vec3 lamp12Size(1.0f, 10.0f, 1.0f);
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-2.0f, 5.0f, -42.0f), lamp12Size));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(2.0f, 5.0f, -42.0f), lamp12Size));
    
            //  BUNNY COTTON CANDY TRUCK 
            // Truck-ul este mare
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(8.0f, 4.0f, 28.0f), glm::vec3(8.0f, 8.0f, 6.0f)));
    
            // TREES (copaci) 
            // Trunchiurile copacilor - înălțime mare
            glm::vec3 treeSize(1.5f, 12.0f, 1.5f);
    
            // Stânga aleii - Rândul 1
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, -42.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, -34.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, -26.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, -18.0f), treeSize));
    
            // Stânga aleii - Rândul 2
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -44.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -38.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -30.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -22.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -14.0f), treeSize));
    
            // Stânga aleii - Rândul 3
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-26.0f, 6.0f, -40.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-26.0f, 6.0f, -28.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-26.0f, 6.0f, -18.0f), treeSize));
    
            // Dreapta aleii - Rândul 1
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, -42.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, -34.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, -26.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, -18.0f), treeSize));
    
            // Dreapta aleii - Rândul 2
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -44.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -38.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -30.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -22.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -14.0f), treeSize));
    
            // Dreapta aleii - Rândul 3
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, -40.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, -28.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, -18.0f), treeSize));
    
            // Copaci suplimentari (zona băncilor 5 și 6)
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, 4.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, -6.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, 0.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, -8.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-26.0f, 6.0f, -4.0f), treeSize));
    
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, 4.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, -6.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, 0.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, -8.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, -4.0f), treeSize));
    
            // Copaci în jurul Bunny Truck
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(18.0f, 6.0f, 14.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(18.0f, 6.0f, 6.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, 18.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(24.0f, 6.0f, 10.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, 16.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, 4.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(22.0f, 6.0f, 20.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(16.0f, 6.0f, 18.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, 24.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(16.0f, 6.0f, 32.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(20.0f, 6.0f, 28.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(24.0f, 6.0f, 24.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(26.0f, 6.0f, 30.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(14.0f, 6.0f, 36.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(22.0f, 6.0f, 34.0f), treeSize));
    
            // Copaci suplimentari stânga
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, 20.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-18.0f, 6.0f, 16.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-20.0f, 6.0f, 24.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-16.0f, 6.0f, 30.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-22.0f, 6.0f, 20.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-24.0f, 6.0f, 28.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-14.0f, 6.0f, 34.0f), treeSize));
            sceneColliders.push_back(AABB::fromCenterSize(glm::vec3(-26.0f, 6.0f, 14.0f), treeSize));
    
            //  SCENE BOUNDARIES (marginile scenei) 
            // Pereți invizibili la marginea scenei pentru a preveni ieșirea
            float wallHeight = 10.0f;
            float wallThickness = 2.0f;
            float sceneSize = 50.0f;
    
            // Perete Nord (Z negativ)
            sceneColliders.push_back(AABB(
                glm::vec3(-sceneSize, 0.0f, -sceneSize - wallThickness),
                glm::vec3(sceneSize, wallHeight, -sceneSize)
            ));
            // Perete Sud (Z pozitiv)
            sceneColliders.push_back(AABB(
                glm::vec3(-sceneSize, 0.0f, sceneSize),
                glm::vec3(sceneSize, wallHeight, sceneSize + wallThickness)
            ));
            // Perete Vest (X negativ)
            sceneColliders.push_back(AABB(
                glm::vec3(-sceneSize - wallThickness, 0.0f, -sceneSize),
                glm::vec3(-sceneSize, wallHeight, sceneSize)
            ));
            // Perete Est (X pozitiv)
            sceneColliders.push_back(AABB(
                glm::vec3(sceneSize, 0.0f, -sceneSize),
                glm::vec3(sceneSize + wallThickness, wallHeight, sceneSize)
            ));
    
            std::cout << "Collision system initialized with " << sceneColliders.size() << " colliders" << std::endl;
        }

int main(int argc, const char * argv[]) {

    if (!initOpenGLWindow()) {
        return 1;
    }
    
    // Load shaders
    basicShader.loadShader("shaders/basic.vert", "shaders/basic.frag");
    basicShader.useShaderProgram();
    
    // Load shadow shader
    shadowShader.loadShader("shaders/shadow.vert", "shaders/shadow.frag");
    
    // Load rain shader
    rainShader.loadShader("shaders/rain.vert", "shaders/rain.frag");
    
    // Initialize rain system
    initRainSystem();
    
    // Initialize shadow map
    initShadowMap();

    // Load textures
    pavementTexture = TextureLoader::LoadTexture("textures/pavement.jpg");
    if (pavementTexture == 0) {
        fprintf(stderr, "ERROR: Failed to load pavement texture\n");
    }

    // Create ground
    createGround();

    // Load models
    std::cout << "Loading 3D models..." << std::endl;

  

    benchModel = new Model();
    if (!benchModel->loadOBJ("models/bench/bench.obj")) {
        fprintf(stderr, "WARNING: Failed to load bench model\n");
    }

    lampModel = new Model();
    if (!lampModel->loadOBJ("models/street_lamp/street_lamp.obj")) {
        fprintf(stderr, "WARNING: Failed to load lamp model\n");
    }

    spruceTreeModel = new Model();
    if (!spruceTreeModel->loadOBJ("models/spruce_tree/spruce_tree.obj")) {
        fprintf(stderr, "WARNING: Failed to load spruce tree model\n");
    }

    pineTreeModel = new Model();
    if (!pineTreeModel->loadOBJ("models/pine_tree/pine_tree.obj")) {
        fprintf(stderr, "WARNING: Failed to load pine tree model\n");
    }

    oakTreeModel = new Model();
    if (!oakTreeModel->loadOBJ("models/petiolate_oak_tree/petiolate_oak_tree.obj")) {
        fprintf(stderr, "WARNING: Failed to load oak tree model\n");
    }

    lindenTreeModel = new Model();
    if (!lindenTreeModel->loadOBJ("models/linden_tree/linden_tree.obj")) {
        fprintf(stderr, "WARNING: Failed to load linden tree model\n");
    }

    angelStatueModel = new Model();
    if (!angelStatueModel->loadOBJ("models/graveyard_angel_statue/graveyard_angel_statue.obj")) {
        fprintf(stderr, "WARNING: Failed to load angel statue model\n");
    }

    lamp12Model = new Model();
    if (!lamp12Model->loadOBJ("models/lamp_12/lamp_12.obj")) {
        fprintf(stderr, "WARNING: Failed to load lamp_12 model\n");
    }

    bunnyTruckModel = new Model();
    if (!bunnyTruckModel->loadOBJ("models/bunny_cotton_candy_truck/bunny_cotton_candy_truck.obj")) {
        fprintf(stderr, "WARNING: Failed to load bunny truck model\n");
    }

    moonModel = new Model();
    if (!moonModel->loadOBJ("models/luna_earths_companion/luna_earths_companion.obj")) {
        fprintf(stderr, "WARNING: Failed to load moon model\n");
    }

    // Load skybox
    skybox = new Skybox();
    if (!skybox->load("models/skybox")) {
        fprintf(stderr, "WARNING: Failed to load skybox\n");
    }

    // Initialize collision system
    initColliders();
    camera.setColliders(&sceneColliders);

    std::cout << "Scene initialized with textured ground (50x50 units)" << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(glWindow)) {
        
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        // Input
        processInput(glWindow);
        
        // Update animations
        lampFlickerTime += deltaTime * 8.0f; // Frecvență pâlpâire
        treeSwayTime += deltaTime * 1.5f;    // Frecvență legănare
        cottonCandyRotation += deltaTime * 2.0f; // Rotație continuă a paharului de vată
        windTime += deltaTime * 0.5f; // Timpul pentru vânt
        
        // Update rain if enabled
        if (rainEnabled) {
            updateRainParticles(deltaTime);
        }
        
        // Render
        renderScene();

        glfwSwapBuffers(glWindow);
        glfwPollEvents();
    }
    
    cleanup();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
    
    // Moon rotation controls
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        moonRotation += 50.0f * deltaTime; // Rotate left
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        moonRotation -= 50.0f * deltaTime; // Rotate right
    
    // Render mode controls
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        renderMode = 0; // Solid
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        renderMode = 1; // Wireframe
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        renderMode = 2; // Points
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
        renderMode = 3; // Smooth
    
    // Weather controls
        static bool key0Pressed = false;
        static bool key9Pressed = false;
        static bool keyBPressed = false;
    
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS && !key0Pressed) {
            fogEnabled = !fogEnabled;
            key0Pressed = true;
            std::cout << "Fog " << (fogEnabled ? "enabled" : "disabled") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_RELEASE) {
            key0Pressed = false;
        }
    
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS && !key9Pressed) {
            rainEnabled = !rainEnabled;
            key9Pressed = true;
            std::cout << "Rain " << (rainEnabled ? "enabled" : "disabled") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_RELEASE) {
            key9Pressed = false;
        }
    
        // Collision toggle
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !keyBPressed) {
            camera.collisionEnabled = !camera.collisionEnabled;
            keyBPressed = true;
            std::cout << "Collision detection " << (camera.collisionEnabled ? "enabled" : "disabled") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) {
            keyBPressed = false;
        }
    }

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    
    lastX = xpos;
    lastY = ypos;
    
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}