uniform lowp vec4 materialColor;
uniform mat4 mWorld;

VARYING_ lowp vec4 varColor;

VARYING_ highp vec3 eyeVec;
VARYING_ highp vec2 planetEye; // eye altitude and planet radius, for per-fragment occlusion

void main(void)
{
	// on a planet the clouds are a shell round it (client/shaders/sphere/lens.glsl)
	gl_Position = sphereClip(inVertexPosition, mWorld);
	planetEye = sphereEye.zw;

	vec4 color = inVertexColor;

	color *= materialColor;
	varColor = color;

	eyeVec = sphereEyeVec(inVertexPosition, mWorld, -(mWorldView * inVertexPosition).xyz);
}
