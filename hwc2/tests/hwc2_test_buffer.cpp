/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mutex>
#include <array>
#include <sstream>

#include <gui/Surface.h>
#include <gui/BufferItemConsumer.h>

#include <ui/GraphicBuffer.h>
#include <ui/vec4.h>

#include <GLES3/gl3.h>

#include "hwc2_test_buffer.h"
#include "hwc2_test_layers.h"

#define HWC2_TEST_BUFFER_MAX 10

using namespace android;

typedef void (*buffer_callback)(sp<GraphicBuffer> graphic_buffer,
        sp<Fence> fence, void *callback_args);


class hwc2_test_surface_manager {
public:
    class buffer_listener : public ConsumerBase::FrameAvailableListener {
    public:
        buffer_listener(sp<IGraphicBufferConsumer> consumer,
                buffer_callback callback, void *callback_args)
            : consumer(consumer),
              callback(callback),
              callback_args(callback_args) { }

        void onFrameAvailable(const BufferItem& /*item*/)
        {
            BufferItem item;

            if (consumer->acquireBuffer(&item, 0))
                return;
            if (consumer->detachBuffer(item.mSlot))
                return;

            if (item.mGraphicBuffer.get())
                callback(item.mGraphicBuffer, item.mFence, callback_args);
        }
    private:
        sp<IGraphicBufferConsumer> consumer;
        buffer_callback callback;
        void *callback_args;
    };

    void initialize(int32_t buffer_width, int32_t buffer_height,
            android_pixel_format_t format, buffer_callback callback,
            void *callback_args)
    {
        sp<IGraphicBufferProducer> producer;
        sp<IGraphicBufferConsumer> consumer;
        BufferQueue::createBufferQueue(&producer, &consumer);

        consumer->setDefaultBufferSize(buffer_width, buffer_height);
        consumer->setDefaultBufferFormat(format);

        buffer_item_consumer = new BufferItemConsumer(consumer,
                GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_RENDER);

        listener = new buffer_listener(consumer, callback, callback_args);
        buffer_item_consumer->setFrameAvailableListener(listener);

        surface = new Surface(producer, true);
    }

    sp<Surface> get_surface() const
    {
        return surface;
    }

private:
    sp<BufferItemConsumer> buffer_item_consumer;
    sp<buffer_listener> listener;
    sp<Surface> surface;
};


class hwc2_test_egl_manager {
public:
    hwc2_test_egl_manager()
        : surface(),
          egl_display(EGL_NO_DISPLAY),
          egl_surface(EGL_NO_SURFACE),
          egl_context(EGL_NO_CONTEXT) { }

    ~hwc2_test_egl_manager()
    {
        cleanup();
    }

    int initialize(sp<Surface> surface)
    {
        this->surface = surface;

        egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (egl_display == EGL_NO_DISPLAY) return false;

        EGLint major;
        EGLint minor;
        if (!eglInitialize(egl_display, &major, &minor)) {
            ALOGW("Could not initialize EGL");
            return false;
        }

        /* We're going to use a 1x1 pbuffer surface later on
         * The configuration doesn'distance really matter for what we're trying to
         * do */
        EGLint config_attrs[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 0,
                EGL_DEPTH_SIZE, 24,
                EGL_STENCIL_SIZE, 0,
                EGL_NONE
        };

        EGLConfig configs[1];
        EGLint config_count;
        if (!eglChooseConfig(egl_display, config_attrs, configs, 1,
                &config_count)) {
            ALOGW("Could not select EGL configuration");
            eglReleaseThread();
            eglTerminate(egl_display);
            return false;
        }

        if (config_count <= 0) {
            ALOGW("Could not find EGL configuration");
            eglReleaseThread();
            eglTerminate(egl_display);
            return false;
        }

        /* These objects are initialized below but the default "null" values are
         * used to cleanup properly at any point in the initialization sequence */
        EGLint attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        egl_context = eglCreateContext(egl_display, configs[0], EGL_NO_CONTEXT,
                attrs);
        if (egl_context == EGL_NO_CONTEXT) {
            ALOGW("Could not create EGL context");
            cleanup();
            return false;
        }

        EGLint surface_attrs[] = { EGL_NONE };
        egl_surface = eglCreateWindowSurface(egl_display, configs[0],
                surface.get(), surface_attrs);
        if (egl_surface == EGL_NO_SURFACE) {
            ALOGW("Could not create EGL surface");
            cleanup();
            return false;
        }

        if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
            ALOGW("Could not change current EGL context");
            cleanup();
            return false;
        }

