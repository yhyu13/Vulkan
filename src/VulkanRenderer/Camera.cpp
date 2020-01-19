#include "Camera.h"

Camera::Camera()
{
	position = glm::vec3(0.0f,-1.0f,0.0f);
}

Camera::~Camera()
{
}

void Camera::UpdateViewMatrix()
{
	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	transM = glm::translate(glm::mat4(1.0f), position);

	
	view = rotM * transM;
	

	updated = true;
}

void Camera::Update(float deltaTime)
{
	updated = false;


	
	camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
	camFront.y = sin(glm::radians(rotation.x));
	camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
	camFront = glm::normalize(camFront);

	moveSpeed = deltaTime * movementSpeed;

	if (keys.up)
		position += camFront * moveSpeed;
	if (keys.down)
		position -= camFront * moveSpeed;
	if (keys.left)
		position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
	if (keys.right)
		position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

	UpdateViewMatrix();


}