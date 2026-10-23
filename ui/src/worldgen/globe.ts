// The globe as real geometry: a sphere mesh whose vertices are pushed out by
// the terrain under them, lit by its own (faceted) surface. WebGL2, no library.

const VERTEX = `#version 300 es
in vec2 a_uv;
uniform sampler2D u_height;
uniform mat3 u_rotation;
uniform float u_relief;
uniform float u_scale;
uniform float u_aspect; // height over width: the sphere stays round on any canvas
out vec2 v_uv;
out vec3 v_pos;
void main() {
    float lon = a_uv.x * 6.2831853;
    float lat = (a_uv.y - 0.5) * 3.1415927;
    vec3 n = vec3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));
    vec3 p = u_rotation * (n * (1.0 + texture(u_height, a_uv).r * u_relief));
    v_uv = a_uv;
    v_pos = p;
    // y down on screen matches the map's rows; +z faces the viewer
    gl_Position = vec4(p.x * u_scale * u_aspect, -p.y * u_scale, -p.z * 0.4, 1.0);
}`

const FRAGMENT = `#version 300 es
precision highp float;
in vec2 v_uv;
in vec3 v_pos;
uniform sampler2D u_color;
uniform vec3 u_light;     // towards the sun, in view space
uniform vec3 u_sun_color; // the star's light
out vec4 o_color;
void main() {
    vec3 n = normalize(cross(dFdx(v_pos), dFdy(v_pos)));
    if (n.z < 0.0) n = -n;
    // a little starlight keeps the night side readable
    float sun = max(dot(n, u_light), 0.0);
    vec3 light = vec3(0.07, 0.09, 0.14) + u_sun_color * sun * 1.1;
    o_color = vec4(texture(u_color, v_uv).rgb * light, 1.0);
}`

const LAT = 128
const LON = 256

function compile(gl: WebGL2RenderingContext, type: number, source: string) {
    const shader = gl.createShader(type)!
    gl.shaderSource(shader, source)
    gl.compileShader(shader)
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(`globe shader: ${gl.getShaderInfoLog(shader)}`)
    return shader
}

