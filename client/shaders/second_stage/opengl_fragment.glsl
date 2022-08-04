uniform sampler2D baseTexture;
uniform sampler2D normalTexture;
uniform sampler2D ShadowMapSampler;

#define rendered baseTexture
#define normalmap normalTexture
#define depthmap ShadowMapSampler

const float far = 1.;
const float near = 1e-4;
float mapDepth(float depth)
{
	depth = near * far / (far - depth * (far - near));
	return clamp(pow(depth, 1.), 0.0, 1.0);
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

float F = far / 107.;
float A = F / 115. * 1680.;

float getCircleOfConfusion(float depth, float focalDepth) {
	return A * abs(1. - focalDepth / depth * (depth - F) / (focalDepth - F));
}

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

#define DOF
#ifdef DOF
	{
		vec2 pixel_size = vec2(1./1920., 1./1020.);

		float count = 1.;
		float focalOffset = 0.05;
#ifdef DYNAMIC_FOCUS
		float focalDepth =
				texture2D(depthmap, vec2(0.5, 0.5)).r +
				texture2D(depthmap, vec2(0.5 - focalOffset, 0.5 - focalOffset)).r +
				texture2D(depthmap, vec2(0.5 - focalOffset, 0.5 + focalOffset)).r +
				texture2D(depthmap, vec2(0.5 + focalOffset, 0.5 - focalOffset)).r +
				texture2D(depthmap, vec2(0.5 + focalOffset, 0.5 + focalOffset)).r;
		focalDepth = 2 * F;
#else
		float focalDepth = far / 300.;
#endif

		const float strength = 5.;

		float rawDepth = texture2D(depthmap, uv).r;
		float depth = mapDepth(rawDepth);
		float bokeh = length(uv - 0.5);
		float delta = clamp(getCircleOfConfusion(depth, focalDepth), 0., 1.);

// #define TEST
#ifdef TEST
		color.rgb = vec3(depth, focalDepth, delta);
#else
		float radius = strength;
		float steps = clamp(ceil(radius), 1., 5.);

		for (float x = -steps; x <= steps; x++)
		for (float y = -steps; y <= steps; y++) {
			if (x != 0. || y != 0.) {
				vec2 offset = vec2(x,y) / steps; // 0..1
				float l = length(offset);
				if (l < 1.) { // ignore pixels outside the radius
					vec2 _uv = uv + offset * pixel_size * radius;
					float rawSampleDepth = texture2D(depthmap, _uv).r;
					float sampleDepth = mapDepth(rawSampleDepth); // depth of the sample
					float sample_delta = clamp(getCircleOfConfusion(sampleDepth, focalDepth), 0., 1.); // CoC radius of the sample

					if (sample_delta > l /* we hit sample's CoC */ /*&& sampleDepth < depth + sample_delta / 20.*/ /* ray is not occluded */) {
						float gauss_weight = 1. - l;
						gauss_weight = gauss_weight * gauss_weight * (3. - 2. * gauss_weight);
						color += texture2D(rendered, _uv).rgba * gauss_weight; // color and alpha of the sample
						count += gauss_weight;
					}
				}
			}
		}

		color /= count;
#endif
	}
#endif

#ifdef BLOOM
	{
		vec2 pixel_size = vec2(1./1920., 1./1020.);
		const float bloom_weight = 1.;

		vec4 bloom = vec4(0.); // bloom value
		float count = 0.;

		float depth = mapDepth(texture2D(depthmap, uv).r); // own pixel depth
		float brightness = 0.2126*color.r + 0.7152*color.g + 0.0722*color.b; // own pixel brightness

		float d = 5. + clamp(10. / (1. + 15. * depth), 1., 10.); // blur radius
		float epsilon = 10. / d / d;

		for (float x = -d; x <= d; x++)
		for (float y = -d; y <= d; y++) {
			vec2 _uv = uv + vec2(x,y) * pixel_size * 2.;
			float sample_depth = mapDepth(texture2D(depthmap, _uv).r); // depth of the sample
			float l = length(vec2(x, y));
			if (l > 1. && sample_depth <= depth + 200. / far) {
				// only accept color from light sources closer to the camera than the pixel we generate
				vec4 sample = texture2D(rendered, _uv).rgba; // color and alpha of the sample
				float sample_brightness = 0.2126*sample.r + 0.7152*sample.g + 0.0722*sample.b; // brightness of the sample
				if (sample_brightness >= 0.3) {
					vec4 delta = max(vec4(0.), sample.rgba * pow(max(0., 1. - l/d), 0.8) - color.rgba);
					if (delta.r + delta.g + delta.b > 0.) {
						// sample emits light
						bloom.rgba += delta;
						count += 1.;
					}
				}
			}
		}

		if (count > 1.)
			bloom /= count;

		color.rgba += bloom_weight * bloom.rgba;
		// color = bloom;
	}
#endif



#ifdef CEL_SHADING
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
#endif

#ifdef ENABLE_TONE_MAPPING
	color = applyToneMapping(color);
#endif

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
#endif
}
