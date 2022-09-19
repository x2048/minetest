uniform sampler2D baseTexture;

#define rendered baseTexture

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

const float contrast = 1.0;
const float brightness = 0.0;

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

vec3 brightnessContrast(vec3 color, float brightness, float contrast)
{
	vec3 increment = color * brightness;
	color += min(increment, -increment) + max(0., brightness);
	return max((color - 0.5) * contrast + 0.5, vec3(0.));
}

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec4 color = texture2D(rendered, uv).rgba;

#if ENABLE_TONE_MAPPING
	color = applyToneMapping(color);
#endif

	// brightness - contrast correction
	color.rgb = brightnessContrast(color.rgb, brightness, contrast);

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
