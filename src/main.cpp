#include <glad/glad.h>
#include "shader_cache/shader_cache.hpp"
#include "text_renderer/text_renderer.hpp"
#include "vertex_geometry/vertex_geometry.hpp"
#include "window/window.hpp"
#include "sound_system/sound_system.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip> // For formatting output
#include <vector>
#include <glm/vec3.hpp> // Ensure you include the GLM library for glm::vec3

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

glm::vec3 center(0.0f, 0.0f, 0.0f);
float width = 2.0f;
float height = 2.0f;
int num_rectangles_x = 5;
int num_rectangles_y = 5;
float spacing = 0.05f;

std::vector<glm::vec3> original_colors = {
    {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};

// Vertex Shader source code
static const char *vertex_shader_text = "#version 330 core\n"
                                        "layout (location = 0) in vec3 aPos;\n"
                                        "layout (location = 1) in vec3 aColor;\n"
                                        "out vec3 ourColor;\n"
                                        "void main()\n"
                                        "{\n"
                                        "   gl_Position = vec4(aPos, 1.0);\n"
                                        "   ourColor = aColor;\n"
                                        "}\0";

// Fragment Shader source code
static const char *fragment_shader_text = "#version 330 core\n"
                                          "in vec3 ourColor;\n"
                                          "out vec4 FragColor;\n"
                                          "void main()\n"
                                          "{\n"
                                          "   FragColor = vec4(ourColor, 1.0f);\n"
                                          "}\n\0";

bool is_point_in_rectangle(const Rectangle &rect, const glm::vec3 &point) {
    float half_width = rect.width / 2.0f;
    float half_height = rect.height / 2.0f;

    float left_bound = rect.center.x - half_width;
    float right_bound = rect.center.x + half_width;
    float bottom_bound = rect.center.y - half_height;
    float top_bound = rect.center.y + half_height;

    return (point.x >= left_bound && point.x <= right_bound && point.y >= bottom_bound && point.y <= top_bound);
}

static double mouse_x, mouse_y;

/**
 * @brief GLFW mouse callback function to get mouse position in screen coordinates.
 */
static void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    mouse_x = xpos;
    mouse_y = ypos;
}

bool lmb_pressed = false;
int ticks_lmb_pressed_for = 0;

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        lmb_pressed = true;
        ticks_lmb_pressed_for++;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        lmb_pressed = false;
        ticks_lmb_pressed_for = 0;
    }
}

/**
 * @brief Converts screen-space mouse coordinates to Normalized Device Coordinates (NDC).
 *
 * @param mouse_x The x-coordinate of the mouse in screen space (pixels).
 * @param mouse_y The y-coordinate of the mouse in screen space (pixels).
 * @param screen_width The width of the screen in pixels.
 * @param screen_height The height of the screen in pixels.
 * @return A pair of floats representing the mouse coordinates in NDC.
 *
 * @note NDC is a coordinate system in OpenGL that ranges from -1.0 to 1.0 for both x and y axes.
 *       The top-left corner of the screen corresponds to (-1, 1), and the bottom-right corresponds
 *       to (1, -1). Mouse coordinates provided by GLFW are in screen-space (pixels), which need to
 *       be transformed to this NDC space for rendering or interaction purposes.
 */
std::pair<float, float> convert_mouse_to_ndc(double mouse_x, double mouse_y, int screen_width, int screen_height) {
    float ndc_x = 2.0f * (mouse_x / screen_width) - 1.0f;
    float ndc_y = 1.0f - 2.0f * (mouse_y / screen_height);
    return {ndc_x, ndc_y};
}

struct OpenGLDrawingData {
    GLuint vbo_name;
    GLuint cbo_name;
    GLuint ibo_name;
    GLuint vao_name;
};

std::vector<glm::vec3> generate_colors_for_indices(const std::vector<glm::vec3> &input_colors) {
    std::vector<glm::vec3> duplicated_colors;
    duplicated_colors.reserve(input_colors.size() * 4); // Reserve space for efficiency

    for (const auto &color : input_colors) {
        // Push the same color four times
        duplicated_colors.push_back(color);
        duplicated_colors.push_back(color);
        duplicated_colors.push_back(color);
        duplicated_colors.push_back(color);
    }

    return duplicated_colors;
}

