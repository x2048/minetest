uniform mat4 mWorld;
// Color of the light emitted by the sun.
uniform vec3 dayLight;
uniform vec3 eyePosition;

// The cameraOffset is the current center of the visible world.
uniform vec3 cameraOffset;
uniform float animationTimer;

varying vec3 vNormal;
varying vec3 vPosition;
// World position in the visible world (i.e. relative to the cameraOffset.)
// This can be used for many shader effects without loss of precision.
// If the absolute position is required it can be calculated with
// cameraOffset + worldPosition (for large coordinates the limits of float
// precision must be considered).
varying vec3 worldPosition;
varying lowp vec3 artificialColor;
varying lowp vec3 naturalColor;

// The centroid keyword ensures that after interpolation the texture coordinates
// lie within the same bounds when MSAA is en- and disabled.
// This fixes the stripes problem with nearest-neighbor textures and MSAA.
#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif
#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
	uniform float f_shadow_strength;
	uniform float f_timeofday;
	uniform vec4 CameraPos;

	varying float cosLight;
	varying float normalOffsetScale;
	varying float directLightIntensity;
	varying float f_normal_length;
	varying vec3 shadow_position;
	varying float perspective_factor;
	varying lowp vec3 directNaturalLight;
#endif

varying float area_enable_parallax;

varying vec3 eyeVec;
varying float nightRatio;
// Color of the light emitted by the light sources.
const vec3 artificialLight = vec3(1.04, 1.04, 1.04);
const float e = 2.718281828459;
const float BS = 10.0;
uniform float xyPerspectiveBias0;
uniform float xyPerspectiveBias1;
uniform float zPerspectiveBias;

// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_OPAQUE || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_BASIC) && ENABLE_WAVING_WATER

//
// Simple, fast noise function.
// See: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
//
vec4 perm(vec4 x)
{
	return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float snoise(vec3 p)
{
	vec3 a = floor(p);
	vec3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 k1 = perm(b.xyxy);
	vec4 k2 = perm(k1.xyxy + b.zzww);

	vec4 c = k2 + a.zzzz;
	vec4 k3 = perm(c);
	vec4 k4 = perm(c + 1.0);

	vec4 o1 = fract(k3 * (1.0 / 41.0));
	vec4 o2 = fract(k4 * (1.0 / 41.0));

	vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	return o4.y * d.y + o4.x * (1.0 - d.y);
}

#endif

void main(void)
{
	varTexCoord = inTexCoord0.st;

	vec4 pos = inVertexPosition;
// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_OPAQUE || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_BASIC) && ENABLE_WAVING_WATER
	// Generate waves with Perlin-type noise.
	// The constants are calibrated such that they roughly
	// correspond to the old sine waves.
	vec3 wavePos = (mWorld * pos).xyz + cameraOffset;
	// The waves are slightly compressed along the z-axis to get
	// wave-fronts along the x-axis.
	wavePos.x /= WATER_WAVE_LENGTH * 3.0;
	wavePos.z /= WATER_WAVE_LENGTH * 2.0;
	wavePos.z += animationTimer * WATER_WAVE_SPEED * 10.0;
	pos.y += (snoise(wavePos) - 1.0) * WATER_WAVE_HEIGHT * 5.0;
#endif
	worldPosition = (mWorld * pos).xyz;
	gl_Position = mWorldViewProj * pos;

	vPosition = gl_Position.xyz;
	eyeVec = -(mWorldView * pos).xyz;
}
