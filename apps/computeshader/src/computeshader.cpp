#include <gli.h>

#include <opengl/glad.h>

static const char* s_shader_source[2]{ // Vertex shader
                                       R"(
#version 400 core

in vec2 vPos;
out vec2 vTexCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(vPos, 0, 1);
    vTexCoord = (vPos + 1.0) * vec2(0.5, -0.5);
}
    )",

                                       // Fragment shader
                                       R"(
#version 400 core

uniform sampler2D sDiffuseMap;
in vec2 vTexCoord;
out vec4 oColor;

void main()
{
    oColor = vec4(texture(sDiffuseMap, vTexCoord).rgb, 1);
}
    )"
};


class ComputeShader : public gli::App
{
public:
    GLuint _shader_program{};
    GLuint _compute_program{};
    GLuint _vbo{};
    GLuint _vao{};
    GLuint _texture{};
    bool _reload_compute_shader{};

    static const int _tex_w = 512;
    static const int _tex_h = 512;

    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;
    void on_render(float delta) override;

    void load_compute_shader();
};

bool ComputeShader::on_create()
{
    GLint status;
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &s_shader_source[0], nullptr);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(vertex_shader, log_length, nullptr, &log[0]);
        gli::logf("Vertex shader failed to compile:\n%s\n", log.c_str());
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &s_shader_source[1], nullptr);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(fragment_shader, log_length, nullptr, &log[0]);
        gli::logf("Fragment shader failed to compile:\n%s\n", log.c_str());
        return false;
    }

    _shader_program = glCreateProgram();
    glAttachShader(_shader_program, vertex_shader);
    glAttachShader(_shader_program, fragment_shader);
    glLinkProgram(_shader_program);
    glGetProgramiv(_shader_program, GL_LINK_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetProgramiv(_shader_program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetProgramInfoLog(_shader_program, log_length, nullptr, &log[0]);
        gli::logf("Program failed to link:\n%s\n", log.c_str());
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGenBuffers(1, &_vbo);
    glGenVertexArrays(1, &_vao);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);

    static const float verts[] = { -1.0f, 1.0f, -1.0f, -3.0f, 3.0f, 1.0f };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenTextures(1, &_texture);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _tex_w, _tex_h, 0, GL_RGBA, GL_FLOAT, NULL);

    _reload_compute_shader = true;

    return true;
}

void ComputeShader::on_destroy()
{
    glDeleteTextures(1, &_texture);
    glDeleteVertexArrays(1, &_vao);
    glDeleteBuffers(1, &_vbo);
    glDeleteProgram(_shader_program);
}

bool ComputeShader::on_update(float delta)
{
    if (key_state(gli::Key_F5).pressed)
    {
        _reload_compute_shader = true;
    }
    return true;
}

void ComputeShader::on_render(float delta)
{
    if (_reload_compute_shader)
    {
        load_compute_shader();
        _reload_compute_shader = false;
    }

    if (_compute_program)
    {
        glBindImageTexture(0, _texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        // launch compute shaders!
        glUseProgram(_compute_program);
        glDispatchCompute(_tex_w, _tex_h, 1);

        // make sure writing to image has finished before read
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
    }

    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glUseProgram(_shader_program);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void ComputeShader::load_compute_shader()
{
    std::vector<uint8_t> compute_shader_source;

    if (!gli::FileSystem::get()->read_entire_file("res/compute_shader.glsl", compute_shader_source))
    {
        gliLog(gli::LogLevel::Error, "App", "ComputeShader::load_compute_shader", "Failed to load compute shader source");
        return;
    }

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    GLchar* source = (GLchar*)std::calloc(compute_shader_source.size() + 1, 1);
    std::memcpy(source, &compute_shader_source[0], compute_shader_source.size());
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    std::free(source);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, &log[0]);
        gli::logf("Compute shader failed to compile:\n%s\n", log.c_str());
        glDeleteShader(shader);
        return;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, &log[0]);
        gli::logf("Compute program failed to link:\n%s\n", log.c_str());
        glDeleteProgram(program);
        return;
    }

    glDeleteProgram(_compute_program);
    _compute_program = program;
}

ComputeShader app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("ComputeShader", 512, 512, 2))
    {
        app.run();
    }

    return 0;
}
