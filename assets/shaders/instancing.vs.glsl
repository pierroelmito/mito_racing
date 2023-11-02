#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

in mat4 instanceTransform;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    mat4 tr = instanceTransform;
    float a = tr[3][3];
    tr[3][3] = 1;
    mat4 mvpi = mvp*tr;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor * vec4(1, 1, 1, a);
    gl_Position = mvpi*vec4(vertexPosition, 1.0);
}
