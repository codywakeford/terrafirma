uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
VARYING_ highp vec3 eyeVec;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec2 planetEye; // eye altitude and planet radius

void main(void)
{
	if (planetEye.y > 0.0) {
		// The lens runs in the vertex shader. Its eye vector interpolates to
		// this fragment's curved position, so test the sight line here rather
		// than interpolating a hidden flag across an entire cloud quad.
		const highp float blockScale = 10.0;
		highp vec3 p = -eyeVec / blockScale;
		highp float far = length(p);
		if (far >= 1.0) {
			highp float d = planetEye.y + planetEye.x;
			highp float t = -d * p.y / far;
			// Clouds need the sea surface as their occluder; the far
			// terrain's depth is cleared before the cloud pass.
			highp float r = planetEye.y;
			if (t > 0.0 && t < far && d * d - t * t < r * r)
				discard;
		}
	}
	vec4 col = varColor;

	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
	col.rgb = mix(fogColor.rgb, col.rgb, clarity);

	gl_FragColor = col;
}
