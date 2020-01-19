

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	float fov;
	float znear, zfar;


	
	// Use to adjust mouse rotation speed
	float rotationSpeed = 0.20f;
	// Use to adjust mouse zoom speed
	float zoomSpeed = 5.50f;
	

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();


	float movementSpeed = 3.0f;

	bool updated = false;

	glm::mat4 view;

	glm::vec3 camFront;

	float moveSpeed;
	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	Camera();
	~Camera();
	void UpdateViewMatrix();
	void Update(float deltaTime);
	
	

};