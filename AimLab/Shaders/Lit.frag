#version 330 core
const int MAX_LIGHTS = 8;

struct Light
{
	int type;
	vec3 color;
	float intensity;
	vec3 position;
	vec3 direction;
	float range;
	float innerCos;
	float outerCos;
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;
uniform vec3 ambientLight;
uniform vec3 viewPosition;
uniform vec3 materialColor;
uniform float shininess;
uniform bool unlit;
uniform bool ambientOnly;
uniform bool useTexture;
uniform sampler2D colorTexture;
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;
out vec4 fragColor;

vec3 SafeNormalize(vec3 value)
{
	return dot(value, value) > 0.0 ? normalize(value) : vec3(0.0);
}

void main()
{
	vec4 base = vec4(materialColor, 1.0);

	if (useTexture)
	{
		base = texture(colorTexture, uv);
	}

	if (unlit)
	{
		fragColor = base;
		return;
	}

	if (ambientOnly)
	{
		fragColor = vec4(ambientLight * base.rgb, base.a);
		return;
	}

	vec3 n = SafeNormalize(worldNormal);
	vec3 viewDirection = SafeNormalize(viewPosition - worldPosition);
	vec3 result = ambientLight;

	for (int i = 0; i < lightCount; ++i)
	{
		vec3 toLight;
		float attenuation = 1.0;

		if (lights[i].type == 0)
		{
			toLight = -SafeNormalize(lights[i].direction);
		}
		else
		{
			vec3 offset = lights[i].position - worldPosition;
			float distance = length(offset);
			toLight = SafeNormalize(offset);

			if (lights[i].range > 0.0)
			{
				float falloff = max(1.0 - distance / lights[i].range, 0.0);
				attenuation = falloff * falloff;
			}

			if (lights[i].type == 2)
			{
				float angleCos = dot(-toLight, SafeNormalize(lights[i].direction));
				attenuation *= smoothstep(lights[i].outerCos, lights[i].innerCos, angleCos);
			}
		}

		float diffuse = max(dot(n, toLight), 0.0);
		float specular = pow(max(dot(viewDirection, reflect(-toLight, n)), 0.0), max(shininess, 1.0));
		vec3 lightColor = lights[i].color * lights[i].intensity * attenuation;
		result += lightColor * diffuse;
		result += lightColor * specular;
	}

	fragColor = vec4(base.rgb * result, base.a);
}
