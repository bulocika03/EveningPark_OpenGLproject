#version 410 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 VertexColor;
in vec4 FragPosLightSpace;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform bool useTexture;
uniform bool smoothShading;  
uniform vec3 emissionColor;  
uniform bool hasEmission;    
uniform bool shadowsEnabled; 

// Fog uniforms
uniform bool fogEnabled;
uniform vec3 fogColor;
uniform float fogDensity;
uniform vec3 cameraPos;

// Point lights from lamps
#define MAX_POINT_LIGHTS 8
uniform vec3 pointLightPos[MAX_POINT_LIGHTS];
uniform vec3 pointLightColor[MAX_POINT_LIGHTS];
uniform int numPointLights;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    if(projCoords.z > 1.0)
        shadow = 0.0;
    
    return shadow;
}

void main()
{
    vec3 norm = normalize(Normal);
    
    // Main directional/ambient light
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    vec3 ambient = 0.2 * lightColor;
    vec3 diffuse = diff * lightColor * 0.3;
    
    // Specular lighting (only in smooth mode)
    vec3 specular = vec3(0.0);
    if (smoothShading) {
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        specular = spec * lightColor * 0.5;
    }
    
    // Calculate point lights contribution
    vec3 pointLighting = vec3(0.0);
    for (int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        vec3 pointDir = normalize(pointLightPos[i] - FragPos);
        float pointDiff = max(dot(norm, pointDir), 0.0);
        
        // Attenuation based on distance
        float distance = length(pointLightPos[i] - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        
        // Add specular for point lights in smooth mode
        float pointSpec = 0.0;
        if (smoothShading) {
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(-pointDir, norm);
            pointSpec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 0.3;
        }
        
        pointLighting += pointLightColor[i] * (pointDiff + pointSpec) * attenuation;
    }
    
    vec3 finalColor;
    
    if (useTexture) {
        vec4 texColor = texture(diffuseTexture, TexCoords);
        finalColor = texColor.rgb * objectColor;
    } else {
        finalColor = objectColor;
    }
    
    vec3 result;
    if (hasEmission) {
        // Obiectele emissive stralucesc independent de iluminare
        // Aplic o tenta calda galbuie pentru becurile de lampa
        vec3 warmEmission = emissionColor * vec3(1.0, 0.85, 0.5);
        result = finalColor * 0.3 + warmEmission * 1.5;
    } else {
        // Calculez umbra de la lumina principala (luna)
        float shadow = 0.0;
        if (shadowsEnabled) {
            vec3 lightDir = normalize(lightPos - FragPos);
            shadow = ShadowCalculation(FragPosLightSpace, norm, lightDir);
        }
        
        // Aplic umbra pe iluminarea directionala
        vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular) + pointLighting;
        result = lighting * finalColor;
    }
    
    // Apply fog effect
    if (fogEnabled) {
        float distance = length(FragPos - cameraPos);
        float fogFactor = 1.0 - exp(-fogDensity * distance * distance);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        result = mix(result, fogColor, fogFactor);
    }
    
    FragColor = vec4(result, 1.0);
}