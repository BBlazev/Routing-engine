#ifndef RENDERER_CAMERA_HPP
#define RENDERER_CAMERA_HPP

#include <glm/glm.hpp>

class Window;


class Camera {
public:
	glm::vec3 position{ 0.0f, 0.0f, 5.0f };

	float pitch = 0.0f;  
	float yaw = 0.0f;  

	float moveSpeed = 5.0f;   
	float sprintMultiplier = 4.0f;
	float lookSensitivity = 0.0025f; 

	void update(const Window& window, float dt, bool inputEnabled);

	[[nodiscard]] glm::mat4 rotation_matrix() const;
	[[nodiscard]] glm::mat4 view_matrix() const;
	[[nodiscard]] glm::mat4 projection_matrix(float aspect, float fovDegrees, float nearPlane, float farPlane) const;

private:
	bool   looking_ = false;
	double lastMouseX_ = 0.0;
	double lastMouseY_ = 0.0;
};

#endif // RENDERER_CAMERA_HPP