uniform bool uThreePoint;
uniform bool uTextureRepeat;
uniform int uMaxTextureLod;

vec4 surfaceTexel(sampler2D tex, ivec2 pixel, int level, bool repeatTexture) {
    ivec2 size = textureSize(tex, level);
    pixel = repeatTexture ? (pixel % size + size) % size : clamp(pixel, ivec2(0), size - 1);
    return texelFetch(tex, pixel, level);
}

// Interpolate within one of the two triangles in a texel cell. Each triangle
// uses three texels, preserving the characteristic diagonal N64 filter pattern.
vec4 threePointLevel(sampler2D tex, vec2 uv, int level, bool repeatTexture) {
    vec2 pixel = uv * vec2(textureSize(tex, level)) - 0.5;
    ivec2 base = ivec2(floor(pixel));
    vec2 f = fract(pixel);
    vec4 right = surfaceTexel(tex, base + ivec2(1, 0), level, repeatTexture);
    vec4 upper = surfaceTexel(tex, base + ivec2(0, 1), level, repeatTexture);
    if (f.x + f.y <= 1.0) {
        vec4 corner = surfaceTexel(tex, base, level, repeatTexture);
        return corner * (1.0 - f.x - f.y) + right * f.x + upper * f.y;
    }
    vec4 corner = surfaceTexel(tex, base + ivec2(1, 1), level, repeatTexture);
    return corner * (f.x + f.y - 1.0) + upper * (1.0 - f.x) + right * (1.0 - f.y);
}

vec4 threePointTexture(sampler2D tex, vec2 uv) {
    vec2 pixel = uv * vec2(textureSize(tex, 0));
    float footprint = max(length(dFdx(pixel)), length(dFdy(pixel)));
    // Favor slightly finer surface detail while retaining mipmaps in the distance.
    const float detailBias = -0.5;
    float lod = clamp(log2(max(footprint, 1.0)) + detailBias, 0.0, float(uMaxTextureLod));
    int lower = int(floor(lod));
    // Blend adjacent mip levels to reduce distant texture shimmer without
    // replacing the three-point reconstruction with hardware bilinear filtering.
    return mix(threePointLevel(tex, uv, lower, uTextureRepeat),
               threePointLevel(tex, uv, min(lower + 1, uMaxTextureLod), uTextureRepeat), fract(lod));
}

vec4 sampleSurface(sampler2D tex, vec2 uv) {
    return uThreePoint ? threePointTexture(tex, uv) : texture(tex, uv);
}

vec4 sampleSurfaceLod(sampler2D tex, vec2 uv, float lod) {
    return uThreePoint ? threePointTexture(tex, uv) : textureLod(tex, uv, lod);
}