        return true;
    }

    void make_current() const
    {
        eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    }

    void present() const
    {
        eglSwapBuffers(egl_display, egl_surface);
    }

private:
    void cleanup()
    {
        if (egl_display == EGL_NO_DISPLAY)
            return;
        if (egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(egl_display, egl_surface);
        if (egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(egl_display, egl_context);

        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                EGL_NO_CONTEXT);
        eglReleaseThread();
        eglTerminate(egl_display);
    }

    sp<Surface> surface;
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;
};


class hwc2_test_program {
public:
    hwc2_test_program()
        : program(0),
          vertex_shader(0),
          fragment_shader(0) { }

    ~hwc2_test_program()
    {
        glDetachShader(program, vertex_shader);
        glDetachShader(program, fragment_shader);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        glDeleteProgram(program);
    }

    void initialize(const char* vertex, const char* fragment)
    {
        vertex_shader = build_shader(vertex, GL_VERTEX_SHADER);
        fragment_shader = build_shader(fragment, GL_FRAGMENT_SHADER);

        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);

        glLinkProgram(program);

        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length > 1) {
                GLchar log[length];
                glGetProgramInfoLog(program, length, nullptr, &log[0]);
                ALOGE("%s", log);
            }
            LOG_ALWAYS_FATAL("Error while linking shaders");
        }
    }

    void use() const
    {
        glUseProgram(program);
    }

    void bind_vec4f(GLint location, vec4 v) const
    {
        glUniform4f(location, v.x, v.y, v.z, v.w);
    }

    void bind_vec1iv(GLint location, GLsizei count,
           const GLint *v) const
    {
        glUniform1iv(location, count, v);
    }

    void bind_vec1fv(GLint location, GLsizei count,
           const float *v) const
    {
        glUniform1fv(location, count, v);
    }

    void bind_vec4fv(GLint location, GLsizei count,
           const vec4 *v) const
    {
        glUniform4fv(location, count, &(v->x));
    }

private:
    GLuint build_shader(const char* source, GLenum type) const
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            ALOGE("Error while compiling shader of type 0x%x:\n===\n%s\n===",
                    type, source);
            /* Some drivers return wrong values for GL_INFO_LOG_LENGTH use a
             * fixed size instead */
            GLchar log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, &log[0]);
            LOG_ALWAYS_FATAL("Shader info log: %s", log);
            return 0;
        }
        return shader;
    }

    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
};


static const char* VERTEX_SHADER = R"SHADER__(
#version 300 es
#extension GL_ARB_explicit_uniform_location : enable
precision highp float;

layout(location = 0) in vec4 mesh_position;

void main() {
    gl_Position = mesh_position;
}
)SHADER__";

static const char* FRAGMENT_SHADER = R"SHADER__(
#version 300 es
#extension GL_ARB_explicit_uniform_location : enable
precision highp float;

layout(location = 0) uniform vec4 resolution;
layout(location = 1) uniform vec4[10] display_frames;
layout(location = 11) uniform vec4[10] source_crops;
layout(location = 21) uniform int[10] is_color_layer;
layout(location = 31) uniform vec4[10] colors;
layout(location = 41) uniform int[10] transforms;
layout(location = 51) uniform int[10] blend_modes;
layout(location = 61) uniform float[10] plane_alphas;

layout(location = 0) out vec4 frag_color;

