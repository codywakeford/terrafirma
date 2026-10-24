// The planet as blocks: the map is the six faces of a cube-sphere laid out
// as a net (src/planet/planet.h), every column pointing at the centre.
// Next to the eye a block is where the engine has it; further off the land
// curves away as on the planet, and over an edge it carries on onto the
// next face, wherever that face is in the net.
//
// On the eye's face it's terragen::Sphere::Lens, the same sums, held to the
// same answers by the unit tests. Off it, a point is placed on the planet
// from its own face and seen from the eye: a difference of two directions,
// good to a millimetre or so on a 13 km planet. planetAxes (each face's
// axes) is written into every shader from terragen::Sphere
// (src/client/sphere_lens.cpp), so the two can't drift apart.
//
// Every vertex shader has this; one that places geometry calls sphereClip
// in place of mWorldViewProj. Off (sphereEye.w = 0) on a flat world.
uniform highp vec4 sphereEye;     // radians east and north on the eye's face, m above the sea, the radius (m)
uniform highp float sphereStep;   // radians per block
uniform highp vec3 sphereCamera;  // the eye, scene units
uniform highp vec3 sphereEast;    // the grid at the eye, inverted: rows, in the eye face's axes
uniform highp vec3 sphereUp;
uniform highp vec3 sphereNorth;
uniform highp mat4 sphereViewProj;
uniform highp vec4 planetNet;     // the net: its first node, face to face (nodes), a face's half width, the eye's face
uniform highp vec3 planetOffset;  // the scene's origin, map nodes

const highp float SPHERE_BS = 10.0; // scene units per block
const highp float PLANET_GAP = 16.0; // planet::Planet::GAP

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
	highp float ea = tan(sphereEye.x), eb = tan(sphereEye.y), ta = tan(a), tb = tan(c);
	highp float ga = sin(da) / (cos(a) * cos(sphereEye.x)), gb = sin(db) / (cos(c) * cos(sphereEye.y));
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
	highp vec3 d = normalize(o + tan(a.x) * e + tan(a.y) * n);
	highp vec3 de = normalize(eo + tan(sphereEye.x) * ee + tan(sphereEye.y) * en);
	highp vec3 p = (sphereEye.w + height) * (d - de) + (height - sphereEye.z) * de;
	return vec3(dot(p, ee), dot(p, eo), dot(p, en)); // in the eye face's axes, as sphereNear's
}

highp vec3 sphereLens(highp vec3 scene)
{
	highp vec3 b = (scene - sphereCamera) / SPHERE_BS; // blocks from the eye
	highp vec2 q = planetOffset.xz + scene.xz / SPHERE_BS;
	highp float face = planetFace(q);
	highp vec3 p = face == planetNet.w ? sphereNear(b) : sphereFar(q, face, sphereEye.z + b.y);
	return sphereCamera + SPHERE_BS * vec3(dot(sphereEast, p), dot(sphereUp, p), dot(sphereNorth, p));
}

highp vec4 sphereClip(highp vec4 model, highp mat4 world)
{
	if (sphereEye.w == 0.0)
		return mWorldViewProj * model;
	return sphereViewProj * vec4(sphereLens((world * model).xyz), 1.0);
}
