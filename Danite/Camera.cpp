#include "pch.h"
#include "Camera.h"
glm::mat4 DDing::Camera::View = glm::mat4();
glm::mat4 DDing::Camera::Projection = glm::mat4();
void DDing::Camera::Update()
{
	UpdateTransform();
	RecalculateMatrix();
	UpdateMatrix();
}

void DDing::Camera::DrawUI()
{
	ImGui::CollapsingHeader("Camera");
}

void DDing::Camera::RecalculateMatrix()
{
	cachedLocalMatrix = glm::scale(glm::translate(glm::mat4(1.0f), localPosition) *
		glm::mat4(localRotation), localScale);

	
	cachedWorldMatrix = cachedLocalMatrix;
	
	
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(cachedWorldMatrix, worldScale, worldRotation, worldPosition, skew,perspective);

}

void DDing::Camera::UpdateMatrix()
{
	const float zNear = 0.0001f;
	const float zFar = 10000.0f;
	
	const float fov = 70.0f;
	const float aspect = 1600.f / 900.f;
	auto eyePosition = worldPosition;
	auto focusDirection = eyePosition + GetLook();
	auto upDirection = GetUp();

	View = glm::lookAtLH(eyePosition, focusDirection, upDirection);

	//if needed, Orthogonal add
	Projection = glm::perspectiveFovLH(glm::radians(fov), static_cast<float>(app->context.swapchainExtent.width), static_cast<float>(app->context.swapchainExtent.height), zNear, zFar);

	Projection[1][1] *= -1;

	const float halfVSide = zFar * tanf(glm::radians(fov) * 0.5f);
	const float halfHSide = halfVSide * aspect;
	const glm::vec3 frontMultFar = zFar * GetLook();

	viewFrustum.nearFace = { eyePosition + zNear * GetLook(),
		GetLook() };
	viewFrustum.farFace = { eyePosition + frontMultFar,
		-GetLook() };
	viewFrustum.rightFace = { eyePosition, 
		-glm::cross(frontMultFar - GetRight() * halfHSide, GetUp()) };
	viewFrustum.leftFace = { eyePosition,
		-glm::cross(GetUp(), frontMultFar + GetRight() * halfHSide)};
	viewFrustum.topFace = { eyePosition,
		-glm::cross(GetRight(), frontMultFar - GetUp() * halfVSide)};
	viewFrustum.bottomFace = { eyePosition,
		-glm::cross(frontMultFar + GetUp()*halfVSide, GetRight())};
}


void DDing::Camera::UpdatePosition()
{
	bool* keyPressed = app->input.keyPressed;
	auto forward = glm::normalize(GetLook());
	auto right = glm::normalize(GetRight());
	auto up = glm::normalize(GetUp());
	glm::vec3 move(0.0f);
	
	if (keyPressed[GLFW_KEY_W]) move += forward;
	if (keyPressed[GLFW_KEY_S]) move -= forward;
	if (keyPressed[GLFW_KEY_A]) move -= right;
	if (keyPressed[GLFW_KEY_D]) move += right;
	if (keyPressed[GLFW_KEY_E]) move += up;
	if (keyPressed[GLFW_KEY_Q]) move -= up;

	if (glm::length(move) > 0.0f) {
		auto movedPosition = GetLocalPosition() + glm::normalize(move) * 0.001f;
		SetLocalPosition(movedPosition);
	}
}

void DDing::Camera::UpdateTransform()
{
	UpdatePosition();
	UpdateRotation();
}

void DDing::Camera::UpdateRotation()
{
	static bool wasRightMouseDown = false;
	static glm::vec2 lastMouse = { InputManager::mouseX, InputManager::mouseY };

	bool rightMouseDown = InputManager::mouseButtons[GLFW_MOUSE_BUTTON_RIGHT];

	if (!rightMouseDown)
	{
		wasRightMouseDown = false;
		return;
	}

	glm::vec2 currentMouse = { InputManager::mouseX, InputManager::mouseY };

	if (!wasRightMouseDown)
	{
		// 첫 클릭 시점에서 lastMouse 초기화
		lastMouse = currentMouse;
		wasRightMouseDown = true;
		return;
	}

	glm::vec2 delta = currentMouse - lastMouse;
	float sensitivity = 0.1f;
	float yawDelta = delta.x * sensitivity;
	float pitchDelta = -delta.y * sensitivity;

	glm::quat quat = GetLocalRotation();
	
	glm::quat pitchQuat = glm::angleAxis(glm::radians(pitchDelta), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat yawQuat = glm::angleAxis(glm::radians(yawDelta), glm::vec3(0.0f, 1.0f, 0.0f));

	quat = yawQuat * quat;
	quat = quat * pitchQuat;

	SetLocalRotation(glm::quat(quat));

	lastMouse = currentMouse;
}