void draw(in vec2 p, in int idx)
{
    float DF_l = display_frames[idx].x;
    float DF_t = display_frames[idx].y;
    float DF_r = display_frames[idx].z;
    float DF_b = display_frames[idx].w;
    float DF_w = DF_r - DF_l;
    float DF_h = DF_b - DF_t;
    float P_x = p.x;
    float P_y = p.y;

    if (P_x < DF_l || P_x >= DF_r || P_y < DF_t || P_y >= DF_b)
        return;

    if (is_color_layer[idx] != 0) {
        frag_color = colors[idx];
        frag_color.w *= plane_alphas[idx];
        return;
    }

    int transform = transforms[idx];

    if (transform > 0) {
        /* Change origin */
        P_x = P_x - DF_l - DF_w / 2.0;
        P_y = P_y - DF_t - DF_h / 2.0;

        /* Flip Horizontal */
        if ((transform & 1) == 1)
            P_x = -P_x;

        /* Flip vertical */
        if ((transform & 2) == 2)
            P_y = -P_y;

        /* Rotate 90 */
        if ((transform & 4) == 4) {
            float tmp = P_x;
            P_x = -P_y * DF_w / DF_h;
            P_y = tmp * DF_h / DF_w;
        }

        /* Change origin back */
        P_x = P_x + DF_l + DF_w / 2.0;
        P_y = P_y + DF_t + DF_h / 2.0;
    }

    float SC_l = source_crops[idx].x;
    float SC_t = source_crops[idx].y;
    float DF_w_div_SC_w = DF_w / (source_crops[idx].z - SC_l);
    float DF_h_div_SC_h = DF_h / (source_crops[idx].w - SC_t);

    /* Equation used to calculate the color to display on the screen given
     * source crop and display frame.
     * P_x < ((CLR_i / CLR_n) - SC_left) * (DF_width / SC_width) + DF_left
     */

    vec4 source_color;

    if (P_x < ((1.0 / 3.0) - SC_l) * (DF_w_div_SC_w) + DF_l)
        source_color = vec4(1.0, 0.0, 0.0, 1.0);
    else if (P_x < ((2.0 / 3.0) - SC_l) * (DF_w_div_SC_w) + DF_l)
        source_color = vec4(0.0, 1.0, 0.0, 1.0);
    else if (P_x < ((3.0 / 3.0) - SC_l) * (DF_w_div_SC_w) + DF_l)
        source_color = vec4(0.0, 0.0, 1.0, 1.0);

    /* Equation used to calculate the shade color to display on the screen given
     * source crop and display frame.
     * P_y < ((CLR_i / CLR_n) - SC_top) * (DF_height / SC_height) + DF_top
     */

    if (P_y < ((1.0 / 5.0) - SC_t) * DF_h_div_SC_h + DF_t)
        source_color = clamp(source_color, 0.66, 1.0);
    else if (P_y < ((2.0 / 5.0) - SC_t) * DF_h_div_SC_h + DF_t)
        source_color = clamp(source_color, 0.33, 1.0);
    else if (P_y >= ((4.0 / 5.0) - SC_t) * DF_h_div_SC_h + DF_t)
        source_color = clamp(source_color, 0.0, 0.33);
    else if (P_y >= ((3.0 / 5.0) - SC_t) * DF_h_div_SC_h + DF_t)
        source_color = clamp(source_color, 0.0, 0.66);

    int blend_mode = blend_modes[idx];

    /* If the blend mode is HWC2_BLEND_MODE_PREMULTIPLIED */
    if (blend_mode == 2)
        source_color = source_color * plane_alphas[idx];
    else
        source_color.w = source_color.w * plane_alphas[idx];

    /* If the blend mode is HWC2_BLEND_MODE_PREMULTIPLIED */
    if (blend_mode == 2)
        frag_color = source_color + frag_color * (1.0 - source_color.w);
    /* If the blend mode is HWC2_BLEND_MODE_COVERAGE */
    else if (blend_mode == 3)
        frag_color = source_color * source_color.w
                + frag_color * (1.0 - source_color.w);
    /* If the blend mode is HWC2_BLEND_MODE_NONE */
    else
        frag_color = source_color;
}

void main()
{
    vec2 p = gl_FragCoord.xy / resolution.xy;
    p.y = 1.0 - p.y;

    for (int idx = 0; idx < 10; idx++)
        draw(p, idx);
}
)SHADER__";

static const std::array<vec2, 4> triangles = {{
    {  1.0f,  1.0f },
    { -1.0f,  1.0f },
    {  1.0f, -1.0f },
    { -1.0f, -1.0f },
}};

class hwc2_test_buffer_generator {
public:
    hwc2_test_buffer_generator()
        : surface_manager(),
          egl_manager(),
          program(),
          buffer_width(-1),
          buffer_height(-1) { }

