#define rendered texture0
#define bloom texture1
#define depthmap texture2

uniform sampler2D rendered;
uniform sampler2D bloom;
uniform sampler2D depthmap;

uniform mediump float exposureFactor;
uniform lowp float bloomIntensity;

uniform vec3 sunPositionScreen;
uniform float sunBrightness;
uniform vec3 moonPositionScreen;
uniform float moonBrightness;

uniform vec3 v_LightDirection;

uniform vec3 dayLight;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

const float far = 20000.;
const float near = 1.;
float mapDepth(float depth)
{
	depth = 2. * depth - 1.;
	return 2. * near * far / (far + near - depth * (far - near));
}

#ifdef ENABLE_BLOOM

vec4 applyBloom(vec4 color, vec2 uv)
{
	vec3 light = texture2D(bloom, uv).rgb;
#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 && uv.y < 0.5)
		return vec4(light, color.a);
	if (uv.x < 0.5)
		return light;
#endif
	color.rgb = mix(color.rgb, light, bloomIntensity);
	return color;
}

#endif

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
	const float exposureBias = 2.0;
	color.rgb = uncharted2Tonemap(exposureBias * color.rgb);
	// Precalculated white_scale from
	//vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
	vec3 whiteScale = vec3(1.036015346);
	color.rgb *= whiteScale;
	return color;
}
#endif

vec3 getScatteringDecay(vec2 uv, float depth)
{
	// Based on talk at 2002 Game Developers Conference by Naty Hoffman and Arcot J. Preetham
	const float beta_r0 = 1.5e-5; // Rayleigh scattering beta
	const float beta_m0 = 1e-7; // Mie scattering beta
	const float depth_scale = 50.; // how many world meters in 0.1 game node
	const float depth_offset = 0.; // how far does the fog start
	const float max_depth = 1e5;  // in world meters

	// These factors are calculated based on expected value of scattering factor of 1e-5
	// for Nitrogen at 532nm (green), 2e25 molecules/m3 in atmosphere
	// 
	const vec3 beta_r0_l = vec3(3.3362176e-01, 8.75378289198826e-01, 1.95342379700656) * beta_r0; // wavelength-dependent scattering

	vec3 f_ex = exp(-(beta_r0_l + beta_m0) * clamp(depth_scale * (depth - depth_offset), 0., max_depth));

	return f_ex;
}

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec4 color = texture2D(rendered, uv).rgba;
	float rawDepth = texture2D(depthmap, uv).r;
	float depth = mapDepth(rawDepth);

	// translate to linear colorspace (approximate)
	color.rgb = pow(color.rgb, vec3(2.2));

#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 || uv.y > 0.5)
#endif
	{
		color.rgb *= exposureFactor;
	}

#ifdef ENABLE_BLOOM
	color.rgb *= getScatteringDecay(uv, depth);
	color = applyBloom(color, uv);
#endif

#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 || uv.y > 0.5)
#endif
	{
#if ENABLE_TONE_MAPPING
		color = applyToneMapping(color);
#endif
	}

	color.rgb = clamp(color.rgb, vec3(0.), vec3(1.));

	// return to sRGB colorspace (approximate)
	color.rgb = pow(color.rgb, vec3(1.0 / 2.2));

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
