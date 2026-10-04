uniform float uLensStrength;
uniform float uLensAspect;

// An inverse barrel warp, applied once to the complete camera image. Measuring
// radius in aspect-correct coordinates keeps the lens circular after resizing.
vec2 lensUv(vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    vec2 radial = p * vec2(uLensAspect, 1.0);
    float radiusSquared = dot(radial, radial) / (1.0 + uLensAspect * uLensAspect);
    // Fit the warped image inside its original rectangle: no empty corners or
    // stretched border texels. Strength zero is an exact identity mapping.
    return (p * (1.0 + uLensStrength * radiusSquared) / (1.0 + uLensStrength)) * 0.5 + 0.5;
}
