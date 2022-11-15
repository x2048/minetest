#define rendered texture0
#define depthmap texture2

struct ExposureParams {
	float compensationFactor;
};

uniform sampler2D rendered;
uniform sampler2D depthmap;

uniform mediump float bloomStrength;
uniform ExposureParams exposureParams;

uniform vec3 sunPositionScreen;
uniform float sunBrightness;
uniform vec3 moonPositionScreen;
uniform float moonBrightness;

uniform vec3 dayLight;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

#ifdef ENABLE_AUTO_EXPOSURE
varying float exposure; // linear exposure factor, see vertex shader
#endif

const float far = 1000.;
const float near = 1.;
float mapDepth(float depth)
{
	return min(1., 1. / (1.00001 - depth) / far);
}

float noise(vec3 uvd) {
	return fract(dot(sin(uvd * vec3(13041.19699, 27723.29171, 61029.77801)), vec3(73137.11101, 37312.92319, 10108.89991)));
}

float sampleVolumetricLight(vec2 uv, vec3 lightVec, float rawDepth)
{
	lightVec = 0.5 * lightVec / lightVec.z + 0.5;
	const float samples = 30.;
	float result = texture2D(depthmap, uv).r < 1. ? 0.0 : 1.0;
	float bias = noise(vec3(uv, rawDepth));
	vec2 samplepos;
	for (float i = 1.; i < samples; i++) {
		samplepos = mix(uv, lightVec.xy, (i + bias) / samples);
		if (min(samplepos.x, samplepos.y) > 0. && max(samplepos.x, samplepos.y) < 1.)
			result += texture2D(depthmap, samplepos).r < 1. ? 0.0 : 1.0;
	}
	return result / samples;
}

vec3 lightToColor(float intensity)
{
	float temperature = 3750. + 3250. * pow(intensity, 0.25);

	// Aproximation of RGB values for specific color temperature in range of 2500K to 7500K
	// Based on the table at https://andi-siess.de/rgb-to-color-temperature/
	vec3 color = min(temperature * vec3(0., 9.11765e-05, 1.77451e-04) + vec3(1., 0.403431, -0.161275),
			temperature * vec3(-7.84314e-05, -6.27451e-05, 7.84314e-06) + vec3(1.50980, 1.40392, 0.941176));

	// scale the color for more saturation
	color = 1. - 2. * (1. - color);
	color /= dot(color, vec3(0.213, 0.715, 0.072));
	return color;
}

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec3 color = texture2D(rendered, uv).rgb;
	// translate to linear colorspace (approximate)
	color = pow(color, vec3(2.2));

	color *= exposureParams.compensationFactor * bloomStrength;

#ifdef ENABLE_AUTO_EXPOSURE
	color *= exposure;
#endif

	float rawDepth = texture2D(depthmap, uv).r;
	float depth = mapDepth(rawDepth);
	vec3 lookDirection = normalize(vec3(uv.x * 2. - 1., uv.y * 2. - 1., 1. / tan(36. / 180. * 3.141596)));
	vec3 lightSourceTint = vec3(1.0, 0.98, 0.4);
	float lightFactor = 0.;
	const float sunBoost = 12.0;
	const float moonBoost = 0.33;

	vec3 sourcePosition = vec3(-1., -1., -1);
	float boost = 0.;
	float brightness = 0.;
	if (sunPositionScreen.z > 0. && sunBrightness > 0.) {
		boost = sunBoost;
		brightness = sunBrightness;
		sourcePosition = sunPositionScreen;
	}
	else if (moonPositionScreen.z > 0. && moonBrightness > 0.) {
		lightSourceTint = vec3(0.4, 0.9, 1.);
		boost = moonBoost;
		brightness = moonBrightness * 0.33;
		sourcePosition = moonPositionScreen;
	}

	lightSourceTint /= dot(lightSourceTint, vec3(0.213, 0.715, 0.072));
	vec3 lightColor = dayLight * lightSourceTint;

	lightFactor = brightness * sampleVolumetricLight(uv, sourcePosition, rawDepth) *
			(0.05 * pow(clamp(dot(sourcePosition, vec3(0., 0., 1.)), 0.0, 0.7), 2.5) + 0.95 * pow(max(0., dot(sourcePosition, lookDirection)), 16.));

	color.rgb = mix(color.rgb, boost * lightSourceTint * pow(lightToColor(lightFactor * 1.1), vec3(2.)), lightFactor);

	// if (sunPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - sunPositionScreen.xy / sunPositionScreen.z) * 1000., 0., 1.);
	// if (moonPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - moonPositionScreen.xy / moonPositionScreen.z) * 1000., 0., 1.);

	gl_FragColor = vec4(color, 1.0); // force full alpha to avoid holes in the image.
}
