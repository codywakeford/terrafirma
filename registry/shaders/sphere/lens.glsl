// The planet as blocks: the map is the six faces of a cube-sphere laid out
// as a net (src/planet/planet.h), every column pointing at the centre.
// Next to the eye a block is where the engine has it; further off the land
// curves away as on the planet, and over an edge it carries on onto the
// next face, wherever that face is in the net.
//
// On the eye's face it's terragen::Sphere::Lens's sums. Off it, a point is placed on the planet
// from its own face and seen from the eye: a difference of two directions,
// good to a millimetre or so on a 13 km planet. It's planet::Lens line for
// line (src/planet/planet.h), which the unit tests hold to the planet.
// planetAxes (each face's axes) is written into every shader ahead of this
// (planet::Planet::glslAxes), so the axes can't drift apart.
//
// Every vertex shader has this; one that places geometry calls sphereClip
// in place of mWorldViewProj. Off (sphereEye.w = 0) on a flat world.
uniform highp vec4 sphereEye;     // radians east and north on the eye's face, m above the sea, the radius (m)
uniform highp float sphereStep;   // face-coordinate units per block
uniform highp float sphereMapping; // terragen::Sphere::Mapping
uniform highp vec3 sphereCamera;  // the eye, scene units
uniform highp vec3 sphereEast;    // the grid at the eye, inverted: rows, in the eye face's axes
uniform highp vec3 sphereUp;
uniform highp vec3 sphereNorth;
uniform highp vec3 spherePhysicalEast;
uniform highp vec3 spherePhysicalUp;
uniform highp vec3 spherePhysicalNorth;
uniform highp mat4 sphereViewProj;
uniform highp vec4 planetNet;     // the net: its first node, face to face (nodes), a face's half width, the eye's face
uniform highp vec3 planetOffset;  // the scene's origin, map nodes

const highp float SPHERE_BS = 10.0; // scene units per block
const highp float PLANET_GAP = 16.0; // planet::Planet::GAP

highp float sphereMapped(highp float a, highp float b)
{
	if (sphereMapping < 1.5)
		return tan(a);
	highp float a2 = a * a, b2 = b * b;
	highp float correction = -0.0941 * a2 + 0.0276 * b2 - 0.0623 * a2 * a2 +
		0.0409 * a2 * b2 + 0.0342 * b2 * b2;
	return 0.7240 * a + 0.2760 * a * a2 + (1.0 - a2) * a * correction;
}

// The face nearest a map point (planet::Planet::nearest): the gaps split down their middles.
highp float planetFace(highp vec2 q)
{
	highp vec2 i = floor((q - planetNet.x + 0.5 + 0.5 * PLANET_GAP) / planetNet.y);
	return clamp(i.y, 0.0, 1.0) * 3.0 + clamp(i.x, 0.0, 2.0);
}

// A map point as blocks from its face's centre (planet::Planet::place).
highp vec2 planetLocal(highp vec2 q, highp float face)
{
	highp float row = floor(face / 3.0);
	highp vec2 square = planetNet.x + vec2(face - 3.0 * row, row) * planetNet.y;
	return q - (square + planetNet.z - 0.5);
}

// A point on the eye's own face: in sums that keep their precision.
highp vec3 sphereNear(highp vec3 b)
{
	highp float height = sphereEye.z + b.y;
	highp float da = b.x * sphereStep, db = b.z * sphereStep;
	highp float a = sphereEye.x + da, c = sphereEye.y + db;
	highp float ea = sphereMapped(sphereEye.x, sphereEye.y);
	highp float eb = sphereMapped(sphereEye.y, sphereEye.x);
	highp float ta = sphereMapped(a, c), tb = sphereMapped(c, a);
	highp float ga = ta - ea, gb = tb - eb;
	if (sphereMapping < 1.5) {
		ga = sin(da) / (cos(a) * cos(sphereEye.x));
		gb = sin(db) / (cos(c) * cos(sphereEye.y));
	}
	highp float n = sqrt(ta * ta + 1.0 + tb * tb), en = sqrt(ea * ea + 1.0 + eb * eb);
	highp float grow = -(ga * (ea + ta) + gb * (eb + tb)) / (n * en * (n + en));
	highp float r = sphereEye.w + height, rise = height - sphereEye.z;
	return vec3(r * (ga / n + ea * grow) + rise * ea / en, r * grow + rise / en,
		r * (gb / n + eb * grow) + rise * eb / en);
}

