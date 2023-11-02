#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main()
{
	finalColor = colDiffuse*fragColor;
	int count = 8;
	float scale = 0.1;
	for (int i = 0; i < count; ++i) {
		float a = i * 3 * 3.14 / (count - 1);
		vec2 d = vec2(cos(a), sin(a));
		vec2 n = vec2(-d.y, d.x);
		vec4 texelColor = texture(texture0, ((15 + 4 * i) / scale) * (fragTexCoord.x * d + fragTexCoord.y * n));
		finalColor.rgb = mix(finalColor.rgb, vec3(1, 0.5, 0.5) * texelColor.rgb, 0.2f * texelColor.a);
	}
}
