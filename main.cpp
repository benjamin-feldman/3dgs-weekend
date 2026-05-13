#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/common.hpp"
#include "glm/trigonometric.hpp"
#include "camera.h"
#include "ply_loader.h"
#include "shader.h"

#include <cstddef>
#include <exception>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

constexpr int initialWidth = 1280 * 1.2;
constexpr int initialHeight = 720 * 1.2;

struct GlBuffers {
    GLuint vao = 0;
    GLuint quadVbo = 0;
    GLuint gaussianVbo = 0;
    GLuint sphericalHarmonicsBuffer = 0;
    GLuint sphericalHarmonicsTexture = 0;
};

struct GpuGaussian {
    glm::vec3 centroid = glm::vec3(0.0f);
    float opacity = 0.0f;
    std::array<float, 3> scale = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct SceneBounds {
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 1.0f;
};

struct SceneOrbit {
    glm::vec3 center = glm::vec3(0.0f);
    glm::mat4 rotation = glm::mat4(1.0f);
    float mouseSensitivity = 0.006f;

    void processMouseDrag(float xOffset, float yOffset, const Camera& camera) {
        glm::mat4 yaw =
            glm::rotate(glm::mat4(1.0f), xOffset * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 right = glm::normalize(glm::cross(camera.front, camera.up));
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), yOffset * mouseSensitivity, right);
        rotation = yaw * pitch * rotation;
    }

    glm::mat4 modelMatrix() const {
        return glm::translate(glm::mat4(1.0f), center) * rotation *
               glm::translate(glm::mat4(1.0f), -center);
    }

    glm::vec3 cameraPositionInScene(const glm::vec3& cameraPosition) const {
        return glm::vec3(glm::inverse(modelMatrix()) * glm::vec4(cameraPosition, 1.0f));
    }
};

struct SplatSortEntry {
    float cameraZ = 0.0f;
    std::size_t splatIndex = 0;
};

Camera* activeCamera = nullptr;
SceneOrbit* activeSceneOrbit = nullptr;
bool isMouseDragging = false;
bool firstMouseMove = true;
double lastMouseX = initialWidth * 0.5;
double lastMouseY = initialHeight * 0.5;

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow*, double xPosition, double yPosition) {
    if (!activeCamera || !activeSceneOrbit || !isMouseDragging) {
        return;
    }

    if (firstMouseMove) {
        lastMouseX = xPosition;
        lastMouseY = yPosition;
        firstMouseMove = false;
        return;
    }

    float xOffset = static_cast<float>(xPosition - lastMouseX);
    float yOffset = static_cast<float>(yPosition - lastMouseY);
    lastMouseX = xPosition;
    lastMouseY = yPosition;

    activeSceneOrbit->processMouseDrag(xOffset, yOffset, *activeCamera);
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    isMouseDragging = action == GLFW_PRESS;
    firstMouseMove = true;
}

void scrollCallback(GLFWwindow*, double, double yOffset) {
    if (activeCamera) {
        activeCamera->processMouseScroll(static_cast<float>(yOffset));
    }
}

void processInput(GLFWwindow* window, Camera& camera, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.processKeyboard(0, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.processKeyboard(1, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.processKeyboard(2, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.processKeyboard(3, deltaTime);
    }

    float verticalVelocity = camera.movementSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        camera.position -= camera.up * verticalVelocity;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        camera.position += camera.up * verticalVelocity;
    }

    float lookSpeed = 90.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        camera.processMouseMovement(-lookSpeed, 0.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        camera.processMouseMovement(lookSpeed, 0.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        camera.processMouseMovement(0.0f, lookSpeed);
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        camera.processMouseMovement(0.0f, -lookSpeed);
    }
}

GLFWwindow* createWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight, "3DGS", nullptr, nullptr);

    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    return window;
}

void configureQuadAttributes() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
}

void configureGaussianAttributes() {
    // Per-instance attributes: one sorted GpuGaussian is reused for every quad vertex.
    constexpr GLsizei stride = sizeof(GpuGaussian);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(GpuGaussian, centroid)));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(GpuGaussian, opacity)));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(GpuGaussian, scale)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(GpuGaussian, rotation)));
    glVertexAttribDivisor(4, 1);
}

GpuGaussian makeGpuGaussian(const GaussianSplat& splat) {
    GpuGaussian gaussian;
    gaussian.centroid = splat.centroid;
    gaussian.opacity = splat.opacity;
    gaussian.scale = splat.scale;
    gaussian.rotation = splat.rotation;
    return gaussian;
}

