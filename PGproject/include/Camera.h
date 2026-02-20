#pragma once
#ifndef Camera_h
#define Camera_h

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 5.0f;
const float SENSITIVITY = 0.1f;
const float FOV = 45.0f;

// Axis-Aligned Bounding Box pentru detectarea coliziunilor
struct AABB {
    glm::vec3 min; 
    glm::vec3 max; 
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(glm::vec3 minPoint, glm::vec3 maxPoint) : min(minPoint), max(maxPoint) {}
    
    static AABB fromCenterSize(glm::vec3 center, glm::vec3 size) {
        glm::vec3 halfSize = size * 0.5f;
        return AABB(center - halfSize, center + halfSize);
    }
};

class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Fov;
    
    // Collision detection
    std::vector<AABB>* colliders;  
    float collisionRadius;          
    bool collisionEnabled;          

    Camera(glm::vec3 position = glm::vec3(0.0f, 5.0f, 10.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = YAW,
        float pitch = PITCH)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
        MovementSpeed(SPEED),
        MouseSensitivity(SENSITIVITY),
        Fov(FOV),
        colliders(nullptr),
        collisionRadius(1.0f),
        collisionEnabled(true)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    
    void setColliders(std::vector<AABB>* colliderList) {
        colliders = colliderList;
    }
    
    bool checkCollisionWithAABB(const glm::vec3& point, const AABB& box) const {
        glm::vec3 expandedMin = box.min - glm::vec3(collisionRadius);
        glm::vec3 expandedMax = box.max + glm::vec3(collisionRadius);
        
        return (point.x >= expandedMin.x && point.x <= expandedMax.x &&
                point.y >= expandedMin.y && point.y <= expandedMax.y &&
                point.z >= expandedMin.z && point.z <= expandedMax.z);
    }
    
    bool checkCollision(const glm::vec3& newPosition) const {
        if (!collisionEnabled || colliders == nullptr) {
            return false;
        }
        
        for (const AABB& box : *colliders) {
            if (checkCollisionWithAABB(newPosition, box)) {
                return true;
            }
        }
        return false;
    }

    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        glm::vec3 newPosition = Position;
        
        if (direction == FORWARD)
            newPosition += Front * velocity;
        if (direction == BACKWARD)
            newPosition -= Front * velocity;
        if (direction == LEFT)
            newPosition -= Right * velocity;
        if (direction == RIGHT)
            newPosition += Right * velocity;
        if (direction == UP)
            newPosition += Up * velocity;
        if (direction == DOWN)
            newPosition -= Up * velocity;
        
        if (!checkCollision(newPosition)) {
            Position = newPosition;
        } else {
            glm::vec3 slideX = Position;
            glm::vec3 slideZ = Position;
            
            slideX.x = newPosition.x;
            slideZ.z = newPosition.z;
            
            if (!checkCollision(slideX)) {
                Position.x = slideX.x;
            }
            if (!checkCollision(slideZ)) {
                Position.z = slideZ.z;
            }
            glm::vec3 slideY = Position;
            slideY.y = newPosition.y;
            if (!checkCollision(slideY)) {
                Position.y = slideY.y;
            }
        }
        
        if (Position.y < 1.0f) {
            Position.y = 1.0f;
        }
    }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        updateCameraVectors();
    }

    void ProcessMouseScroll(float yoffset)
    {
        Fov -= yoffset;
        if (Fov < 1.0f)
            Fov = 1.0f;
        if (Fov > 45.0f)
            Fov = 45.0f;
    }

private:
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

#endif