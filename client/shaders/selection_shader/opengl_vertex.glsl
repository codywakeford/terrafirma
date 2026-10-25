uniform mat4 mWorld;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	varTexCoord = inTexCoord0.st;
	gl_Position = sphereClip(inVertexPosition, mWorld); // on a planet, where the world is drawn

	varColor = inVertexColor;
}
