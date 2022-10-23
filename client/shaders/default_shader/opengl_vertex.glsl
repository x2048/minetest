varying lowp vec4 varColor;

void main(void)
{
	gl_Position = mWorldViewProj * inVertexPosition;
#ifdef GL_ES
	varColor = inVertexColor.bgra;
#else
	varColor = inVertexColor;
#endif

	// translate to linear colorspace (approximate)
	varColor.rgb = pow(varColor.rgb, vec3(2.2));
}
