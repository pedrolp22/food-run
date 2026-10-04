uniform bool uTextureRepeat;
uniform int uMaxTextureLod;
uniform bool uPsxMinification;
uniform float uPsxLodBias;

vec4 surfaceTexel(sampler2D tex, ivec2 pixel, int level, bool repeatTexture) {
    ivec2 size = textureSize(tex, level);
    pixel = repeatTexture ? (pixel % size + size) % size : clamp(pixel, ivec2(0), size - 1);
    return texelFetch(tex, pixel, level);
}

vec4 sampleSurface(sampler2D tex, vec2 uv) {
    return texture(tex, uv);
}

vec4 sampleSurfaceLod(sampler2D tex, vec2 uv, float lod) {
    return textureLod(tex, uv, lod);
}

// Optional hybrid PSX minification: visible texels stay nearest sampled, while
// subpixel texture detail fades through the mip chain instead of sparkling.
// Measure the perspective UV footprint separately from the affine sample UVs,
// so triangle warping does not create diagonal mip boundaries.
vec4 sampleSurfaceLod(sampler2D tex, vec2 uv, float lod, vec2 perspectiveUv) {
    if (!uPsxMinification) {
        return sampleSurfaceLod(tex, uv, lod);
    }
    vec2 pixel = perspectiveUv * vec2(textureSize(tex, 0));
    float footprint = max(length(dFdx(pixel)), length(dFdy(pixel)));
    float biasedFootprint = max(footprint * exp2(uPsxLodBias), 1.0);
    float filteredLod = clamp(log2(biasedFootprint), 0.0, float(uMaxTextureLod));
    int lower = int(floor(filteredLod));
    int upper = min(lower + 1, uMaxTextureLod);
    vec4 fine = surfaceTexel(tex, ivec2(floor(uv * vec2(textureSize(tex, lower)))), lower, uTextureRepeat);
    vec4 coarse = surfaceTexel(tex, ivec2(floor(uv * vec2(textureSize(tex, upper)))), upper, uTextureRepeat);
    // Continuous crossfades avoid discrete mip popping; no bilinear texel blur
    // or animated/dithered sample locations are added to the native raster.
    return mix(fine, coarse, fract(filteredLod));
}
