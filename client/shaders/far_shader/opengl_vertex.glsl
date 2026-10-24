// The far view (src/far/view.cpp): the world beyond the loaded blocks, as
// blocks. Vertex colours are each face's colour and shade, as the near
// mesh's are in full daylight; they're lit here as the near shader lights
// them.
//
// The map is laid on a sphere around the eye (terragen::Map): a block spans
// farAcross metres of it, east and north, and it curves with farRadius.
// Near the ground that's a metre a block on a gently curved world; up the
// air both grow with height, until it's the planet at true size. A point is
// placed as on a globe whose latitudes and longitudes run from the eye's:
// at true size, exactly where it is on the planet.
uniform mat4 mWorld;
uniform vec3 dayLight;
uniform highp vec3 farEye;    // scene units
uniform highp float farRadius; // scene units: the sphere the land lies on
uniform float farCut;         // where the near world ends, scene units
uniform highp float farSea;   // the scene y of the sea
uniform highp vec4 farPlace;  // the eye: map x, z (blocks), altitude (m); the map's size (blocks)
uniform highp vec2 farAcross; // m of the sphere per block, east and north, where the eye is

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varLayer; // vertex alpha: 1 solid, 0 a liquid, ½ under a liquid
VARYING_ highp vec2 flatPos; // scene xz, before the curve
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude; // m above the sea
VARYING_ lowp vec3 varDay;        // the daylight, for the air

const float BS = 10.0; // scene units per block
const float PI = 3.14159265;

// Where a point of the map, d blocks east and north of the eye and `height`
// m above the sea, is from the eye: east, up and north of it (the eye's own
// frame, the scene's axes), metres. In forms that keep their precision a
// whole planet away.
vec3 globe(vec2 d, float height)
{
	float R = farRadius / BS;
	float eyeLat = PI * farPlace.y / farPlace.w; // the eye's true latitude: where the map's rows shrink
	float lat = clamp(eyeLat + d.y * farAcross.y / R, -0.5 * PI, 0.5 * PI); // past the poles: at them
	float dLat = lat - eyeLat, dLon = d.x * farAcross.x / (R * max(cos(eyeLat), 0.01));
	float r = R + height, eyeR = R + farPlace.z;
	float halfLat = sin(0.5 * dLat), halfLon = sin(0.5 * dLon);
	float around = 2.0 * cos(lat) * halfLon * halfLon; // 1 − cos Δλ, scaled by cos φ
	float drop = 2.0 * halfLat * halfLat + cos(eyeLat) * around; // 1 − cos of the angle between them
	return vec3(r * cos(lat) * sin(dLon), (r - eyeR) - r * drop, r * (sin(dLat) + sin(eyeLat) * around));
}

void main(void)
{
	vec3 world = (mWorld * inVertexPosition).xyz;
	vec2 d = (world.xz - farEye.xz) / BS;
	float height = (world.y - farSea) / BS;
	// where the near world ends the two meet: no drop before it
	float cut = farCut / BS;
	vec3 at = globe(d, height);
	at.y += cut * cut / (2.0 * farRadius / BS);
	vec4 pos = vec4(farEye + at * BS - mWorld[3].xyz, 1.0);

	flatPos = world.xz;
	varAltitude = height;
	varDay = dayLight;
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
