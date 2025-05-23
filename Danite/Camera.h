#pragma once
namespace DDing {
	class Camera
	{
	public:
		static glm::mat4 View;
		static glm::mat4 Projection;

		glm::vec3 localPosition = glm::vec3(0.0f);
		glm::quat localRotation = { 1.0f,0.0f,0.0f,0.0f };
		glm::vec3 localEulerAngle = {};
		glm::vec3 localScale = glm::vec3(1.0f);

		glm::vec3 worldPosition = {};
		glm::quat worldRotation = {};
		glm::vec3 worldScale = {};

		glm::mat4 cachedLocalMatrix = { 1.0f };
		glm::mat4 cachedWorldMatrix = { 1.0f };
		
		void Update() ;

		void DrawUI() ;
	private:
		glm::vec3 GetLocalScale() const { return localScale; }
		void SetLocalScale(const glm::vec3& localScale) { this->localScale = localScale; }
		glm::quat GetLocalRotation() const { return localRotation; }
		void SetLocalRotation(const glm::quat& localRotation) { this->localRotation = localRotation;  }
		glm::vec3 GetLocalPosition() const { return localPosition; }
		void SetLocalPosition(const glm::vec3& localPosition) { this->localPosition = localPosition;  }
		glm::mat4 GetLocalMatrix() const { return cachedLocalMatrix; }

		glm::vec3 GetRight() {
			return glm::normalize(worldRotation * glm::vec3(-1, 0, 0));
		}
		glm::vec3 GetUp() {
			
			return glm::normalize(worldRotation * glm::vec3(0, 1, 0));
		}
		glm::vec3 GetLook() {
			return glm::normalize(worldRotation * glm::vec3(0, 0, -1));
		}
		void RecalculateMatrix();
		void UpdateMatrix();
		void UpdatePosition();
		void UpdateTransform();
		void UpdateRotation();
	};
}

