// the mask, per map-block column: r, the near world draws all of its
// surface; g, it draws the top of it (its own liquid, over our floor)
uniform sampler2D texture0;
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform float farCut;
uniform float farHaze;       // the distance past the cut the haze builds over, near the ground
uniform highp vec4 farMask;  // its corner (scene x, z), its size, 1 if it applies
uniform float farShare;      // 0 in the air's haze .. 1 above the air, seeing the planet
uniform highp vec4 farPlace; // the eye: map x, z (blocks), altitude (m), unused
uniform float farAir;        // m: the air's scale height

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varLayer;
VARYING_ highp float varSeen;  // how far off it's seen, scene units
VARYING_ highp vec2 flatPos;
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude;
VARYING_ lowp vec3 varDay;

const float BS = 10.0;              // scene units per block
const float CLEAR = 3.0 / 60000.0;  // per m: the air's haze through clear air, at the sea
const vec3 AIR = vec3(0.55, 0.68, 0.85); // the colour of lit air seen from above

// How much air a straight line from the eye's height to `altitude` passes
// through, per metre of it, against air at the sea.
float airmass(float eye, float altitude)
{
	float rise = altitude - eye;
	float low = exp(-min(eye, altitude) / farAir);
	if (abs(rise) < 1.0)
		return low;
	return low * (1.0 - exp(-abs(rise) / farAir)) * farAir / abs(rise);
}

void main(void)
{
	// where the near world draws, it's the near world (the mask, laid out
	// round the eye in its own grid: the scene's, where we're drawn too)
	vec2 uv = (flatPos - farMask.xy) / farMask.z;
	if (farMask.w > 0.5 && uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0) {
		// The near world's faces replace ours where it draws them. What lies
		// under a liquid never gives way: where the near world's own floor is
		// there it draws over ours (the same blocks); where it isn't yet, its
		// liquid would otherwise lie over the sky.
		vec4 near = texture2D(texture0, uv);
		bool liquid = varLayer < 0.25, under = varLayer >= 0.25 && varLayer < 0.75;
		if (!under && (near.r > 0.5 || (near.g > 0.5 && liquid)))
			discard;
	}

	vec4 col = vec4(varColor.rgb, 1.0);
	float dist = length(eyeVec);

	// the near world's fog, so the two match at the seam (high up there's
	// no seam, and the near world's fog would black out the planet) ...
	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * dist / fogDistance, 0.0, 1.0);
	clarity = mix(clarity, 1.0, farShare);
	// ... and the air beyond it: haze through the air the sight line
	// crosses, so kilometres read as kilometres and the air thins as you
	// climb. Near the ground it builds over the far view's reach, as it
	// always has; up the air, as through real air.
	// how far off it's seen
	float past = max(varSeen - farCut, 0.0) / BS;
	float through = mix(past, dist / BS, farShare);
	float thick = mix(2.2 * BS / farHaze, CLEAR, farShare);
	float haze = exp(-thick * through * airmass(farPlace.z, max(varAltitude, 0.0)));

	// near the ground the haze is the sky's fog; from above, lit air
	float fogColorMax = max(max(fogColor.r, fogColor.g), fogColor.b);
	if (fogColorMax < 0.0000001) fogColorMax = 1.0;
	vec4 fog = fogColor * pow(fogColor / fogColorMax, vec4(2.0 * clarity * haze));
	vec3 air = mix(fog.rgb, AIR * varDay, farShare);
	col.rgb = mix(fog.rgb, col.rgb, clarity);
	col.rgb = mix(air, col.rgb, haze);
	gl_FragColor = vec4(col.rgb, 1.0);
}
