#ifndef RAY_H
#define RAY_H

#include <glm/glm.hpp>
#include <algorithm>
#include <limits>
#include "AABB.h"

struct camDefaults{
	glm::mat4 view{ 1.0f };
	glm::mat4 projection{ 1.0f };
	glm::vec3 position{ 0.0f };
};

struct ray {
	glm::vec3 origin{ 0.0f };
	glm::vec3 direction{ 0.0f };

	ray() = default;

	ray(const glm::vec3& orig,const glm::vec3& dir) : origin(orig), direction(glm::normalize(dir)){}

	static ray rayGen(float mouseX, float mouseY, float screenWidth, float screenHeight,const camDefaults& conf) 
	{
		float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
		float ndcY = 1.0f - (2.0f * mouseY) / screenHeight ;

		glm::mat4 invVP = glm::inverse(conf.projection * conf.view);

		glm::vec4 pointNDC(ndcX, ndcY, -1.0f, 1.0f);
		glm::vec4 worldNear4D = invVP * pointNDC;
		glm::vec3 worldNear = glm::vec3(worldNear4D) / worldNear4D.w;

		return ray(conf.position, worldNear - conf.position);
	}

};

inline bool AABB::intersects(const ray& r, float& outDist) const {
	float tmin = 0.0f;
	float tmax = std::numeric_limits<float>::max();

	//Axis-X
	{
		float dirX = (std::abs(r.direction.x) < 1e-8f) ? 1e-8f : r.direction.x;
		float invD = 1.0f / dirX;
		float t1 = (min.x - r.origin.x) * invD;
		float t2 = (max.x - r.origin.x) * invD;

		if (t1 > t2) std::swap(t1, t2);	
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}
	//Axis-Y
	{
		float dirY = (std::abs(r.direction.y) < 1e-8f) ? 1e-8f : r.direction.y;
		float invD = 1.0f / dirY;
		float t1 = (min.y - r.origin.y) * invD;
		float t2 = (max.y - r.origin.y) * invD;

		if (t1 > t2) std::swap(t1, t2);	
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}
	//Axis-Z
	{
		float dirZ = (std::abs(r.direction.z) < 1e-8f) ? 1e-8f : r.direction.z;
		float invD = 1.0f / dirZ;
		float t1 = (min.z - r.origin.z) * invD;
		float t2 = (max.z - r.origin.z) * invD;

		if (t1 > t2) std::swap(t1, t2);	
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}
	outDist = tmin;
	return true;
};

// Add to Ray.h
inline bool intersectTriangle(const ray& r, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& outDist) {
	const float EPSILON = 0.0000001f;
	glm::vec3 edge1 = v1 - v0;
	glm::vec3 edge2 = v2 - v0;
	glm::vec3 h = glm::cross(r.direction, edge2);
	float a = glm::dot(edge1, h);

	// If 'a' is too close to 0, the ray is parallel to the triangle
	if (a > -EPSILON && a < EPSILON) return false;

	float f = 1.0f / a;
	glm::vec3 s = r.origin - v0;
	float u = f * glm::dot(s, h);

	// Ray misses the triangle
	if (u < 0.0f || u > 1.0f) return false;

	glm::vec3 q = glm::cross(s, edge1);
	float v = f * glm::dot(r.direction, q);

	// Ray misses the triangle
	if (v < 0.0f || u + v > 1.0f) return false;

	// Compute where the intersection is on the line
	float t = f * glm::dot(edge2, q);

	if (t > EPSILON) {
		outDist = t;
		return true;
	}

	return false; // Line intersection, but not a ray intersection
}

#endif // !RAY_H