    ~hwc2_test_buffer_generator()
    {
        egl_manager.make_current();
    }

    int initialize(int32_t buffer_width, int32_t buffer_height,
            android_pixel_format_t format, buffer_callback callback,
            void *callback_args)
    {
        this->buffer_width = buffer_width;
        this->buffer_height = buffer_height;

        surface_manager.initialize(buffer_width, buffer_height, format,
               callback, callback_args);

        if (!egl_manager.initialize(surface_manager.get_surface()))
            return -EINVAL;

        egl_manager.make_current();

        program.initialize(VERTEX_SHADER, FRAGMENT_SHADER);
        program.use();
        program.bind_vec4f(0, vec4{buffer_width, buffer_height,
                1.0f / buffer_width, 1.0f / buffer_height});

        glClearColor(0.0, 0.0, 0.0, 1.0);

        glEnableVertexAttribArray(0);

        return 0;
    }

    void send_layer_properties(const hwc2_test_layers *test_layers,
            const std::set<hwc2_layer_t> *client_layers)
    {
        std::vector<vec4> display_frames;
        std::vector<vec4> source_crops;
        std::vector<GLint> is_color_layer;
        std::vector<vec4> colors;
        std::vector<GLint> transforms;
        std::vector<GLint> blend_modes;
        std::vector<float> plane_alphas;

        if (test_layers && client_layers) {
            /* The client layers are ordered from highest z order to lowest z order */
            for (auto layer = client_layers->rbegin();
                    layer != client_layers->rend(); layer++) {

                const auto &frame = test_layers->get_display_frame(*layer);
                display_frames.push_back({
                        static_cast<float>(frame.left) / buffer_width,
                        static_cast<float>(frame.top) / buffer_height,
                        static_cast<float>(frame.right) / buffer_width,
                        static_cast<float>(frame.bottom) / buffer_height});

                const auto &crop = test_layers->get_source_crop(*layer);
                source_crops.push_back({
                        crop.left / buffer_width, crop.top / buffer_height,
                        crop.right / buffer_width, crop.bottom / buffer_height});

                const auto &color = test_layers->get_color(*layer);
                colors.push_back({color.r, color.g, color.b, color.a});

                is_color_layer.push_back(test_layers->get_composition(*layer)
                        == HWC2_COMPOSITION_SOLID_COLOR);

                transforms.push_back(test_layers->get_transform(*layer));
                blend_modes.push_back(test_layers->get_blend_mode(*layer));
                plane_alphas.push_back(test_layers->get_plane_alpha(*layer));
            }
        } else {
            display_frames.push_back({0.0, 0.0, 1.0, 1.0});
            source_crops.push_back({0.0, 0.0, 1.0, 1.0});
            is_color_layer.push_back(0);
            colors.push_back({0.0, 0.0, 0.0, 0.0});
            transforms.push_back(0);
            blend_modes.push_back(1);
            plane_alphas.push_back(1.0);
        }

        display_frames.resize(HWC2_TEST_BUFFER_MAX, {0.0, 0.0, 0.0, 0.0});
        source_crops.resize(HWC2_TEST_BUFFER_MAX, {0.0, 0.0, 0.0, 0.0});
        is_color_layer.resize(HWC2_TEST_BUFFER_MAX, 0);
        colors.resize(HWC2_TEST_BUFFER_MAX, {0.0, 0.0, 0.0, 0.0});
        transforms.resize(HWC2_TEST_BUFFER_MAX, 0);
        blend_modes.resize(HWC2_TEST_BUFFER_MAX, 0);
        plane_alphas.resize(HWC2_TEST_BUFFER_MAX, 0.0);

        program.bind_vec4fv(1, display_frames.size(), display_frames.data());
        program.bind_vec4fv(11, source_crops.size(), source_crops.data());
        program.bind_vec1iv(21, is_color_layer.size(), is_color_layer.data());
        program.bind_vec4fv(31, colors.size(), colors.data());
        program.bind_vec1iv(41, transforms.size(), transforms.data());
        program.bind_vec1iv(51, blend_modes.size(), blend_modes.data());
        program.bind_vec1fv(61, plane_alphas.size(), plane_alphas.data());
    }

