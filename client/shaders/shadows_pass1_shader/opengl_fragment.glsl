uniform sampler2D ColorMapSampler;
varying vec4 tPos;
#ifdef GL_ES
varying mediump vec4 varTexCoord;
#else
centroid varying vec4 varTexCoord;
#endif

void main()
{
	vec4 col = texture2D(ColorMapSampler, varTexCoord.st);

	if (col.a < 0.70)
		discard;

	float depth = 0.5 + tPos.z * 0.5;
	gl_FragColor = vec4(depth, 0.0, 0.0, 1.0);
}
