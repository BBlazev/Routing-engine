#include <renderer/camera.hpp>

#include <platform/window.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>

namespace {
constexpr float kPitchLimit = 1.5533f;  // ~89 degrees
}

void Camera::update(const Window& window, float dt, bool inputEnabled) {
	
	GLFWwindow* handle = window.handle();

	const bool wantsLook = inputEnabled && window.mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT);

	if (wantsLook && !looking_) {

		glfwGetCursorPos(handle, &lastMouseX_, &lastMouseY_);
		glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		looking_ = true;
	}
	else if (!wantsLook && looking_) {
		glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		looking_ = false;
	}

	if (looking_) {
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(handle, &x, &y);

		const float dx = static_cast<float>(x - lastMouseX_);
		const float dy = static_cast<float>(y - lastMouseY_);
		lastMouseX_ = x;
		lastMouseY_ = y;

		yaw -= dx * lookSensitivity;
		pitch -= dy * lookSensitivity;
		pitch = std::clamp(pitch, -kPitchLimit, kPitchLimit);
	}

	if (!inputEnabled) return;

	glm::vec3 local{ 0.0f };
	if (window.key_pressed(GLFW_KEY_W)) local.z -= 1.0f;
	if (window.key_pressed(GLFW_KEY_S)) local.z += 1.0f;
	if (window.key_pressed(GLFW_KEY_A)) local.x -= 1.0f;
	if (window.key_pressed(GLFW_KEY_D)) local.x += 1.0f;
	if (window.key_pressed(GLFW_KEY_E)) local.y += 1.0f;
	if (window.key_pressed(GLFW_KEY_Q)) local.y -= 1.0f;

	if (glm::dot(local, local) <= 0.0f) return;

	local = glm::normalize(local); 

	float speed = moveSpeed;
	if (window.key_pressed(GLFW_KEY_LEFT_SHIFT)) speed *= sprintMultiplier;

	const glm::vec3 worldDelta =
		glm::vec3(rotation_matrix() * glm::vec4(local * speed * dt, 0.0f));

	position += worldDelta;
}

glm::mat4 Camera::rotation_matrix() const {
	const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.0f, 0.0f, 0.0f });
	const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.0f, 1.0f, 0.0f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 Camera::view_matrix() const {
	const glm::mat4 cameraTranslation = glm::translate(glm::mat4{ 1.0f }, position);
	return glm::inverse(cameraTranslation * rotation_matrix());
}

glm::mat4 Camera::projection_matrix(float aspect, float fovDegrees, float nearPlane, float farPlane) const 
{
	glm::mat4 proj =
		glm::perspective(glm::radians(fovDegrees), aspect, farPlane, nearPlane);


	proj[1][1] *= -1.0f;
	return proj;
}