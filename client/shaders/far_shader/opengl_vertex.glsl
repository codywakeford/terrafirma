// The far view (src/far/view.cpp): the world beyond the loaded blocks, as
// blocks. Vertex colours are each face's colour and shade, as the near
// mesh's are in full daylight; they're lit here as the near shader lights
// them. The planet's curve is drawn: everything drops by d²/2R with its
// distance d from the eye, counted from where the near world ends, so the
// two meet without a step.
uniform mat4 mWorld;
uniform vec3 dayLight;
uniform highp vec3 farEye;   // scene units
uniform float farRadius;     // scene units
uniform float farCut;        // where the near world ends, scene units

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varLayer; // vertex alpha: 1 solid, 0 a liquid, ½ under a liquid
VARYING_ highp vec2 flatPos; // scene xz, before the curve
VARYING_ highp vec3 eyeVec;

void main(void)
{
	vec4 pos = inVertexPosition;
	vec3 world = (mWorld * pos).xyz;
	vec2 d = world.xz - farEye.xz;
	float past = max(length(d) - farCut, 0.0);
	pos.y -= past * (past + 2.0 * farCut) / (2.0 * farRadius);

	flatPos = world.xz;
	gl_Position = mWorldViewProj * pos;
	eyeVec = -(mWorldView * pos).xyz;

	// as nodes_shader lights a block in full sun
	vec4 color = inVertexColor;
	varLayer = color.a;
	color.a = 1.0;
	color.rgb *= dayLight;
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) + 0.07 * brightness);
	varColor = clamp(color, 0.0, 1.0);
}
