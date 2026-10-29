// The far view on an atlas planet (src/far/layout.cpp, GlobeLayout): its
// tiles are built on the planet, round each tile's middle, and drawn as the
// near world's blocks are (AtlasView: the eye's one map), so no lens here.
// Vertex colours are each face's colour and shade, as the near mesh's are
// in full daylight; they're lit here as the near shader lights them. The
// first texture coordinate is the vertex's height over the sea, in blocks.
uniform mat4 mWorld;
uniform vec3 dayLight;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude; // m above the sea
VARYING_ lowp vec3 varDay;        // the daylight, for the air
VARYING_ highp float varSeen;     // how far off it's seen, scene units

void main(void)
{
	vec3 world = (mWorld * inVertexPosition).xyz;
	varAltitude = inTexCoord0.x;
	varDay = dayLight;
	gl_Position = mWorldViewProj * inVertexPosition;
	eyeVec = -(mWorldView * inVertexPosition).xyz;
	varSeen = length(eyeVec);

	// as nodes_shader lights a block in full sun
	vec4 color = inVertexColor;
	color.a = 1.0;
	color.rgb *= dayLight;
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) + 0.07 * brightness);
	varColor = clamp(color, 0.0, 1.0);
}
