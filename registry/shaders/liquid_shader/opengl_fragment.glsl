// The liquid layer's fine water: its colour, fogged as the near world is.
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	vec4 col = varColor;
	float clarity = clamp(fogShadingParameter - fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
	col.rgb = mix(fogColor.rgb, col.rgb, clarity);
	gl_FragColor = col;
}
