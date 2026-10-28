uniform sampler2D uAoDepth;
uniform bool uAoEnabled;
uniform vec3 uAoCamera;      // Near plane, far plane, tangent of half vertical FOV.
uniform vec3 uAoParameters;  // World radius, strength, depth bias.

float aoLinearDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * uAoCamera.x * uAoCamera.y) /
           max(uAoCamera.y + uAoCamera.x - z * (uAoCamera.y - uAoCamera.x), 0.00001);
}

vec3 aoViewPosition(vec2 uv, float depth, vec2 screenSize) {
    float linearDepth = aoLinearDepth(depth);
    vec2 ndc = uv * 2.0 - 1.0;
    float aspect = screenSize.x / max(screenSize.y, 1.0);
    return vec3(ndc.x * aspect * uAoCamera.z, ndc.y * uAoCamera.z, -1.0) * linearDepth;
}

float screenSpaceAmbientVisibility(vec2 uv) {
    if (!uAoEnabled) {
        return 1.0;
    }
    vec2 screenSize = vec2(textureSize(uAoDepth, 0));
    ivec2 size = textureSize(uAoDepth, 0);
    ivec2 pixel = clamp(ivec2(uv * screenSize), ivec2(0), size - 1);
    vec2 centerUv = (vec2(pixel) + 0.5) / screenSize;
    float depth = texelFetch(uAoDepth, pixel, 0).r;
    if (depth >= 0.999999) {
        return 1.0;
    }
    vec3 position = aoViewPosition(centerUv, depth, screenSize);
    // Derivatives of a nearest-filtered depth texture alternate between zero and
    // depth steps after lens warping. Reconstruct the plane from actual adjacent
    // depth texels instead, choosing the neighbor on the same side of an edge.
    ivec2 left = max(pixel - ivec2(1, 0), ivec2(0));
    ivec2 right = min(pixel + ivec2(1, 0), size - 1);
    ivec2 down = max(pixel - ivec2(0, 1), ivec2(0));
    ivec2 up = min(pixel + ivec2(0, 1), size - 1);
    float dl = texelFetch(uAoDepth, left, 0).r;
    float dr = texelFetch(uAoDepth, right, 0).r;
    float dd = texelFetch(uAoDepth, down, 0).r;
    float du = texelFetch(uAoDepth, up, 0).r;
    vec3 pl = aoViewPosition((vec2(left) + 0.5) / screenSize, dl, screenSize);
    vec3 pr = aoViewPosition((vec2(right) + 0.5) / screenSize, dr, screenSize);
    vec3 pd = aoViewPosition((vec2(down) + 0.5) / screenSize, dd, screenSize);
    vec3 pu = aoViewPosition((vec2(up) + 0.5) / screenSize, du, screenSize);
    vec3 dx = abs(dl - depth) < abs(dr - depth) ? position - pl : pr - position;
    vec3 dy = abs(dd - depth) < abs(du - depth) ? position - pd : pu - position;
    vec3 planeNormal = cross(dx, dy);
    if (dot(planeNormal, planeNormal) < 1e-12) {
        return 1.0;
    }
    vec3 normal = normalize(planeNormal);
    if (dot(normal, -position) < 0.0) {
        normal = -normal;
    }

    float linearDepth = -position.z;
    float radius = clamp(uAoParameters.x, 0.05, 2.0);
    float radiusPixels = clamp(radius * screenSize.y / (2.0 * uAoCamera.z * linearDepth), 1.5, 12.0);
    const vec2 kernel[8] = vec2[](
        vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
        vec2(0.7071, 0.7071), vec2(-0.7071, 0.7071),
        vec2(0.7071, -0.7071), vec2(-0.7071, -0.7071)
    );

    float occlusion = 0.0;
    for (int i = 0; i < 8; ++i) {
        // Two radii keep the cost fixed while catching both close contacts and
        // the broader creases between nearby scene surfaces.
        float ring = (i < 4) ? 0.62 : 1.0;
        vec2 sampleUv = clamp(centerUv + kernel[i] * (radiusPixels * ring) / screenSize,
                              vec2(0.001), vec2(0.999));
        ivec2 samplePixel = clamp(ivec2(sampleUv * screenSize), ivec2(0), size - 1);
        sampleUv = (vec2(samplePixel) + 0.5) / screenSize;
        float sampleDepth = texelFetch(uAoDepth, samplePixel, 0).r;
        if (sampleDepth >= 0.999999) {
            continue;
        }
        vec3 delta = aoViewPosition(sampleUv, sampleDepth, screenSize) - position;
        float distanceToSample = length(delta);
        if (distanceToSample <= 0.0001 || distanceToSample >= radius) {
            continue;
        }
        float aboveSurface = max(dot(normal, delta) - uAoParameters.z, 0.0);
        float distanceFade = 1.0 - smoothstep(radius * 0.55, radius, distanceToSample);
        occlusion += clamp(aboveSurface / distanceToSample, 0.0, 1.0) * distanceFade;
    }

    float strength = clamp(uAoParameters.y, 0.0, 3.0);
    return clamp(1.0 - occlusion * strength * 0.55, 0.42, 1.0);
}
