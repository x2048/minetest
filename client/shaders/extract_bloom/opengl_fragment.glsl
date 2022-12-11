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

uniform float bloomIntensity;

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

vec3 getInscatteredLight(vec2 uv, float depth, float rawDepth)
{
	vec3 lookDirection = normalize(vec3(uv.x * 2. - 1., uv.y * 2. - 1., 1.));
	vec3 lightSourceTint = dayLight;
	vec3 moonLight = dayLight * vec3(0.4, 0.9, 1.) * 0.33;

	vec3 sourcePosition = sunPositionScreen;

	if (moonPositionScreen.z > 0. && moonBrightness > 0.)
		sourcePosition = moonPositionScreen;

	sourcePosition = normalize(sourcePosition);

	// Based on talk at 2002 Game Developers Conference by Naty Hoffman and Arcot J. Preetham
	const float beta_r0 = 1.5e-5; // Rayleigh scattering beta
	const float beta_m0 = 1e-7; // Mie scattering beta
	const float depth_scale = 50.; // how many world meters in 0.1 game node
	const float depth_offset = 0.; // how far does the fog start
	const float max_depth = 1e5;  // in world meters

	// These factors are calculated based on expected value of scattering factor of 1e-5
	// for Nitrogen at 532nm (green), 2e25 molecules/m3 in atmosphere
	const vec3 beta_r0_l = vec3(3.3362176e-01, 8.75378289198826e-01, 1.95342379700656) * beta_r0; // wavelength-dependent scattering

	vec3 f_ex = exp(-(beta_r0_l + beta_m0) * clamp(depth_scale * (depth - depth_offset), 0., max_depth));

	const float atmosphere_height = 20000.; // height of the atmosphere
	const float g = 0.; // Henyey-Greenstein anisotropy factor 0..1

	// sun/moon light at the ground level, after going through the atmosphere
	vec3 light_ground = (sunBrightness * dayLight + moonBrightness * moonLight) * exp(-beta_r0_l * atmosphere_height / (1e-20 - dot(v_LightDirection, vec3(0., 1., 0.))));

	float cos_theta = dot(sourcePosition, lookDirection);
	// float cos_omega = pow(clamp(dot(sourcePosition, vec3(0., 0., 1.)), 0.0, 0.7), 2.5);
	float cos_omega = clamp(dot(sourcePosition, vec3(0., 0., 1.)), 0., 1.);

	float phase_r = 3. / 16. / 3.14 * (1. + cos_theta * cos_theta);
	float phase_m = 1. / 4. / 3.14 * pow(1. - g, 2.) / pow(1. + g * (g - 2. * cos_theta), 1.5);

	vec3 beta_r =  beta_r0_l * phase_r;
	float beta_m = beta_m0 * phase_m;
	float ray_sample = sampleVolumetricLight(uv, sourcePosition, rawDepth);

	vec3 l_in = light_ground * (1. - f_ex);
	l_in *= (beta_r + beta_m) / (beta_r0_l + beta_m0);

	return l_in / bloomIntensity * (0.2 * (1. - cos_omega) + cos_omega * sampleVolumetricLight(uv, sourcePosition, rawDepth));
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

	color += getInscatteredLight(uv, depth, rawDepth);

	// if (sunPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - sunPositionScreen.xy / sunPositionScreen.z) * 1000., 0., 1.);
	// if (moonPositionScreen.z < 0.)
	// 	color.rg += 1. - clamp(abs((2. * uv.xy - 1.) - moonPositionScreen.xy / moonPositionScreen.z) * 1000., 0., 1.);

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
