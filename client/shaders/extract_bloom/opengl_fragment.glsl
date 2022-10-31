#define rendered texture0
#define depthmap texture1

uniform sampler2D rendered;
uniform sampler2D depthmap;

uniform mediump float exposureFactor;
uniform mediump float bloomStrength;

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
	float samples = 20.;
	float result = texture2D(depthmap, uv).r < 1. ? 0.0 : 1.0;
	float bias = noise(vec3(uv, rawDepth));
	vec2 samplepos;
	for (float i = 1.; i < samples; i++) {
		samplepos = mix(uv, lightVec.xy, (i + bias) / samples);
		result += texture2D(depthmap, samplepos).r < 1. ? 0.0 : 1.0;
	}
	return result / samples;
}

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec3 color = texture2D(rendered, uv).rgb;
	// translate to linear colorspace (approximate)
	color = pow(color, vec3(2.2));

	// Scale colors by luminance to amplify bright colors
	// in SDR textures.
	float luminance = dot(color, vec3(0.213, 0.515, 0.072));
	luminance *= luminance;
	color *= luminance * exposureFactor * bloomStrength;

	float rawDepth = texture2D(depthmap, uv).r;
	float depth = mapDepth(rawDepth);
	vec3 lookDirection = normalize(vec3(uv.x * 2. - 1., uv.y * 2. - 1., 1. / tan(36. / 180. * 3.141596)));
	vec3 lightSourceTint = vec3(1.0, 0.95, 0.6);
	lightSourceTint /= dot(lightSourceTint, vec3(0.213, 0.715, 0.072));
	vec3 lightColor = dayLight * lightSourceTint;
	float lightFactor = 0.;
	const float sunBoost = 3.;
	const float moonBoost = 0.33;

	if (sunPositionScreen.z > 0. && sunBrightness > 0.) {
		lightFactor = sunBoost * sunBrightness * sampleVolumetricLight(uv, sunPositionScreen, rawDepth) *
				(0.2 * pow(clamp(dot(sunPositionScreen, vec3(0., 0., 1.)), 0.0, 0.7), 2.5) + 0.8 * pow(max(0., dot(sunPositionScreen, lookDirection)), 8.));
	}
	else if (moonPositionScreen.z > 0. && moonBrightness > 0.) {
		lightFactor = moonBoost * moonBrightness * sampleVolumetricLight(uv, moonPositionScreen, rawDepth) *
				(0.2 * pow(clamp(dot(moonPositionScreen, vec3(0., 0., 1.)), 0.0, 0.7), 2.5) + 0.8 * pow(max(0., dot(moonPositionScreen, lookDirection)), 8.));
	}
	color.rgb += lightColor * lightFactor;

	// if (sunPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - sunPositionScreen.xy / sunPositionScreen.z) * 1000., 0., 1.);
	// if (moonPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - moonPositionScreen.xy / moonPositionScreen.z) * 1000., 0., 1.);

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
