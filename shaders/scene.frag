#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
out vec4 fragColor;
uniform vec3 uEye;
uniform vec3 uAmbient;
uniform vec3 uDirDirection;
uniform vec3 uDirColor;
uniform vec3 uPointPosition;
uniform vec3 uPointColor;
uniform vec3 uAttenuation;
uniform vec3 uAlbedo;
uniform float uSpecular;
uniform float uShininess;
uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform float uUvScale;
vec3 safeNormalize(vec3 value) {
    return value * inversesqrt(max(dot(value, value), 1e-12));
}
vec3 lightContribution(vec3 N, vec3 V, vec3 L, vec3 color, vec3 albedo) {
    float diffuse = max(dot(N,L), 0.0);
    vec3 H = safeNormalize(L+V);
    // Do not light the back of a surface with a specular highlight.
    float specular = diffuse > 0.0 ? pow(max(dot(N,H),0.0),uShininess) : 0.0;
    return color * (albedo*diffuse + vec3(uSpecular*specular));
}
vec3 linearToSrgb(vec3 linearColor) {
    vec3 c = clamp(linearColor, 0.0, 1.0);
    vec3 low = 12.92*c;
    vec3 high = 1.055*pow(c, vec3(1.0/2.4))-0.055;
    return mix(high, low, lessThanEqual(c, vec3(0.0031308)));
}
void main() {
    vec3 albedo = uAlbedo;
    if (uUseTexture) albedo *= texture(uTexture, vUv*uUvScale).rgb;
    vec3 N = safeNormalize(vNormal);
    vec3 V = safeNormalize(uEye-vWorldPosition);
    vec3 toPoint = uPointPosition-vWorldPosition;
    float d = length(toPoint);
    float attenuation = 1.0 / max(uAttenuation.x+uAttenuation.y*d+uAttenuation.z*d*d, 1e-6);
    vec3 linearColor = uAmbient*albedo;
    linearColor += lightContribution(N,V,safeNormalize(-uDirDirection),uDirColor,albedo);
    linearColor += attenuation * lightContribution(N,V,safeNormalize(toPoint),uPointColor,albedo);
    fragColor = vec4(linearToSrgb(linearColor),1.0);
}