std::vector<GpuGaussian> collectGpuGaussians(const Scene& ply) {
    std::vector<GpuGaussian> values;
    values.reserve(ply.splats.size());
    for (const GaussianSplat& splat : ply.splats) {
        values.push_back(makeGpuGaussian(splat));
    }
    return values;
}

std::vector<float> collectSphericalHarmonics(const Scene& ply) {
    std::vector<float> values;
    values.reserve(ply.splats.size() * SH_FLOAT_COUNT);
    for (const GaussianSplat& splat : ply.splats) {
        values.insert(values.end(), splat.sphericalHarmonics.begin(),
                      splat.sphericalHarmonics.end());
    }
    return values;
}

void sortSplatsBackToFront(const Scene& ply, const glm::mat4& view,
                           std::vector<SplatSortEntry>& sortEntries,
                           std::vector<GpuGaussian>& sortedGaussians,
                           std::vector<float>& sortedSphericalHarmonics) {
    sortEntries.resize(ply.splats.size());
    sortedGaussians.resize(ply.splats.size());
    sortedSphericalHarmonics.resize(ply.splats.size() * SH_FLOAT_COUNT);

    for (std::size_t i = 0; i < ply.splats.size(); ++i) {
        glm::vec4 cameraCentroid = view * glm::vec4(ply.splats[i].centroid, 1.0f);
        sortEntries[i] = {cameraCentroid.z, i};
    }

    std::sort(
        sortEntries.begin(), sortEntries.end(),
        [](const SplatSortEntry& a, const SplatSortEntry& b) { return a.cameraZ < b.cameraZ; });

    for (std::size_t outputIndex = 0; outputIndex < sortEntries.size(); ++outputIndex) {
        const GaussianSplat& splat = ply.splats[sortEntries[outputIndex].splatIndex];
        sortedGaussians[outputIndex] = makeGpuGaussian(splat);
        std::copy(splat.sphericalHarmonics.begin(), splat.sphericalHarmonics.end(),
                  sortedSphericalHarmonics.begin() +
                      static_cast<std::ptrdiff_t>(outputIndex * SH_FLOAT_COUNT));
    }
}

void uploadSortedSplats(const GlBuffers& buffers, const std::vector<GpuGaussian>& sortedGaussians,
                        const std::vector<float>& sortedSphericalHarmonics) {
    glBindBuffer(GL_ARRAY_BUFFER, buffers.gaussianVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(sortedGaussians.size() * sizeof(GpuGaussian)),
                    sortedGaussians.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_TEXTURE_BUFFER, buffers.sphericalHarmonicsBuffer);
    glBufferSubData(GL_TEXTURE_BUFFER, 0,
                    static_cast<GLsizeiptr>(sortedSphericalHarmonics.size() * sizeof(float)),
                    sortedSphericalHarmonics.data());
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

GlBuffers createBuffers(const Scene& ply) {
    GlBuffers buffers;
    glGenVertexArrays(1, &buffers.vao);
    glGenBuffers(1, &buffers.quadVbo);
    glGenBuffers(1, &buffers.gaussianVbo);
    glGenBuffers(1, &buffers.sphericalHarmonicsBuffer);
    glGenTextures(1, &buffers.sphericalHarmonicsTexture);

    glBindVertexArray(buffers.vao);

    constexpr glm::vec2 quadCorners[] = {
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {-1.0f, 1.0f},
        {1.0f, 1.0f},
    };
    glBindBuffer(GL_ARRAY_BUFFER, buffers.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadCorners), quadCorners, GL_STATIC_DRAW);
    configureQuadAttributes();

    std::vector<GpuGaussian> gpuGaussians = collectGpuGaussians(ply);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.gaussianVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(gpuGaussians.size() * sizeof(GpuGaussian)),
                 gpuGaussians.data(), GL_DYNAMIC_DRAW);
    configureGaussianAttributes();

    std::vector<float> sphericalHarmonics = collectSphericalHarmonics(ply);
    glBindBuffer(GL_TEXTURE_BUFFER, buffers.sphericalHarmonicsBuffer);
    glBufferData(GL_TEXTURE_BUFFER,
                 static_cast<GLsizeiptr>(sphericalHarmonics.size() * sizeof(float)),
                 sphericalHarmonics.data(), GL_DYNAMIC_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, buffers.sphericalHarmonicsTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buffers.sphericalHarmonicsBuffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return buffers;
}

void destroyBuffers(const GlBuffers& buffers) {
    glDeleteTextures(1, &buffers.sphericalHarmonicsTexture);
    glDeleteBuffers(1, &buffers.sphericalHarmonicsBuffer);
    glDeleteBuffers(1, &buffers.quadVbo);
    glDeleteBuffers(1, &buffers.gaussianVbo);
    glDeleteVertexArrays(1, &buffers.vao);
}

