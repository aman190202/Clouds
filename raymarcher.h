#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include <cmath>

class RayMarcher {
private:
    // Constants from the original shader
    static constexpr int NUM_STEPS = 256;
    static constexpr float MAX_DIST = 1000.0f;
    static constexpr float MIN_DIST = 0.00001f;

    struct Material {
        glm::vec3 color;
        float dist;
    };

    // Utility functions
    static float random(const glm::vec2& p) {
        glm::vec2 scaledP = 50.0f * glm::fract(p * 0.318f + glm::vec2(0.71f, 0.113f));
        return -1.0f + 2.0f * glm::fract(scaledP.x * scaledP.y * (scaledP.x + scaledP.y));
    }

    static float noise(const glm::vec2& p) {
        glm::vec2 texSize = glm::vec2(1.0f);
        glm::vec2 pc = p * texSize;
        glm::vec2 base = glm::floor(pc);

        float s1 = random(base + glm::vec2(0.0f, 0.0f) / texSize);
        float s2 = random(base + glm::vec2(1.0f, 0.0f) / texSize);
        float s3 = random(base + glm::vec2(0.0f, 1.0f) / texSize);
        float s4 = random(base + glm::vec2(1.0f, 1.0f) / texSize);

        glm::vec2 f = glm::smoothstep(0.0f, 1.0f, glm::fract(pc));

        return glm::mix(glm::mix(s1, s2, f.x), glm::mix(s3, s4, f.x), f.y);
    }

    static float noiseFBM(const glm::vec2& p, int octaves) {
        float v = 0.0f;
        float a = 0.4f;
        glm::vec2 shift = glm::vec2(500.0f);
        
        // Rotate to reduce axial bias
        glm::mat2 rot(std::cos(0.7f), std::sin(0.7f), 
                     -std::sin(0.7f), std::cos(0.7f));
        
        glm::vec2 currP = p;
        for (int i = 0; i < octaves; ++i) {
            v += a * noise(currP);
            currP = rot * currP * 2.0f + shift;
            a *= 0.5f;
        }
        
        return v;
    }

    static float saturate(float x) {
        return glm::clamp(x, 0.0f, 1.0f);
    }

    // Scene SDF calculation
   static Material calculateSceneSDF(const glm::vec3& pos, float baseY) {
        float currNoiseSample = std::abs(noiseFBM(glm::vec2(pos.x, pos.z) / 2.0f, 1));
        currNoiseSample *= 5.5f;

        currNoiseSample += 0.1f * noiseFBM(glm::vec2(pos.x, pos.z) * 4.0f, 10);
        
        glm::vec3 landColor(0.498f, 0.434f, 0.396f);
        landColor = glm::mix(
            landColor,
            landColor * 0.25f,
            glm::smoothstep(0.1f, 0.9f, currNoiseSample)
        );

        return {landColor, pos.y - baseY + currNoiseSample};
    }

    // Normal calculation
    static glm::vec3 calculateNormal(const glm::vec3& pos, float baseY) 
    {
        const float EPS = 0.0001f;

        return glm::normalize(glm::vec3(
            calculateSceneSDF(pos + glm::vec3(EPS, 0.0f, 0.0f), baseY).dist - 
            calculateSceneSDF(pos - glm::vec3(EPS, 0.0f, 0.0f), baseY).dist,
            calculateSceneSDF(pos + glm::vec3(0.0f, EPS, 0.0f), baseY).dist - 
            calculateSceneSDF(pos - glm::vec3(0.0f, EPS, 0.0f), baseY).dist,
            calculateSceneSDF(pos + glm::vec3(0.0f, 0.0f, EPS), baseY).dist - 
            calculateSceneSDF(pos - glm::vec3(0.0f, 0.0f, EPS), baseY).dist
        ));
    }


