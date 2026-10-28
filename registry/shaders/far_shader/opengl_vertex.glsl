// The far view (src/far/view.cpp): the world beyond the loaded blocks, as
// blocks. Vertex colours are each face's colour and shade, as the near
// mesh's are in full daylight; they're lit here as the near shader lights
// them.
//
// The world is a planet: each vertex is placed through the sphere lens
// (client/shaders/sphere/lens.glsl), the near world's, so the two are one
// surface.
uniform mat4 mWorld;
uniform vec3 dayLight;
uniform highp float farSea;   // the scene y of the sea

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varLayer; // vertex alpha: 1 solid, 0 a liquid, ½ under a liquid
VARYING_ highp vec2 flatPos; // scene xz, before the curve
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude; // m above the sea
VARYING_ lowp vec3 varDay;        // the daylight, for the air
// (sphere/lens.glsl) behind the planet; on another face than the eye's; how far off it's seen
VARYING_ highp vec3 planetVec;
VARYING_ highp vec2 planetEye;
VARYING_ lowp float varOff;
VARYING_ highp float varSeen;

const float BS = 10.0; // scene units per block

void main(void)
{
	vec3 world = (mWorld * inVertexPosition).xyz;
	float height = (world.y - farSea) / BS;
	vec3 curved = sphereLens(world);
	vec4 pos = vec4(curved - mWorld[3].xyz, 1.0);

	flatPos = world.xz;
	varAltitude = height;
	varDay = dayLight;
	gl_Position = mWorldViewProj * pos;
	planetVec = (curved - sphereCamera) / BS;
	planetEye = sphereEye.zw;
	varOff = sphereOffFace(world) ? 1.0 : 0.0;
	varSeen = length(curved - sphereCamera);
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
