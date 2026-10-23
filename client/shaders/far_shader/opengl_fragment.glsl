// the mask, per map-block column: r, the near world draws all of its
// surface; g, it draws the top of it (its own liquid, over our floor)
uniform sampler2D texture0;
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform highp vec3 farEye;
uniform float farCut;
uniform float farHaze;       // the distance past the cut the haze builds over
uniform highp vec4 farMask;  // its corner (scene x, z), its size

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varLayer;
VARYING_ highp vec2 flatPos;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	// where the near world draws, it's the near world
	vec2 uv = (flatPos - farMask.xy) / farMask.z;
	if (uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0) {
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

	// the near world's fog, so the two match at the seam ...
	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * dist / fogDistance, 0.0, 1.0);
	// ... and the air beyond it: haze that thickens with distance past the
	// near world, so kilometres read as kilometres
	float past = max(length(flatPos - farEye.xz) - farCut, 0.0);
	clarity *= exp(-2.2 * past / farHaze);

	float fogColorMax = max(max(fogColor.r, fogColor.g), fogColor.b);
	if (fogColorMax < 0.0000001) fogColorMax = 1.0;
	col = mix(fogColor * pow(fogColor / fogColorMax, vec4(2.0 * clarity)), col, clarity);
	gl_FragColor = vec4(col.rgb, 1.0);
}
