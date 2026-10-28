in vec4 vShadowPosition;
in vec3 vDitherPosition;
uniform sampler2DShadow uShadowMap;
uniform float uShadowDitherStrength = 0.0;
uniform bool uDitheredShadows;
uniform float uDitheredShadowDarkness = 0.8;

float shadowBlend(float visibility, vec3 position, vec3 dx, vec3 dy) {
    if (uDitheredShadows) {
        // Saturn-inspired mesh transparency: draw the shadow on alternate
        // native raster pixels, keeping the lit material visible in the holes.
        // This includes the umbra, and never discards the receiver's geometry.
        ivec2 pixel = ivec2(floor(gl_FragCoord.xy));
        return ((pixel.x ^ pixel.y) & 1) == 0 ? visibility : 1.0;
    }
    if (uShadowDitherStrength <= 0.0 || visibility <= 0.0 || visibility >= 1.0) {
        return visibility;
    }
    // Ordered coverage blends the actual lit/shadowed colors, rather than
    // adding RGB noise. Only the filtered shadow's transition band is affected.
    const int bayer[16] = int[16](
        0, 8, 2, 10,
        12, 4, 14, 6,
        3, 11, 1, 9,
        15, 7, 13, 5);
    // Project onto the surface's dominant local plane, independent of camera,
    // light, texture tiling or time. The grid never changes density with distance.
    // Prefer Z, then Y at near-equal angles so derivative rounding cannot
    // alternate the projection plane on diagonal faces as the camera moves.
    vec3 normal = abs(cross(dx, dy)) * vec3(0.98, 0.99, 1.0);
    vec2 uv = position.xy;
    vec2 du = dx.xy;
    vec2 dv = dy.xy;
    if (normal.x > normal.y && normal.x > normal.z) {
        uv = position.yz;
        du = dx.yz;
        dv = dy.yz;
    } else if (normal.y > normal.z) {
        uv = position.xz;
        du = dx.xz;
        dv = dy.xz;
    }
    const float density = 48.0; // Fixed cells per world unit in the scaled mesh.
    ivec2 cell = ivec2(floor(uv * density));
    float threshold = (float(bayer[(cell.y & 3) * 4 + (cell.x & 3)]) + 0.5) / 16.0;
    // Unresolved cells fade into the original filtered shadow instead of
    // aliasing. A soft threshold avoids abrupt flips from tiny coverage changes.
    vec2 footprint = (abs(du) + abs(dv)) * density;
    float resolved = 1.0 - smoothstep(0.75, 1.5, max(footprint.x, footprint.y));
    float covered = smoothstep(threshold - 1.0 / 32.0, threshold + 1.0 / 32.0, visibility);
    return mix(visibility, covered, uShadowDitherStrength * resolved);
}

float ditheredShadowAmbient(float visibility, float diffuse) {
    // Half the mesh exposes full lighting. Give its covered pixels enough
    // contrast by attenuating ambient/sky fill as well as shadowing the sun.
    // Filtered edge coverage fades the attenuation; lit holes remain intact.
    // A sun-away face is already shaded, not a receiver of a cast sun shadow.
    // Fade this extra contrast at grazing angles so self-occlusion cannot put
    // a checkerboard over an otherwise uniformly ambient-lit back face.
    float facing = smoothstep(0.0, 0.1, diffuse);
    return uDitheredShadows ? 1.0 - uDitheredShadowDarkness * (1.0 - visibility) * facing : 1.0;
}

float ambientOcclusionVisibility(float visibility, float diffuse) {
    // Let the existing scene-wide shadow map occlude part of the sky/ambient
    // fill as well as direct sunlight. Keeping this softer than direct shadow
    // visibility grounds every receiver without adding another render pass.
    float strength = 0.42;
    if (uDitheredShadows) {
        // Match the shadow mesh's own pixel mask: the untouched pixels remain
        // fully lit, and sun-away faces do not acquire a checkerboard.
        ivec2 pixel = ivec2(floor(gl_FragCoord.xy));
        if (((pixel.x ^ pixel.y) & 1) != 0) {
            return 1.0;
        }
        strength *= smoothstep(0.0, 0.1, diffuse);
    }
    return mix(1.0, clamp(visibility, 0.0, 1.0), strength);
}

float sunVisibility(float diffuse) {
    vec3 projected = vShadowPosition.xyz / vShadowPosition.w * 0.5 + 0.5;
    vec2 depthGradient = vec2(0.0);
    vec3 ditherDx = vec3(0.0);
    vec3 ditherDy = vec3(0.0);
    if (uShadowDitherStrength > 0.0 || uDitheredShadows) {
        ditherDx = dFdx(vDitherPosition);
        ditherDy = dFdy(vDitherPosition);
        // Compare each PCF tap against the receiver's depth at that tap, not
        // the center depth. Otherwise a sloped lit plane can shadow itself,
        // and ordered coverage would turn that small error into dark speckles.
        vec3 dx = dFdx(projected);
        vec3 dy = dFdy(projected);
        float determinant = dx.x * dy.y - dx.y * dy.x;
        if (abs(determinant) > 1e-12) {
            depthGradient = vec2(dx.z * dy.y - dy.z * dx.y,
                                 dx.x * dy.z - dy.x * dx.z) / determinant;
        }
    }
    if (any(lessThanEqual(projected, vec3(0.0))) || any(greaterThanEqual(projected, vec3(1.0)))) {
        return 1.0;
    }
    float bias = mix(0.00035, 0.00006, clamp(diffuse, 0.0, 1.0));
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    // The comparison sampler also covers half a texel around each tap.
    bias += dot(abs(depthGradient), texel) * 0.5;
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(x, y) * texel;
            visibility += texture(uShadowMap, vec3(projected.xy + offset,
                                                   projected.z + dot(offset, depthGradient) - bias));
        }
    }
    return shadowBlend(visibility / 9.0, vDitherPosition, ditherDx, ditherDy);
}

float shadowedLight(float light, float ambient) {
    float direct = max(light - ambient, 0.0);
    float diffuse = direct / max(1.0 - ambient, 0.0001);
    float visibility = sunVisibility(diffuse);
    return min(light, ambient) * ditheredShadowAmbient(visibility, diffuse) + direct * visibility;
}
