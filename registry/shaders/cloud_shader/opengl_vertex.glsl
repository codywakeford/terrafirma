uniform lowp vec4 materialColor;
uniform mat4 mWorld;

VARYING_ lowp vec4 varColor;

VARYING_ highp vec3 eyeVec;
VARYING_ lowp float varHidden; // on a planet: behind it

void main(void)
{
	// on a planet the clouds are a shell round it (client/shaders/sphere/lens.glsl)
	gl_Position = sphereClip(inVertexPosition, mWorld);
	varHidden = sphereHidden((mWorld * inVertexPosition).xyz) ? 1.0 : 0.0;

	vec4 color = inVertexColor;

	color *= materialColor;
	varColor = color;

	eyeVec = sphereEyeVec(inVertexPosition, mWorld, -(mWorldView * inVertexPosition).xyz);
}
