#version 430

layout (location = 0) out vec2 UV;

//renderizza un quadrilatero fullscreen
void main() {
    vec2 vertices[] = {
        vec2(-1, -1),
        vec2(-1, 1),
        vec2(1, -1),
        vec2(1, 1)
    };
    vec2 uvs[] = {
        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 0),
        vec2(1, 1)
    };

    gl_Position = vec4(vertices[gl_VertexIndex], 0.0f, 1.0f);
    UV = uvs[gl_VertexIndex];
}