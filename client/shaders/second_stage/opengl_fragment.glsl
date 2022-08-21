uniform sampler2D baseTexture;
uniform sampler2D normalTexture;
uniform sampler2D ShadowMapSampler;

#define rendered baseTexture
#define normalmap normalTexture
#define depthmap ShadowMapSampler

const float far = 20000.;
const float near = 1.;
float mapDepth(float depth)
{
	depth = 2. * near / (far + near - depth * (far - near));
	return clamp(pow(depth, 1./2.5), 0.0, 1.0);
}

#if ENABLE_TONE_MAPPING

/* Hable's UC2 Tone mapping parameters
	A = 0.22;
	B = 0.30;
	C = 0.10;
	D = 0.20;
	E = 0.01;
	F = 0.30;
	W = 11.2;
	equation used:  ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F
*/

vec3 uncharted2Tonemap(vec3 x)
{
	return ((x * (0.22 * x + 0.03) + 0.002) / (x * (0.22 * x + 0.3) + 0.06)) - 0.03333;
}

vec4 applyToneMapping(vec4 color)
{
	color = vec4(pow(color.rgb, vec3(2.2)), color.a);
	const float gamma = 1.6;
	const float exposureBias = 5.5;
	color.rgb = uncharted2Tonemap(exposureBias * color.rgb);
	// Precalculated white_scale from
	//vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
	vec3 whiteScale = vec3(1.036015346);
	color.rgb *= whiteScale;
	return vec4(pow(color.rgb, vec3(1.0 / gamma)), color.a);
}
#endif

void main(void)
{
	vec2 uv = gl_TexCoord[0].st;
	vec4 color = texture2D(rendered, uv).rgba;
#ifdef SECONDSTAGE_DEBUG
	vec4 normal_and_depth = texture2D(normalmap, uv);
	vec3 normal = normal_and_depth.rgb;
	float draw_type = normal_and_depth.a * 256. / 25.;
	float depth = mapDepth(texture2D(depthmap, uv).r);

	if (uv.x < 0.5 && uv.y < 0.5)
		gl_FragColor = color;
	else if (uv.y < 0.5)
		gl_FragColor = vec4(depth, depth, depth, 1);
	else if (uv.x < 0.5)
		gl_FragColor = vec4(normal, 1);
	else
		gl_FragColor = vec4(draw_type, draw_type, draw_type, 1);
#else

	color.rgb = floor(color.rgb * 16. + .5) / 16.;


	float depth = mapDepth(texture2D(depthmap, uv).r);
	float edge = 0.0;
	vec2 pixel_size = vec2(1./1960., 1./1080.);
	for (float x = -pixel_size.x; x <= pixel_size.x; x += pixel_size.x)
	for (float y = -pixel_size.y; y <= pixel_size.y; y += pixel_size.y)
	{
		edge += abs(mapDepth(texture2D(depthmap, uv + vec2(x,y)).r) - depth) * far / 300.;
	}

	color.rgb *= 1.0 - min(1.0, edge);

#ifdef ENABLE_TONE_MAPPING
	color = applyToneMapping(color);
#endif

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
#endif
}