    int get_next(const hwc2_test_layers *test_layers,
            const std::set<hwc2_layer_t> *client_layers)
    {
        if (client_layers && client_layers->size() > HWC2_TEST_BUFFER_MAX)
            return -EINVAL;

        egl_manager.make_current();

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangles.data());

        glClear(GL_COLOR_BUFFER_BIT);

        send_layer_properties(test_layers, client_layers);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, triangles.size());

        egl_manager.present();

        return 0;
    }

private:
    hwc2_test_surface_manager surface_manager;
    hwc2_test_egl_manager egl_manager;
    hwc2_test_program program;

    int32_t buffer_width;
    int32_t buffer_height;
};


static void set_buffer(sp<GraphicBuffer> buffer, sp<Fence> fence,
        void *test_buffer)
{
    static_cast<hwc2_test_buffer *>(test_buffer)->set(buffer->handle,
            fence->dup());
}

hwc2_test_buffer::hwc2_test_buffer()
    : buffer_generator(),
      buffer_width(-1),
      buffer_height(-1),
      format(static_cast<android_pixel_format_t>(0)),
      mutex(),
      cv(),
      pending(false),
      handle(nullptr),
      fence(-1) { }

hwc2_test_buffer::~hwc2_test_buffer()
{
    close_fence();
}

void hwc2_test_buffer::update_buffer_area(int32_t buffer_width,
        int32_t buffer_height)
{
    if (this->buffer_width == buffer_width
            && this->buffer_height == buffer_height)
        return;

    this->buffer_width = buffer_width;
    this->buffer_height = buffer_height;
    buffer_generator.reset();
}

void hwc2_test_buffer::update_format(android_pixel_format_t format)
{
    if (this->format == format)
        return;

    this->format = format;
    buffer_generator.reset();
}

/* Save the generated handle and reuse it when possible */
int hwc2_test_buffer::get(buffer_handle_t *out_handle, int32_t *out_fence)
{
    if (buffer_generator && handle) {
        *out_handle = handle;
        *out_fence = dup(fence);
        return 0;
    }

    return get(out_handle, out_fence, nullptr, nullptr, true);
}

/* Do not save the generated handle because client layers can vary */
int hwc2_test_buffer::get(buffer_handle_t *out_handle, int32_t *out_fence,
        const hwc2_test_layers *test_layers,
        const std::set<hwc2_layer_t> *client_layers)
{
    return get(out_handle, out_fence, test_layers, client_layers, false);
}

void hwc2_test_buffer::set(buffer_handle_t handle, int32_t fence)
{
    this->handle = handle;
    this->fence = fence;
    pending = true;

    cv.notify_all();
}

/* Calls into buffer_generator which returns the buffer through set_buffer which
 * calls into set_buffer_locked. set_bufer_locked sets the handle/fence and
 * signals get */
int hwc2_test_buffer::get(buffer_handle_t *out_handle, int32_t *out_fence,
        const hwc2_test_layers *test_layers,
        const std::set<hwc2_layer_t> *client_layers, bool save_handle)
{
    if (buffer_width == -1 || buffer_height == -1 || format == 0)
        return -EINVAL;

    close_fence();

    if (!buffer_generator) {
        buffer_generator.reset(new hwc2_test_buffer_generator());
        int ret = buffer_generator->initialize(buffer_width, buffer_height,
                format, set_buffer, this);
        if (ret)
            return ret;
    }

    std::unique_lock<std::mutex> lock(mutex);

    while (pending != false)
        if (cv.wait_for(lock, std::chrono::seconds(2)) == std::cv_status::timeout)
            return -ETIME;

    int ret = buffer_generator->get_next(test_layers, client_layers);
    if (ret < 0)
        return ret;

    while (pending != true)
        if (cv.wait_for(lock, std::chrono::seconds(2)) == std::cv_status::timeout)
            return -ETIME;

    pending = false;
    *out_handle = handle;
    *out_fence = dup(fence);

    if (!save_handle)
        handle = nullptr;

    return 0;
}

void hwc2_test_buffer::close_fence()
{
    if (fence >= 0)
        close(fence);
    fence = -1;
}
