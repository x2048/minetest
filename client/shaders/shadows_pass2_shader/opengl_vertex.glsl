#ifdef GL_ES
varying mediump vec4 varTexCoord;
#else
centroid varying vec4 varTexCoord;
#endif

void main()
{
	vec4 pos = vec4(inVertexPosition.xyz, 1.0);
	varTexCoord = pos * 0.5 + 0.5;
	gl_Position = pos;
}
