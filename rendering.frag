
#version 430


layout (binding = 0) uniform sampler2D simulationImage;
layout (location = 0) in vec2 UV;

layout (location = 0) out vec4 outColor;


#define M_PI 3.1415926535897932384626433832795
//fase della funzione d'onda
//normalizzata 0..1 per renderizzare con hsv
float phase(vec4 simState) {
    return acos(simState.x / length(simState)) / M_PI;    
}

//ampiezza della funzione d'onda
float amplitude(vec4 simState) {
    return simState.x * simState.x + simState.y * simState.y;
}

vec2 hash(vec2 seed) {
    return 
    vec2( 
        fract(sin((sin(seed.x * 10) * 133 + seed.y * 33) * 98) * 100),
        fract(sin((seed.x * 12 - 123 * cos(seed.y)) * 130 - 123) * 133)
    );
}

vec3 HSVtoRGB(vec3 hsv) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
} 

void main() {
    vec4 simState = texture(simulationImage, UV);
    outColor = vec4(HSVtoRGB(vec3(phase(simState), 1.0, amplitude(simState))), 1.0);
    //outColor = vec4(UV, 0, 1);
    //outColor = vec4(vec3(length(hash(UV))) * vec3(1.0 / length(vec3(1))), 1);
}