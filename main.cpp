#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <opencv2/opencv.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "utils.hpp"

// --- GL helpers ---
GLuint createComputeProgram(const std::string& src) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);
    GLint ok; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::cerr << "Shader error:\n" << log << "\n";
        exit(1);
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(prog, sizeof log, nullptr, log);
        std::cerr << "Program error:\n" << log << "\n";
        exit(1);
    }
    glDeleteShader(shader);
    return prog;
}

std::string loadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video.mp4>\n";
        return 1;
    }

    // --- Init OpenGL (headless) ---
    if (!glfwInit()) { std::cerr << "GLFW failed\n"; return 1; }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glewInit();

    // --- Load shader ---
    std::string shaderSrc = loadFile("compute_shader.glsl");
    GLuint prog = createComputeProgram(shaderSrc);

    // --- Video ---
    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) { std::cerr << "Can't open video\n"; return 1; }
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;

    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    auto frameDur = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(1.0 / fps)
    );
    Clock::time_point nextFrame = Clock::now();

    // --- OpenGL resources ---
    GLuint tex, ssbo;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glCreateBuffers(1, &ssbo);

    // --- Terminal ---
    TerminalSize term;
    term.update();

    // --- Main loop ---
    cv::Mat frame;
    bool first = true;

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            cap >> frame;
            if (frame.empty()) break;
        }

        term.update();
        if (term.cols < 10 || term.rows < 5) {
            write(STDOUT_FILENO, "\033[2J\033[H[Terminal too small]\n", 29);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int outW = term.cols;
        int outH = term.rows;

        // Upload frame to GPU
        cv::Mat bgra;
        cv::cvtColor(frame, bgra, cv::COLOR_BGR2BGRA);
        glTextureStorage2D(tex, 1, GL_RGBA8, bgra.cols, bgra.rows);
        glTextureSubImage2D(tex, 0, 0, 0, bgra.cols, bgra.rows, GL_BGRA, GL_UNSIGNED_BYTE, bgra.data);

        // Resize SSBO
        size_t ssboSize = outW * outH * sizeof(uint32_t);
        glNamedBufferStorage(ssbo, ssboSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

        // Run compute shader
        glUseProgram(prog);
        glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
        glUniform1i(glGetUniformLocation(prog, "outputWidth"), outW);
        glUniform1i(glGetUniformLocation(prog, "outputHeight"), outH);
        glDispatchCompute((outW + 7) / 8, (outH + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Read result
        std::vector<uint32_t> output(outW * outH);
        glGetNamedBufferSubData(ssbo, 0, ssboSize, output.data());

        // Build ANSI frame
        std::string ansi;
        if (first) {
            ansi = "\033[2J"; //clear
            first = false;
        }
        ansi += "\033[H"; // cursor home

        for (int y = 0; y < outH; ++y) {
            for (int x = 0; x < outW; ++x) {
                uint32_t v = output[y * outW + x];
                uint8_t ch = v & 0xFF;
                uint8_t color = (v >> 8) & 0xFF;
                char esc[32];
                int len = snprintf(esc, sizeof esc, "\033[38;5;%dm", color);
                ansi.append(esc, len);
                ansi += (ch ? static_cast<char>(ch) : ' ');
            }
            ansi += '\n';
        }
        ansi += "\033[0m";
        write(STDOUT_FILENO, ansi.data(), ansi.size());

        // Sync to FPS
        nextFrame += frameDur;
        std::this_thread::sleep_until(nextFrame);
    }

    // --- Cleanup ---
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &ssbo);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}