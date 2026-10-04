#pragma once

namespace engine::graphics {
class Texture;
struct SurfaceMaterial {
    float roughness = 0.85f; // Perceptual microfacet roughness, [0, 1].
    float metallic = 0; // 0: dielectric; 1: base color tints the metal's reflection.
    float reflectivity = 1; // Specular multiplier, [0, 1]; 0 disables reflections.
};
// Borrowed linear data maps, sampled with the surface's UVs. G/B multiply
// roughness/metallic; optional packed A multiplies reflectivity. Occlusion uses R.
struct MaterialTextures {
    const Texture* properties = nullptr;
    const Texture* normal = nullptr;
    const Texture* occlusion = nullptr;
    float normalScale = 1;
    float occlusionStrength = 1;
    bool packedReflectivity = false; // Standard glTF metallic-roughness maps ignore R/A.
    const Texture* glass = nullptr; // Optional facade coat mask in R; paired property maps define the whole finish.
};
struct WindowMaterial {
    float roughness = 0.24f; // Smooth glass by default; higher values give frosted reflections.
    float reflectivity = 1; // Scales glass Fresnel response (4% at normal incidence), [0, 1].
};
} // namespace engine::graphics