    // Lighting calculation
static glm::vec3 calculateLighting(const glm::vec3& pos, const glm::vec3& normal, 
                                        const std::vector<Light>& lights, 
                                        float ambientStrength = 0.1f,
                                        float brightnessMultiplier = 1.5f) {
        glm::vec3 totalLighting(0.0f);
        
        // Ambient component
        totalLighting += ambientStrength * glm::vec3(1.0f);
        
        // Iterate through all lights
        for (const auto& light : lights) {
            // Calculate light direction and distance
            glm::vec3 lightDir = glm::normalize(light.pos - pos);
            float lightDistance = glm::distance(light.pos, pos);
            
            // Diffuse calculation with distance falloff
            float diff = std::max(glm::dot(normal, lightDir), 0.0f);
            float distanceFalloff = 1.0f / (lightDistance * lightDistance + 1.0f);
            
            // Accumulate lighting from this light
            totalLighting += diff * light.emissionColor * distanceFalloff;
        }
        
        // Apply brightness multiplier
        return glm::clamp(totalLighting * brightnessMultiplier, glm::vec3(0.0f), glm::vec3(1.0f));
    }

    // Ray casting
    static Material rayCast(const glm::vec3& cameraOrigin, const glm::vec3& cameraDir, 
                             float baseY, int numSteps, float startDist, float maxDist) {
        Material mat{glm::vec3(0.0f), startDist};
        Material defaultMat{glm::vec3(0.0f), -1.0f};
        
        for (int i = 0; i < numSteps; ++i) {
            glm::vec3 pos = cameraOrigin + cameraDir * mat.dist;
            Material result = calculateSceneSDF(pos, baseY);

            // Case 1: intersected scene
            if (std::abs(result.dist) < MIN_DIST * mat.dist) {
                break;
            }

            mat.dist += result.dist;
            mat.color = result.color;
            
            // Case 2: out of the scene entirely
            if (mat.dist > maxDist) {
                return defaultMat;
            }
        }

        return mat;
    }

    static float calculateShadow(const glm::vec3& pos, const glm::vec3& lightPos, 
                                  float baseY, int numSteps = 64, 
                                  float startDist = 0.01f, float maxDist = 10.0f) {
        glm::vec3 lightDir = glm::normalize(lightPos - pos);
        Material result = rayCast(pos, lightDir, baseY, numSteps, startDist, maxDist);
        return result.dist >= 0.0f ? 0.0f : 1.0f;
    }

public:
    // Main ray marching function
static glm::vec3 rayMarch(const glm::vec3& cameraOrigin, const glm::vec3& cameraDir, 
                            const std::vector<Light>& lights,
                            glm::vec3 skyColor=glm::vec3(0.f,0.f,0.f),
                            float baseY = 0.0f,
                            float brightnessMultiplier = 7.0f) 
                            {
        Material mat = rayCast(cameraOrigin, cameraDir, baseY, NUM_STEPS, 1.0f, MAX_DIST);

        // Sky color calculation
        // float skyFactor = std::exp(saturate(cameraDir.y) * -40.0f);
        // glm::vec3 skyColor = glm::exp(-cameraDir.y / glm::vec3(0.025f, 0.0165f, 0.1f));
        // skyColor = glm::mix(skyColor, glm::vec3(0.025f, 0.0165f, 0.0f), skyFactor);
        
        if (mat.dist < 0.0f) {
            return skyColor;
        }

        glm::vec3 pos = cameraOrigin + mat.dist * cameraDir;
        glm::vec3 normal = calculateNormal(pos, baseY);

        float totalShadow = 0.0f;
        for (const auto& light : lights) {
            totalShadow += calculateShadow(pos, light.pos, baseY);
        }
        totalShadow = glm::clamp(totalShadow / lights.size(), 0.0f, 1.0f);

        // Calculate lighting from all lights
        glm::vec3 lighting = calculateLighting(pos, normal, lights, 0.1f, brightnessMultiplier);

        // Apply shadow
        lighting *= totalShadow;

        glm::vec3 color = mat.color * lighting;

        // Fog Calculation
        float fogDist = glm::distance(cameraOrigin, pos);
        float inscatter = 1.0f - std::exp(-fogDist * fogDist * 0.0003f);
        float extinction = std::exp(-fogDist * fogDist * 0.02f);
        glm::vec3 fogColor = glm::vec3(0.025f, 0.0165f, 0.1f);

        color = color * extinction + fogColor * inscatter;
        return color;
    }
};