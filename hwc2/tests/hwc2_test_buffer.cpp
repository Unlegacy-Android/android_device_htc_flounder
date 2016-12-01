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
layout(location = 0) out vec4 frag_color;

void main() {
    vec2 p = gl_FragCoord.xy / resolution.xy;
    p.y = 1.0 - p.y;

    if (p.x < 0.0 || p.x >= resolution.x || p.y < 0.0 || p.y >= resolution.y)
        return;

    float wd = resolution.x / 3.0;
    float hd = resolution.y / 5.0;

    if (p.x < wd)
        frag_color = vec4(1.0, 0.0, 0.0, 1.0);
    else if (p.x < wd * 2.0)
        frag_color = vec4(0.0, 1.0, 0.0, 1.0);
    else
        frag_color = vec4(0.0, 0.0, 1.0, 1.0);

    if (p.y >= resolution.y - hd)
        frag_color = clamp(frag_color, 0.0, 0.33);
    else if (p.y >= resolution.y - hd * 2.0)
        frag_color = clamp(frag_color, 0.0, 0.66);
    else if (p.y < resolution.y - hd * 4.0)
        frag_color = clamp(frag_color, 0.66, 1.0);
    else if (p.y < resolution.y - hd * 3.0)
        frag_color = clamp(frag_color, 0.33, 1.0);
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

    void get_next()
    {
        egl_manager.make_current();

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangles.data());

        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, triangles.size());

        egl_manager.present();
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
    this->buffer_width = buffer_width;
    this->buffer_height = buffer_height;
    buffer_generator.reset();
}

void hwc2_test_buffer::update_format(android_pixel_format_t format)
{
    this->format = format;
    buffer_generator.reset();
}

/* Calls into buffer_generator which returns the buffer through set_buffer which
 * calls into set_buffer_locked. set_bufer_locked sets the handle/fence and
 * signals get */
int hwc2_test_buffer::get(buffer_handle_t *out_handle, int32_t *out_fence)
{
    if (buffer_width == -1 || buffer_height == -1 || format == 0)
        return -EINVAL;

    if (buffer_generator) {
        *out_handle = handle;
        *out_fence = dup(fence);
        return 0;
    }

    close_fence();

    buffer_generator.reset(new hwc2_test_buffer_generator());
    int ret = buffer_generator->initialize(buffer_width, buffer_height, format,
            set_buffer, this);
    if (ret)
        return ret;

    std::unique_lock<std::mutex> lock(mutex);

    while (pending != false)
        if (cv.wait_for(lock, std::chrono::seconds(2)) == std::cv_status::timeout)
            return -ETIME;

    buffer_generator->get_next();

    while (pending != true)
        if (cv.wait_for(lock, std::chrono::seconds(2)) == std::cv_status::timeout)
            return -ETIME;

    pending = false;
    *out_handle = handle;
    *out_fence = dup(fence);

    return 0;
}

void hwc2_test_buffer::set(buffer_handle_t handle, int32_t fence)
{
    this->handle = handle;
    this->fence = fence;
    pending = true;

    cv.notify_all();
}

void hwc2_test_buffer::close_fence()
{
    if (fence >= 0)
        close(fence);
    fence = -1;
}