/** Owns one canvas's WebGL state; draw() whenever the spin, relief or terrain changes. */
export function GlobeRenderer(opts: { canvas: HTMLCanvasElement }) {
    const gl = opts.canvas.getContext("webgl2", { alpha: true, premultipliedAlpha: true, antialias: false })
    if (!gl) throw new Error("globe: WebGL2 is not available")

    const program = gl.createProgram()!
    gl.attachShader(program, compile(gl, gl.VERTEX_SHADER, VERTEX))
    gl.attachShader(program, compile(gl, gl.FRAGMENT_SHADER, FRAGMENT))
    gl.linkProgram(program)
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(`globe program: ${gl.getProgramInfoLog(program)}`)

    // a (LON+1)×(LAT+1) grid of uv points; the seam column repeats u=0 as u=1
    const uv = new Float32Array((LAT + 1) * (LON + 1) * 2)
    for (let y = 0; y <= LAT; y++)
        for (let x = 0; x <= LON; x++) {
            const i = (y * (LON + 1) + x) * 2
            uv[i] = x / LON
            uv[i + 1] = y / LAT
        }
    const indices = new Uint16Array(LAT * LON * 6)
    let k = 0
    for (let y = 0; y < LAT; y++)
        for (let x = 0; x < LON; x++) {
            const a = y * (LON + 1) + x, b = a + 1, c = a + LON + 1, d = c + 1
            indices.set([a, c, b, b, c, d], k)
            k += 6
        }

    const vao = gl.createVertexArray()
    gl.bindVertexArray(vao)
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer())
    gl.bufferData(gl.ARRAY_BUFFER, uv, gl.STATIC_DRAW)
    const loc = gl.getAttribLocation(program, "a_uv")
    gl.enableVertexAttribArray(loc)
    gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0)
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gl.createBuffer())
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW)

    function texture(unit: number) {
        const t = gl!.createTexture()
        gl!.activeTexture(gl!.TEXTURE0 + unit)
        gl!.bindTexture(gl!.TEXTURE_2D, t)
        gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_S, gl!.REPEAT)
        gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_T, gl!.CLAMP_TO_EDGE)
        return t
    }
    const colorTex = texture(0)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    const heightTex = texture(1)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR)
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR)

    const u = (name: string) => gl!.getUniformLocation(program, name)
    const uniforms = {
        rotation: u("u_rotation"), relief: u("u_relief"), scale: u("u_scale"), aspect: u("u_aspect"),
        color: u("u_color"), height: u("u_height"), light: u("u_light"), sunColor: u("u_sun_color"),
    }

    return {
        /** New terrain: colour image plus row-major height (-1..1, sea at 0) of the same size. */
        terrain(image: ImageData, elevation: Float32Array) {
            gl.activeTexture(gl.TEXTURE0)
            gl.bindTexture(gl.TEXTURE_2D, colorTex)
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image)
            // only land rises; the sea is a smooth surface
            const height = Uint8Array.from(elevation, (e) => Math.round(Math.max(0, Math.min(1, e)) * 255))
            gl.activeTexture(gl.TEXTURE1)
            gl.bindTexture(gl.TEXTURE_2D, heightTex)
            gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1)
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.R8, image.width, image.height, 0, gl.RED, gl.UNSIGNED_BYTE, height)
        },
        /**
         * spin: the view (lon spins the planet, tilt tips it toward you).
         * sun: declination (radians, the latitude under the noon sun), and colour.
         * The sun stays put in space as the planet spins, so day and night move across it.
         * size: the planet's radius as a share of half the canvas's height (room around it for an orbit).
         */
        draw(spin: { lon: number; tilt: number }, relief: number, sun: { declination: number; color: readonly [number, number, number] }, size = 0.92) {
            const c = opts.canvas
            const dpr = window.devicePixelRatio || 1
            const w = Math.round(c.clientWidth * dpr), h = Math.round(c.clientHeight * dpr)
            if (c.width !== w || c.height !== h) {
                c.width = w
                c.height = h
            }
            gl.viewport(0, 0, w, h)
            gl.clearColor(0, 0, 0, 0)
            gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT)
            gl.enable(gl.DEPTH_TEST)
            gl.useProgram(program)
            gl.bindVertexArray(vao)

            const cy = Math.cos(spin.lon), sy = Math.sin(spin.lon), cx = Math.cos(spin.tilt), sx = Math.sin(spin.tilt)
            // tilt (about x) after spin (about y); column-major
            const m = [cy, sx * sy, -cx * sy, 0, cx, sx, sy, -sx * cy, cx * cy] // column-major
            gl.uniformMatrix3fv(uniforms.rotation, false, m)
            // The sun in the planet's frame: overhead at its declination (north
            // is -y here), at a longitude that cancels the spin so it holds still
            // just left of the viewer.
            const lon = -spin.lon - 0.6
            const p = [Math.cos(sun.declination) * Math.sin(lon), -Math.sin(sun.declination), Math.cos(sun.declination) * Math.cos(lon)]
            gl.uniform3f(uniforms.light,
                m[0]! * p[0]! + m[3]! * p[1]! + m[6]! * p[2]!,
                m[1]! * p[0]! + m[4]! * p[1]! + m[7]! * p[2]!,
                m[2]! * p[0]! + m[5]! * p[1]! + m[8]! * p[2]!)
            gl.uniform3f(uniforms.sunColor, sun.color[0] / 255, sun.color[1] / 255, sun.color[2] / 255)
            gl.uniform1f(uniforms.relief, relief)
            gl.uniform1f(uniforms.scale, size / (1 + relief))
            gl.uniform1f(uniforms.aspect, h / w)
            gl.uniform1i(uniforms.color, 0)
            gl.uniform1i(uniforms.height, 1)
            gl.drawElements(gl.TRIANGLES, indices.length, gl.UNSIGNED_SHORT, 0)
        },
    }
}
