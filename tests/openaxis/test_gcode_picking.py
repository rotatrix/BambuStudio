"""Exercise the shipped extrusion shader and one-pixel picking on Mesa/EGL.

Run on Linux with the system EGL/OpenGL runtime; no window or Python packages
are needed. This does not replace the native viewer's layer/filter GUI check.
"""
import ctypes as C
import ctypes.util
import os
from pathlib import Path
import sys
import unittest


class GCodePicking(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if sys.platform != "linux":
            raise unittest.SkipTest("headless EGL check runs on Linux")
        os.environ.setdefault("EGL_PLATFORM", "surfaceless")
        egl_name = ctypes.util.find_library("EGL")
        gl_name = ctypes.util.find_library("GL")
        if not egl_name or not gl_name:
            if os.environ.get("CI"):
                raise RuntimeError("required EGL/OpenGL runtime unavailable")
            raise unittest.SkipTest("EGL/OpenGL runtime unavailable")
        cls.egl = C.CDLL(egl_name)
        cls.gl = C.CDLL(gl_name)

        def bind(lib, name, result, *args):
            fn = getattr(lib, name)
            fn.restype, fn.argtypes = result, args
            return fn

        ptr, integer, uint = C.c_void_p, C.c_int, C.c_uint
        cls.bind = staticmethod(bind)
        display = bind(cls.egl, "eglGetDisplay", ptr, ptr)(None)
        major, minor = integer(), integer()
        if not bind(cls.egl, "eglInitialize", uint, ptr, ptr, ptr)(display, C.byref(major), C.byref(minor)):
            raise RuntimeError("EGL initialization failed")
        cls.display = display
        assert bind(cls.egl, "eglBindAPI", uint, uint)(0x30A2)  # OpenGL
        attrs = (integer * 11)(0x3033, 1, 0x3040, 8, 0x3025, 24, 0x3024, 8, 0x3023, 8, 0x3038)
        config, count = ptr(), integer()
        assert bind(cls.egl, "eglChooseConfig", uint, ptr, ptr, ptr, integer, ptr)(display, attrs, C.byref(config), 1, C.byref(count)) and count.value
        cls.surface = bind(cls.egl, "eglCreatePbufferSurface", ptr, ptr, ptr, ptr)(display, config, (integer * 5)(0x3057, 1, 0x3056, 1, 0x3038))
        cls.context = bind(cls.egl, "eglCreateContext", ptr, ptr, ptr, ptr, ptr)(display, config, None, (integer * 1)(0x3038))
        assert cls.surface and cls.context
        assert bind(cls.egl, "eglMakeCurrent", uint, ptr, ptr, ptr, ptr)(display, cls.surface, cls.surface, cls.context)

        cls.create_shader = bind(cls.gl, "glCreateShader", uint, uint)
        cls.shader_source = bind(cls.gl, "glShaderSource", None, uint, integer, ptr, ptr)
        cls.compile_shader = bind(cls.gl, "glCompileShader", None, uint)
        cls.shader_iv = bind(cls.gl, "glGetShaderiv", None, uint, uint, ptr)
        cls.shader_log = bind(cls.gl, "glGetShaderInfoLog", None, uint, integer, ptr, ptr)
        vertex = """#version 140
uniform float fixture_depth;
out float moveId;
out vec3 frag_normal;
out vec3 frag_pos;
void main() {
    vec2 p[3] = vec2[3](vec2(-0.25,-0.25),vec2(0.25,-0.25),vec2(0.0,0.25));
    gl_Position = vec4(p[gl_VertexID], fixture_depth * 2.0 - 1.0, 1.0);
    moveId = 0.0; frag_normal = vec3(0,0,1); frag_pos = vec3(0,0,-1);
}
"""
        fragment = (Path(__file__).resolve().parents[2] / "resources/shaders/140/gcode.fs").read_text()
        shaders = []
        for kind, source in ((0x8B31, vertex), (0x8B30, fragment)):
            shader = cls.create_shader(kind)
            text = C.c_char_p(source.encode())
            cls.shader_source(shader, 1, C.byref(text), None)
            cls.compile_shader(shader)
            status = integer()
            cls.shader_iv(shader, 0x8B81, C.byref(status))
            log = C.create_string_buffer(8192)
            cls.shader_log(shader, len(log), None, log)
            if not status.value:
                raise AssertionError(log.value.decode())
            shaders.append(shader)
        cls.program = bind(cls.gl, "glCreateProgram", uint)()
        attach = bind(cls.gl, "glAttachShader", None, uint, uint)
        for shader in shaders:
            attach(cls.program, shader)
        bind(cls.gl, "glLinkProgram", None, uint)(cls.program)
        status = integer()
        bind(cls.gl, "glGetProgramiv", None, uint, uint, ptr)(cls.program, 0x8B82, C.byref(status))
        assert status.value, "shader link failed"
        bind(cls.gl, "glUseProgram", None, uint)(cls.program)
        cls.location = bind(cls.gl, "glGetUniformLocation", integer, uint, C.c_char_p)
        cls.uniform_i = bind(cls.gl, "glUniform1i", None, integer, integer)
        cls.uniform_f = bind(cls.gl, "glUniform1f", None, integer, C.c_float)
        cls.uniform_i(cls.location(cls.program, b"s_per_move_data_texture"), 1)
        cls.uniform_i(cls.location(cls.program, b"s_color_range_texture"), 0)
        bind(cls.gl, "glUniform3f", None, integer, C.c_float, C.c_float, C.c_float)(cls.location(cls.program, b"u_rangeType_isUnlit_topLayerIndex"), 0, 1, 0)

        cls.buffer = uint()
        bind(cls.gl, "glGenBuffers", None, integer, ptr)(1, C.byref(cls.buffer))
        bind(cls.gl, "glBindBuffer", None, uint, uint)(0x8C2A, cls.buffer)
        cls.buffer_data = bind(cls.gl, "glBufferData", None, uint, C.c_ssize_t, ptr, uint)
        cls.buffer_data(0x8C2A, 12, (C.c_float * 3)(10, 0, 0), 0x88E8)
        texture = uint()
        bind(cls.gl, "glGenTextures", None, integer, ptr)(1, C.byref(texture))
        bind(cls.gl, "glActiveTexture", None, uint)(0x84C1)
        bind(cls.gl, "glBindTexture", None, uint, uint)(0x8C2A, texture)
        bind(cls.gl, "glTexBuffer", None, uint, uint, uint)(0x8C2A, 0x8815, cls.buffer)
        cls.viewport = bind(cls.gl, "glViewport", None, integer, integer, integer, integer)
        cls.clear = bind(cls.gl, "glClear", None, uint)
        cls.draw = bind(cls.gl, "glDrawArrays", None, uint, integer, integer)
        cls.read = bind(cls.gl, "glReadPixels", None, integer, integer, integer, integer, uint, uint, ptr)
        cls.error = bind(cls.gl, "glGetError", uint)
        bind(cls.gl, "glEnable", None, uint)(0x0B71)
        bind(cls.gl, "glDepthFunc", None, uint)(0x0201)
        bind(cls.gl, "glClearDepth", None, C.c_double)(1)

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "display"):
            cls.bind(cls.egl, "eglMakeCurrent", C.c_uint, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)(cls.display, None, None, None)
            cls.bind(cls.egl, "eglTerminate", C.c_uint, C.c_void_p)(cls.display)

    def sample(self, x, y, draws, picking=True):
        # Physical top-left pixels -> translated OpenGL bottom-left viewport.
        self.viewport(-x, -(100 - 1 - y), 100, 100)
        self.clear(0x00000100)
        self.uniform_i(self.location(self.program, b"u_pick_extrusions"), picking)
        for kind, depth in draws:
            self.buffer_data(0x8C2A, 12, (C.c_float * 3)(kind, 0, 0), 0x88E8)
            self.uniform_f(self.location(self.program, b"fixture_depth"), depth)
            self.draw(0x0004, 0, 3)
        depth = C.c_float()
        self.read(0, 0, 1, 1, 0x1902, 0x1406, C.byref(depth))
        self.assertEqual(self.error(), 0, "OpenGL error")
        return depth.value

    def test_nearest_extrusion_and_empty_pixel(self):
        self.assertAlmostEqual(self.sample(50, 50, [(10, .7), (10, .3)]), .3, places=5)
        self.assertEqual(self.sample(5, 5, [(10, .3)]), 1)

    def test_travel_and_wipe_do_not_occlude_extrusion(self):
        for kind in (8, 9):
            self.assertEqual(self.sample(50, 50, [(kind, .1)]), 1)
            self.assertAlmostEqual(self.sample(50, 50, [(kind, .1), (10, .6)]), .6, places=5)

    def test_normal_render_still_draws_travel(self):
        self.assertAlmostEqual(self.sample(50, 50, [(8, .1)], picking=False), .1, places=5)

    def test_top_left_pixel_convention(self):
        # The fixture widens toward its bottom; inverted Y would swap these.
        self.assertAlmostEqual(self.sample(58, 58, [(10, .4)]), .4, places=5)
        self.assertEqual(self.sample(58, 42, [(10, .4)]), 1)


if __name__ == "__main__":
    unittest.main()
