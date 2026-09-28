#ifndef AABB_H
#define AABB_H
#include "glm/glm.hpp"
#include <algorithm>
#include <limits>

struct ray;


struct AABB {
	glm::vec3 max{ std::numeric_limits<float>::lowest()};
	glm::vec3 min{ std::numeric_limits<float>::max()};

	AABB() = default;
	AABB(const glm::vec3 minPoint, const glm::vec3 maxPoint) : min(minPoint), max(maxPoint) {};

	void reset() {
		max = glm::vec3(std::numeric_limits<float>::lowest());
		min = glm::vec3(std::numeric_limits<float>::max());
	}

	void expand(const glm::vec3 point){
		min = glm::min(min, point);
		max = glm::max(max, point);
	}

	void expand(const AABB& other){
		min = glm::min(min, other.min);
		max = glm::max(max, other.max);
	}
	glm::vec3 getCenter() {
		return (min + max) * 0.5f;
	}

	glm::vec3 getExtent() const{
		return max - min;
	}

	float getMaxExtent() const {
		glm::vec3 ext = getExtent();
		return std::max({ ext.x, ext.y, ext.z });
	}

	inline bool intersects(const ray& r, float& outDist) const;
};


#endif