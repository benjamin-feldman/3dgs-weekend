#ifndef CAMERA_H
#define CAMERA_H

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Camera {
  public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw;
    float pitch;
    float fov;

    float movementSpeed;
    float mouseSensitivity;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f))
        : position(position), front(glm::vec3(0.0f, 0.0f, -1.0f)), up(glm::vec3(0.0f, 1.0f, 0.0f)),
          yaw(-90.0f), pitch(0.0f), fov(45.0f), movementSpeed(2.5f), mouseSensitivity(0.1f) {}

    glm::mat4 getViewMatrix() { return glm::lookAt(position, position + front, up); }

    void processKeyboard(int direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (direction == 0)
            position += front * velocity;
        if (direction == 1)
            position -= front * velocity;
        if (direction == 2)
            position -= glm::normalize(glm::cross(front, up)) * velocity;
        if (direction == 3)
            position += glm::normalize(glm::cross(front, up)) * velocity;
    }

    void processMouseMovement(float xOffset, float yOffset) {
        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        yaw += xOffset;
        pitch += yOffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(direction);
    }

    void processMouseScroll(float yOffset) {
        fov -= yOffset;
        if (fov < 1.0f)
            fov = 1.0f;
        if (fov > 45.0f)
            fov = 45.0f;
    }
};

#endif
