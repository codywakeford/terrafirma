uniform sampler2D texture0;  // the mask: map-block columns the near world draws
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform highp vec3 farEye;
uniform float farCut;
uniform float farHaze;       // the distance past the cut the haze builds over
uniform highp vec4 farMask;  // its corner (scene x, z), its size

VARYING_ lowp vec4 varColor;
VARYING_ highp vec2 flatPos;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	// where the near world draws, it's the near world
	vec2 uv = (flatPos - farMask.xy) / farMask.z;
	if (uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0 && texture2D(texture0, uv).r > 0.5)
		discard;

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
