#define rendered texture0
#define bloom texture1
#define depthmap texture3
#define normalmap texture4
#define water_mask texture5

struct ExposureParams {
	float compensationFactor;
};

uniform sampler2D rendered;
uniform sampler2D bloom;
uniform sampler2D depthmap;
uniform sampler2D normalmap;
uniform sampler2D water_mask;

uniform vec2 texelSize0;

uniform ExposureParams exposureParams;
uniform lowp float bloomIntensity;
uniform lowp float saturation;

uniform vec3 cameraOffset;
varying vec3 eyeVec;
varying mat4 viewMatrix;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

#ifdef ENABLE_AUTO_EXPOSURE
varying float exposure; // linear exposure factor, see vertex shader
#endif

#ifdef ENABLE_BLOOM

vec4 applyBloom(vec4 color, vec2 uv)
{
	vec3 light = texture2D(bloom, uv).rgb;
#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 && uv.y < 0.5)
		return vec4(light, color.a);
	if (uv.x < 0.5)
		return color;
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

vec3 applySaturation(vec3 color, float factor)
{
	// Calculate the perceived luminosity from the RGB color.
	// See also: https://www.w3.org/WAI/GL/wiki/Relative_luminance
	float brightness = dot(color, vec3(0.2125, 0.7154, 0.0721));
	return max(vec3(0.), mix(vec3(brightness), color, factor));
}
#endif

float det(mat2 matrix) {
    return matrix[0].x * matrix[1].y - matrix[0].y * matrix[1].x;
}

mat3 inverse(mat3 matrix) {
    vec3 row0 = matrix[0];
    vec3 row1 = matrix[1];
    vec3 row2 = matrix[2];

    vec3 minors0 = vec3(
        det(mat2(row1.y, row1.z, row2.y, row2.z)),
        det(mat2(row1.z, row1.x, row2.z, row2.x)),
        det(mat2(row1.x, row1.y, row2.x, row2.y))
    );
    vec3 minors1 = vec3(
        det(mat2(row2.y, row2.z, row0.y, row0.z)),
        det(mat2(row2.z, row2.x, row0.z, row0.x)),
        det(mat2(row2.x, row2.y, row0.x, row0.y))
    );
    vec3 minors2 = vec3(
        det(mat2(row0.y, row0.z, row1.y, row1.z)),
        det(mat2(row0.z, row0.x, row1.z, row1.x)),
        det(mat2(row0.x, row0.y, row1.x, row1.y))
    );

    mat3 adj = transpose(mat3(minors0, minors1, minors2));

    return (1.0 / dot(row0, minors0)) * adj;
}

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct Frame {
    vec3 x, y, z;
};

const float _MaxDistance = 15.0;
const float _Step = 0.05;
const float _Thickness = 0.0006;
const float _Near = 1.0;
const float _Far = 1000.0;

// mat3 lookMat = inverse(mat3(viewMatrix));
// Frame _Camera = Frame(-lookMat[0], -lookMat[1], lookMat[2]);
Frame _Camera = Frame(vec3(1, 0, 0), vec3(0, -1, 0), vec3(0, 0, 1));

float map(float value, float min1, float max1, float min2, float max2) {
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

vec2 projectOnScreen(vec3 eye, vec3 point) {
    vec3 toPoint = (point - eye);
    point = (point - toPoint * (1.0 - _Near / dot(toPoint, _Camera.z)));
    point -= eye + _Near * _Camera.z;
    return point.xy;
}

void main(void)
{
	vec2 uv = varTexCoord.st;
	vec4 mask = texture2D(water_mask, uv).rgba;
    // gl_FragColor = mask;
    // gl_FragColor = vec4((texture2D(normalmap, uv).rgb + 1.0) / 2.0, 1.0);
    // return;

    // if (mask == vec4(1.0)) { // This somehow catches the sun color ........... somehow
    if (mask == vec4(1.0, 0.0, 1.0, 1.0)) {
        float aspect = texelSize0.y / texelSize0.x;
        vec3 lookDir = normalize(inverse(mat3(viewMatrix)) * vec3(0, 0, 1));

        // View ray
        vec2 r_uv = 2.0 * gl_FragCoord.xy / (1 / texelSize0.y) - vec2(aspect, 1.0);
        vec3 r_dir = vec3(r_uv.x * _Camera.x + r_uv.y * _Camera.y + _Near * _Camera.z);
        Ray ray = Ray(cameraOffset, normalize(r_dir));

        float depth = texture2D(depthmap, uv).x;
        vec3 normal = texture2D(normalmap, uv).xyz;

        vec3 view = ray.dir * length(r_dir) * depth * _Far / _Near;
        vec3 position = ray.origin + view;
        vec3 reflected = reflect(normalize(view), normal);

        vec2 reflectionUV = uv;
        float atten = 0.0;

        vec3 marchReflection;
        float currentDepth = depth;
        for (float i = _Step; i < _MaxDistance; i += _Step) {
            marchReflection = i * reflected;
            
            float targetDepth = dot(view + marchReflection, _Camera.z) / _Far;
            vec2 target = projectOnScreen(cameraOffset, position + marchReflection);

            target.x = map(target.x, -aspect, aspect, 0.0, 1.0);
            target.y = map(target.y, -1.0, 1.0, 0.0, 1.0);

            float sampledDepth = texture2D(depthmap, target).x;
            float depthDiff = sampledDepth - currentDepth;

            if (depthDiff > 0.0 && depthDiff < targetDepth - currentDepth + _Thickness) {
                reflectionUV = target;
                atten = 1.0 - i / _MaxDistance;
                break;
            }

            currentDepth = targetDepth;
            if (currentDepth > 1.0) {
                atten = 1.0;
                break;
            }
        }

        gl_FragColor = vec4(texture2D(rendered, reflectionUV).rgb * atten, 1.0);

        return;
    }
    
#ifdef ENABLE_SSAA
	vec4 color = vec4(0.);
	for (float dx = 1.; dx < SSAA_SCALE; dx += 2.)
	for (float dy = 1.; dy < SSAA_SCALE; dy += 2.)
		color += texture2D(rendered, uv + texelSize0 * vec2(dx, dy)).rgba;
	color /= SSAA_SCALE * SSAA_SCALE / 4.;
#else
	vec4 color = texture2D(rendered, uv).rgba;
#endif

	// translate to linear colorspace (approximate)
	color.rgb = pow(color.rgb, vec3(2.2));

#if ENABLE_TONE_MAPPING
	color.rgb = applySaturation(color.rgb, 1.25);
#endif	

#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 || uv.y > 0.5)
#endif
	{
		color.rgb *= exposureParams.compensationFactor;
#ifdef ENABLE_AUTO_EXPOSURE
		color.rgb *= exposure;
#endif
	}


#ifdef ENABLE_BLOOM
	color = applyBloom(color, uv);
#endif

#ifdef ENABLE_BLOOM_DEBUG
	if (uv.x > 0.5 || uv.y > 0.5)
#endif
	{
#if ENABLE_TONE_MAPPING
		color = applyToneMapping(color);
		color.rgb = applySaturation(color.rgb, saturation);
#endif
	}

	color.rgb = clamp(color.rgb, vec3(0.), vec3(1.));

	// return to sRGB colorspace (approximate)
	color.rgb = pow(color.rgb, vec3(1.0 / 2.2));

	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
