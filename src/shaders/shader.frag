#version 330 core
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2
#define NR_POINT_LIGHTS 16

in vec3 normal;
in vec3 fragPos;
in vec2 texCoords;

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform Light light[NR_POINT_LIGHTS];
uniform int usedLight;

struct Material {
    float shininess;
    sampler2D diffuse;
    sampler2D specular;
    sampler2D opacity;
    int hasOpacityMap;
    vec3 specularColor;
};

uniform Material material;
uniform int usedLightCount;
uniform vec3 viewPos;
out vec4 fragColor;

vec3 CalcLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
    vec4 texColor = texture(material.diffuse, texCoords);
    if (texColor.a < 0.1) {
        discard;
    }

    
    if (material.hasOpacityMap == 1) {
        float mask = texture(material.opacity, texCoords).r;
        if (mask < 0.5) {
            discard;
        }
    }

    vec3 norm;
    if (length(normal) > 0.1) {
        norm = normalize(normal);
    } else {
        norm = normalize(cross(dFdx(fragPos), dFdy(fragPos)));
    }

    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 result = vec3(0.0);
    for (int i = 0; i < usedLight && i < NR_POINT_LIGHTS; i++) {
        result += CalcLight(light[i], norm, fragPos, viewDir);
    }

    fragColor = vec4(result, 1.0);
}

vec3 CalcLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir;
    float attenuation = 1.0;
    float intensity = 1.0;

    if (light.type == DIRECTIONAL_LIGHT) {
        lightDir = normalize(-light.direction);
    } else {
        lightDir = normalize(light.position - fragPos);
        float dist = length(light.position - fragPos);
        attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);
        
        if (light.type == SPOT_LIGHT) {
            float theta = dot(lightDir, normalize(-light.direction));
            float epsilon = (light.cutOff - light.outerCutOff);
            intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
        }
    }

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);

    vec3 ambient  = light.ambient  * vec3(texture(material.diffuse, texCoords));
    vec3 diffuse  = light.diffuse  * diff * vec3(texture(material.diffuse, texCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, texCoords)) * material.specularColor;

    return (ambient + diffuse + specular) * attenuation * intensity;
}