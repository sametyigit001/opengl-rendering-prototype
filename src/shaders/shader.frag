#version 330 core
in vec3 normal;
in vec3 fragPos;
in vec2 texCoords;


struct dirLight {
vec3 direction;

vec3 ambient;
vec3 diffuse;
vec3 specular;
};

struct pointLight {
vec3 position;

vec3 ambient;
vec3 diffuse;
vec3 specular;

float constant;
float linear;
float quadratic;
};

#define NR_POINT_LIGHTS 4
uniform pointLight L_point[NR_POINT_LIGHTS];

struct spotLight{

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

uniform spotLight L_spot;

struct Material {

float shininess;
sampler2D diffuse;
sampler2D specular;

};
uniform Material material;
uniform dirLight L_dir;
uniform vec3 viewPos;

out vec4 fragColor;

vec3 calcPointLight(pointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 calcDirLight(dirLight light, vec3 normal,vec3 viewDir);
vec3 calcSpotLight(spotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);


void main()
{

vec3 norm = normalize(normal);
vec3 viewDir = normalize(viewPos - fragPos);

vec3 result = calcSpotLight(L_spot,norm,fragPos,viewDir);
result += calcDirLight(L_dir,norm,viewDir);

//for(int i = 0;i < NR_POINT_LIGHTS;i++){
 //result += CalcPointLight(L_point[i],norm,fragPos,viewDir);
//}

fragColor = vec4(result,1.0);
}

vec3 calcDirLight(dirLight light, vec3 normal,vec3 viewDir)
{

vec3 lightDir = normalize(-light.direction);
float diff = max(dot(normal,lightDir),0.0);
vec3 reflectDir = reflect(-lightDir,normal);
float spec = pow(max(dot(viewDir,reflectDir),0.0),material.shininess);

vec3 ambient = light.ambient * vec3(texture(material.diffuse,texCoords));
vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse,texCoords));
vec3 specular = light.specular * spec * vec3(texture(material.specular,texCoords));

return (ambient + diffuse + specular);
}


vec3 calcPointLight(pointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{

vec3 lightDir = normalize(light.position - fragPos);
float diff = max(dot(normal,lightDir),0.0);
vec3 reflectDir = reflect(-lightDir,normal);
float spec = pow(max(dot(viewDir,reflectDir),0.0),material.shininess);

float dist = length(light.position - fragPos);
float attenuation = 1.0/(light.constant + light.linear * dist + light.quadratic * dist * dist);

vec3 ambient = light.ambient * vec3(texture(material.diffuse,texCoords));
vec3 specular = light.specular * spec * vec3(texture(material.specular,texCoords));
vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse,texCoords));

ambient *= attenuation;
specular *= attenuation;
diffuse *= attenuation;

return (ambient + specular + diffuse);
}

vec3 calcSpotLight(spotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{

vec3 lightDir = normalize(light.position - fragPos);
float diff = max(dot(normal,lightDir),0.0);
vec3 reflectDir = reflect(-lightDir,normal);
float spec = pow(max(dot(viewDir,reflectDir),0.0),material.shininess);

float dist = length(light.position - fragPos);
float attenuation = 1.0/(light.constant + light.linear * dist + light.quadratic * dist * dist);

vec3 ambient = light.ambient * vec3(texture(material.diffuse,texCoords));
vec3 specular = light.specular * spec * vec3(texture(material.specular,texCoords));
vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse,texCoords));

float theta = dot(lightDir,normalize(-light.direction));
float epsilon = light.cutOff - light.outerCutOff;
float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0 ,1.0);

ambient *= attenuation * intensity;
specular *= attenuation * intensity;
diffuse *= attenuation * intensity;

return (ambient + specular + diffuse);
}