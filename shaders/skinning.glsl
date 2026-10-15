layout (location = 3) in vec4 aJoints;
layout (location = 4) in vec4 aWeights;
layout (std140) uniform SkinMatrices {
    mat4 uJointMatrices[MODEL_MAX_SKIN_JOINTS];
};
uniform bool uSkinned;
mat4 skinMatrix() {
    if (!uSkinned) return mat4(1.0);
    return aWeights.x * uJointMatrices[int(aJoints.x)] +
           aWeights.y * uJointMatrices[int(aJoints.y)] +
           aWeights.z * uJointMatrices[int(aJoints.z)] +
           aWeights.w * uJointMatrices[int(aJoints.w)];
}