OpenGLDrawingData prepare_drawing_data_and_opengl_drawing_data() {

    IndexedVertices grid_data = generate_grid(center, width, height, num_rectangles_x, num_rectangles_y, spacing);

    auto vertex_colors = generate_colors_for_indices(original_colors);

    auto vertices = grid_data.vertices;
    auto indices = grid_data.indices;

    unsigned int vbo_name, cbo_name, vao_name, ibo_name;
    glGenVertexArrays(1, &vao_name);
    glGenBuffers(1, &vbo_name); // For vertices
    glGenBuffers(1, &cbo_name); // For colors
    glGenBuffers(1, &ibo_name); // For indices

    glBindVertexArray(vao_name);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_name);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, cbo_name);
    glBufferData(GL_ARRAY_BUFFER, vertex_colors.size() * sizeof(glm::vec3), vertex_colors.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_name);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return {vbo_name, cbo_name, ibo_name, vao_name};
}

GLuint create_shader_program() {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}

int main() {
    if (!glfwInit())
        return -1;

    GLFWwindow *window =
        initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "cjmines", true, false, false);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    auto [vbo_name, cbo_name, ibo_name, vao_name] = prepare_drawing_data_and_opengl_drawing_data();
    GLuint shader_program = create_shader_program();

    int aspect_ratio_loc = glGetUniformLocation(shader_program, "aspect_ratio");

    std::vector<Rectangle> grid_rectangles =
        generate_grid_rectangles(center, width, height, num_rectangles_x, num_rectangles_y, spacing);

    auto copied_colors = original_colors;

    SoundSystem sound_system;

    sound_system.load_sound_into_system_for_playback("flag", "assets/audio/flag/output_12.mp3");
    sound_system.create_sound_source("cell");

    ShaderCache shader_cache({ShaderType::TEXT});
    TextRenderer text_renderer("assets/fonts/cnr.otf", 50, SCREEN_WIDTH, SCREEN_HEIGHT, shader_cache);

    double previous_time = glfwGetTime();
    int frame_count = 0;
    float fps = 0;

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double current_time = glfwGetTime();
        frame_count++;
        if (current_time - previous_time >= 1.0) { // Update every second
            fps = frame_count / (current_time - previous_time);
            previous_time = current_time;
            frame_count = 0;
        }

        int current_width, current_height;
        glfwGetWindowSize(window, &current_width, &current_height);

        float aspect_ratio = (float)current_width / (float)current_height;
        aspect_ratio = 1;

        auto [ndc_x, ndc_y] = convert_mouse_to_ndc(mouse_x, mouse_y, current_width, current_height);
        glm::vec3 cursor_pos(ndc_x, ndc_y, 0);

        glUseProgram(shader_program);
        glUniform1f(aspect_ratio_loc, aspect_ratio);

        auto vertex_colors = generate_colors_for_indices(copied_colors);

        glBindBuffer(GL_ARRAY_BUFFER, cbo_name);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_colors.size() * sizeof(glm::vec3), vertex_colors.data());

        glBindVertexArray(vao_name);
        glDrawElements(GL_TRIANGLES, 6 * grid_rectangles.size(), GL_UNSIGNED_INT, 0);

        for (size_t i = 0; i < grid_rectangles.size(); ++i) {
            Rectangle rect = grid_rectangles.at(i);
            text_renderer.render_text(std::to_string(i), rect.center, 1, {.5, .5, .2});
            if (is_point_in_rectangle(rect, cursor_pos)) {
                copied_colors[i] = {1.0f, 1.0f, 1.0f};
                if (lmb_pressed and ticks_lmb_pressed_for == 1) {
                    sound_system.play_sound("cell", "flag");
                }
            } else {
                copied_colors[i] = original_colors[i];
            }
        }

        std::stringstream ss;
        ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
        std::string fps_text = ss.str();

        glm::vec2 text_dims = text_renderer.get_text_dimensions_in_ndc(fps_text, 1);
        text_renderer.render_text(fps_text, glm::vec2(1, 1) - text_dims, 1, {0, 1, 0});

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