SceneBounds computeSceneBounds(const Scene& ply) {
    if (ply.splats.empty()) {
        return {};
    }

    glm::vec3 sum(0.0f);

    for (const GaussianSplat& splat : ply.splats) {
        sum += splat.centroid;
    }

    SceneBounds bounds;
    bounds.center = sum / static_cast<float>(ply.splats.size());

    std::vector<float> distances;
    distances.reserve(ply.splats.size());
    for (const GaussianSplat& splat : ply.splats) {
        distances.push_back(glm::length(splat.centroid - bounds.center));
    }

    std::size_t index = static_cast<std::size_t>(static_cast<float>(distances.size() - 1) * 0.95f);
    std::nth_element(distances.begin(), distances.begin() + index, distances.end());
    bounds.radius = std::max(distances[index] * 1.5f, 1.0f);
    return bounds;
}

std::string plyPathFromArgs(int argc, char** argv) {
    if (argc > 1) {
        return argv[1];
    }
    // load default scene
    return "scene.ply";
}

int main(int argc, char** argv) {
    GLFWwindow* window = nullptr;

    try {
        const std::string plyPath = plyPathFromArgs(argc, argv);
        Scene plyScene = loadPly(plyPath);
        std::cout << "Loaded " << plyScene.splats.size() << " Gaussian splats from " << plyPath
                  << '\n';

        for (GaussianSplat& splat : plyScene.splats) {
            // Flip Y for this OpenGL preview so the PLY scene appears upright.
            splat.centroid.y = -splat.centroid.y;
        }

        window = createWindow();
        {
            Shader shader("shaders/splat.vert", "shaders/splat.frag");
            GlBuffers buffers = createBuffers(plyScene);
            SceneBounds bounds = computeSceneBounds(plyScene);
            SceneOrbit sceneOrbit;
            sceneOrbit.center = bounds.center;
            Camera camera(bounds.center + glm::vec3(0.0f, 0.0f, bounds.radius * 2.5f));
            camera.position = {0.0f, 0.15f, 0.4f};
            glm::vec3 defaultFront = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
            camera.front = defaultFront;
            camera.pitch = glm::degrees(std::asin(defaultFront.y));
            camera.yaw = glm::degrees(std::atan2(defaultFront.z, defaultFront.x));
            camera.movementSpeed = bounds.radius * 0.1;

            activeCamera = &camera;
            activeSceneOrbit = &sceneOrbit;
            isMouseDragging = false;
            firstMouseMove = true;
            glfwSetCursorPosCallback(window, mouseCallback);
            glfwSetMouseButtonCallback(window, mouseButtonCallback);
            glfwSetScrollCallback(window, scrollCallback);

            // Translucent splats need draw-order sorting instead of depth testing.
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            std::vector<SplatSortEntry> sortEntries;
            std::vector<GpuGaussian> sortedGaussians;
            std::vector<float> sortedSphericalHarmonics;
            float lastFrame = static_cast<float>(glfwGetTime());

            while (!glfwWindowShouldClose(window)) {
                float currentFrame = static_cast<float>(glfwGetTime());
                float deltaTime = currentFrame - lastFrame;
                lastFrame = currentFrame;

                processInput(window, camera, deltaTime);

                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                glViewport(0, 0, width, height);
                float aspect =
                    height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 sceneModel = sceneOrbit.modelMatrix();
                glm::mat4 viewModel = view * sceneModel;
                glm::mat4 projection = glm::perspective(
                    glm::radians(camera.fov), aspect, bounds.radius * 0.01f, bounds.radius * 10.0f);

                sortSplatsBackToFront(plyScene, viewModel, sortEntries, sortedGaussians,
                                      sortedSphericalHarmonics);
                uploadSortedSplats(buffers, sortedGaussians, sortedSphericalHarmonics);

                glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                shader.use();
                shader.setMat4("view", viewModel);
                shader.setMat4("projection", projection);
                shader.setVec3("cameraPosition", sceneOrbit.cameraPositionInScene(camera.position));
                shader.setVec2("viewportSize", glm::vec2(width, height));
                shader.setInt("sphericalHarmonicsTexture", 0);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_BUFFER, buffers.sphericalHarmonicsTexture);
                glBindVertexArray(buffers.vao);
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                                      static_cast<GLsizei>(plyScene.splats.size()));
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_BUFFER, 0);

                glfwSwapBuffers(window);
                glfwPollEvents();
            }

            destroyBuffers(buffers);
            activeCamera = nullptr;
            activeSceneOrbit = nullptr;
        }

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
        return 1;
    }
}
