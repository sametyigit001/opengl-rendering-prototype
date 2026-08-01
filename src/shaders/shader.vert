#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
//layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoords;

out vec3 normal;
out vec3 fragPos;
out vec2 texCoords;
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

//out vec3 colorInput;
//out vec2 texCoords;

void main()
{
	gl_Position = uProjection * uView * uModel * vec4(aPos,1.0);
	fragPos = vec3(uModel * vec4(aPos, 1.0));
	normal = mat3(transpose(inverse(uModel))) * aNormal;
	texCoords = aTexCoords;
//texCoords = aTexCoords;
//colorInput = aColor;
	
}