// A point on another face: placed on the planet, then seen from the eye.
highp vec3 sphereFar(highp vec2 q, highp float face, highp float height)
{
	highp vec3 o, e, n, eo, ee, en;
	planetAxes(face, o, e, n);
	planetAxes(planetNet.w, eo, ee, en);
	highp vec2 a = planetLocal(q, face) * sphereStep;
	highp vec3 d = normalize(o + sphereMapped(a.x, a.y) * e + sphereMapped(a.y, a.x) * n);
	highp vec3 de = normalize(eo + sphereMapped(sphereEye.x, sphereEye.y) * ee +
		sphereMapped(sphereEye.y, sphereEye.x) * en);
	highp vec3 p = (sphereEye.w + height) * (d - de) + (height - sphereEye.z) * de;
	return vec3(dot(p, ee), dot(p, eo), dot(p, en)); // in the eye face's axes, as sphereNear's
}

highp vec3 sphereLens(highp vec3 scene)
{
	highp vec3 b = (scene - sphereCamera) / SPHERE_BS; // blocks from the eye
	highp vec2 q = planetOffset.xz + scene.xz / SPHERE_BS;
	highp float face = planetFace(q);
	highp vec3 p = face == planetNet.w ? sphereNear(b) : sphereFar(q, face, sphereEye.z + b.y);
	highp vec3 grid = vec3(dot(sphereEast, p), dot(sphereUp, p), dot(sphereNorth, p));
	highp vec3 physical = vec3(dot(spherePhysicalEast, p), dot(spherePhysicalUp, p), dot(spherePhysicalNorth, p));
	highp float t = clamp((length(physical) - 16.0) / 112.0, 0.0, 1.0);
	t = t * t * (3.0 - 2.0 * t);
	return sphereCamera + SPHERE_BS * mix(grid, physical, t);
}

highp vec4 sphereClip(highp vec4 model, highp mat4 world)
{
	if (sphereEye.w == 0.0)
		return mWorldViewProj * model;
	return sphereViewProj * vec4(sphereLens((world * model).xyz), 1.0);
}

// From a vertex to the eye, for the fog: on a planet, from where it's drawn.
// (Only its length is used, so world space serves for view space.)
highp vec3 sphereEyeVec(highp vec4 model, highp mat4 world, highp vec3 plain)
{
	if (sphereEye.w == 0.0)
		return plain;
	return sphereCamera - sphereLens((world * model).xyz);
}

// Whether a scene point is behind the planet as the eye sees it: the straight
// line from the eye to it passes through the ground. (A sphere a little under
// the sea, so a shore's shallows aren't hidden behind its own water.) What
// the near world doesn't reach has nothing else to hide it: the far view,
// clouds.
const highp float SPHERE_UNDER = 100.0; // m below the sea

bool sphereHidden(highp vec3 scene)
{
	if (sphereEye.w == 0.0)
		return false;
	highp vec3 p = (sphereLens(scene) - sphereCamera) / SPHERE_BS; // m, the eye's frame: up is straight up
	highp float far = length(p);
	if (far < 1.0)
		return false;
	highp float d = sphereEye.w + sphereEye.z; // the eye to the centre, straight down
	highp float t = -d * p.y / far; // along the line, where it's nearest the centre
	highp float r = sphereEye.w - SPHERE_UNDER;
	return t > 0.0 && t < far && d * d - t * t < r * r;
}

// Whether a scene point is on a face other than the eye's (in the net, far off).
bool sphereOffFace(highp vec3 scene)
{
	return sphereEye.w != 0.0 && planetFace(planetOffset.xz + scene.xz / SPHERE_BS) != planetNet.w;
}
