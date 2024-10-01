#include <glad/glad.h>
#include "shader_cache/shader_cache.hpp"
#include "text_renderer/text_renderer.hpp"
#include "vertex_geometry/vertex_geometry.hpp"
#include "window/window.hpp"
#include "sound_system/sound_system.hpp"
#include "game_logic/game_logic.hpp"
#include "game_logic/solver.hpp"
#include "colors/colors.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip> // For formatting output
#include <unordered_map>
#include <vector>
#include <glm/vec3.hpp> // Ensure you include the GLM library for glm::vec3

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

glm::vec3 center(0.0f, 0.0f, 0.0f);
float width = 2.0f;
float height = 2.0f;
float spacing = 0.01f;

std::unordered_map<unsigned int, Colors::ColorRGB> mine_count_to_color = {
    {0, Colors::color_map.at(Colors::ColorName::GRAY70)},
    {1, Colors::color_map.at(Colors::ColorName::LIGHTSKYBLUE)},
    {2, Colors::color_map.at(Colors::ColorName::AQUAMARINE3)},
    {3, Colors::color_map.at(Colors::ColorName::PASTELRED)},
    {4, Colors::color_map.at(Colors::ColorName::MUTEDLIMEGREEN)},
    {5, Colors::color_map.at(Colors::ColorName::MAROON2)},
    {6, Colors::color_map.at(Colors::ColorName::MUTEDHOTPINK)},
    {7, Colors::color_map.at(Colors::ColorName::MUSTARDYELLOW)},
};

auto unrevelead_cell_color = Colors::color_map.at(Colors::ColorName::BROWN);
auto flagged_cell_color = Colors::color_map.at(Colors::ColorName::BROWN);
auto ngs_start_pos_color = Colors::color_map.at(Colors::ColorName::LIMEGREEN);

auto text_color = Colors::color_map.at(Colors::ColorName::BLACK);
auto flag_text_color = Colors::color_map.at(Colors::ColorName::PURPLE);

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
bool lmb_pressed_last_tick = false;
bool rmb_pressed = false;
bool rmb_pressed_last_tick = false;

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        lmb_pressed = true;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        lmb_pressed = false;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rmb_pressed = true;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        rmb_pressed = false;
    }
}

bool left_shift_pressed = false;

bool flag_all_pressed = false;
bool flag_all_pressed_last_tick = false;

bool flag_one_pressed = false;
bool flag_one_pressed_last_tick = false;

bool mine_all_pressed = false;
bool mine_all_pressed_last_tick = false;

bool mine_one_pressed = false;
bool mine_one_pressed_last_tick = false;

bool show_times = false;

bool user_requested_quit=false;

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {

    if (key == GLFW_KEY_LEFT_SHIFT) {
        if (action == GLFW_PRESS) {
            left_shift_pressed = true;
        }
        if (action == GLFW_RELEASE) {
            left_shift_pressed = false;
        }
    }

    if (key == GLFW_KEY_F) {
        if (action == GLFW_PRESS) {
            if (left_shift_pressed) {
                flag_one_pressed = true;
            } else {
                flag_all_pressed = true;
            }
        }
        if (action == GLFW_RELEASE) {
            flag_all_pressed = false;
            flag_one_pressed = false;
        }
    }

    if (key == GLFW_KEY_TAB) {
        if (action == GLFW_PRESS) {
            show_times = !show_times;
        }
    }
    if (key == GLFW_KEY_Q){
	if (action == GLFW_PRESS){
	user_requested_quit = true;
	}	
    }
    if (key == GLFW_KEY_D) {
        if (action == GLFW_PRESS) {
            if (left_shift_pressed) {
                mine_one_pressed = true;
            } else {
                mine_all_pressed = true;
            }
        }
        if (action == GLFW_RELEASE) {
            mine_all_pressed = false;
            mine_one_pressed = false;
        }
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

std::vector<glm::vec3> normalize_rgb_colors(const std::vector<glm::vec3> &colors_255) {
    std::vector<glm::vec3> normalized_colors;

    for (const auto &color : colors_255) {
        normalized_colors.push_back(color / 255.0f); // Normalize each color
    }

    return normalized_colors;
}

OpenGLDrawingData prepare_drawing_data_and_opengl_drawing_data(unsigned int num_cells_x, unsigned int num_cells_y) {

    IndexedVertices grid_data = generate_grid(center, width, height, num_cells_x, num_cells_y, spacing);

    /*auto vertex_colors = generate_colors_for_indices(original_colors);*/

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
    /*glBufferData(GL_ARRAY_BUFFER, vertex_colors.size() * sizeof(glm::vec3), vertex_colors.data(), GL_STATIC_DRAW);*/

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

Board generate_ng_solvable_board(int mine_count, int num_cells_x, int num_cells_y) {
    Solver solver;
    Board board = generate_board(mine_count, num_cells_x, num_cells_y);
    // keep trying until we genrate a ngsolvable board
    std::optional<std::pair<int, int>> solution = solver.solve(board, mine_count);
    while (not solution.has_value()) {
        std::cout << "gnerating a new board and trying again" << std::endl;
        board = generate_board(mine_count, num_cells_x, num_cells_y);
        solution = solver.solve(board, mine_count);
    }

    auto [row, col] = solution.value();
    board[row][col].safe_start = true;

    return board;
}

int main() {

    // intialize game systems

    int num_cells_x = 8;
    int num_cells_y = 8;
    int mine_count = 10;
    bool no_guess = true;
    std::string file_path;

    Board board;

    bool uses_file = not file_path.empty();

    if (uses_file) {
        auto pair = read_board_from_file(file_path);
        board = pair.first;
        mine_count = pair.second;
    } else {
        board = generate_board(mine_count, num_cells_x, num_cells_y);
    }

    if (no_guess) {
        if (uses_file) {
            Solver solver;
            // check if the board is ng solvable
            std::optional<std::pair<int, int>> solution = solver.solve(board, mine_count);
            if (solution.has_value()) {
                std::cout << "file board is ngs" << std::endl;
            } else {
                std::cout << "file board is not ngs" << std::endl;
            }
        } else {
            board = generate_ng_solvable_board(mine_count, num_cells_x, num_cells_y);
        }
    }

    // initialize visuals and sound

    if (!glfwInit())
        return -1;

    GLFWwindow *window =
        initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "cjmines", true, false, false);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);

    auto [vbo_name, cbo_name, ibo_name, vao_name] =
        prepare_drawing_data_and_opengl_drawing_data(num_cells_x, num_cells_y);
    GLuint shader_program = create_shader_program();

    int aspect_ratio_loc = glGetUniformLocation(shader_program, "aspect_ratio");

    std::vector<Rectangle> grid_rectangles =
        generate_grid_rectangles(center, width, height, num_cells_x, num_cells_y, spacing);

    /*auto copied_colors = original_colors;*/

    SoundSystem sound_system;

    sound_system.load_sound_into_system_for_playback("mine", "assets/audio/mine/output_12.mp3");
    sound_system.load_sound_into_system_for_playback("flag", "assets/audio/flag/flag_0.mp3");
    sound_system.load_sound_into_system_for_playback("success", "assets/audio/success.mp3");
    sound_system.load_sound_into_system_for_playback("explosion", "assets/audio/explosion.mp3");

    sound_system.create_sound_source("cell");

    ShaderCache shader_cache({ShaderType::TEXT});
    TextRenderer text_renderer("assets/fonts/cnr.otf", 50, SCREEN_WIDTH, SCREEN_HEIGHT, shader_cache);

    double previous_time = glfwGetTime();
    int frame_count = 0;
    float fps = 0;

    std::vector<glm::vec3> grid_colors;

    bool sucessfully_mined = true;

    const double max_fps = 256.0;
    const double max_frame_time = 1.0 / max_fps;

    double game_start_time = 0;
    bool game_started = false;
    std::vector<double> game_times;
    double total_time = 0.0;
    int games_played = 0;

    // Main game loop
    while (!glfwWindowShouldClose(window) and !user_requested_quit) {
    
        double frame_start_time = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start the game time when the first move is made
        if (!game_started && (lmb_pressed || mine_all_pressed)) {
            game_start_time = glfwGetTime();
            game_started = true;
        }

        if (field_clear(board)) {
            std::cout << "field was clear" << std::endl;
            sound_system.play_sound("cell", "success");

            // Calculate elapsed time and store it
            if (game_started) {
                double game_time = glfwGetTime() - game_start_time;
                game_times.push_back(game_time);
                total_time += game_time;
                games_played++;
            }

            game_started = false; // Reset game start flag for the next game

            // Generate a new board after winning
            if (no_guess) {
                board = generate_ng_solvable_board(mine_count, num_cells_x, num_cells_y);
            } else {
                board = generate_board(mine_count, num_cells_x, num_cells_y);
            }
        }

        if (!sucessfully_mined) {
            sound_system.play_sound("cell", "explosion");
            std::cout << "you died" << std::endl;

            // Calculate elapsed time and store it
            /*if (game_started) {*/
            /*    double game_time = glfwGetTime() - game_start_time;*/
            /*    game_times.push_back(game_time);*/
            /*    total_time += game_time;*/
            /*    games_played++;*/
            /*}*/

            game_started = false; // Reset game start flag for the next game

            // Generate a new board after losing
            if (no_guess) {
                board = generate_ng_solvable_board(mine_count, num_cells_x, num_cells_y);
            } else {
                board = generate_board(mine_count, num_cells_x, num_cells_y);
            }
            sucessfully_mined = true;
        }

        // FPS calculation
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

        if (grid_colors.size() >= 1) {
            glBindBuffer(GL_ARRAY_BUFFER, cbo_name);
            glBufferData(GL_ARRAY_BUFFER, grid_colors.size() * sizeof(glm::vec3), grid_colors.data(), GL_STATIC_DRAW);
        }
        grid_colors.clear();

        glBindVertexArray(vao_name);
        glDrawElements(GL_TRIANGLES, 6 * grid_rectangles.size(), GL_UNSIGNED_INT, 0);

        unsigned int flat_idx = 0;
        for (const auto &row : board) {
            for (const auto &cell : row) {
                Rectangle graphical_rect = grid_rectangles.at(flat_idx);

                if (cell.is_revealed) {
                    text_renderer.render_text(std::to_string(cell.adjacent_mines), graphical_rect.center, 1,
                                              text_color);
                    grid_colors.push_back(mine_count_to_color.at(cell.adjacent_mines));
                } else if (cell.is_flagged) {
                    text_renderer.render_text("F", graphical_rect.center, 1, flag_text_color / 255.0f);
                    grid_colors.push_back(flagged_cell_color);
                } else if (cell.safe_start) {
                    text_renderer.render_text("X", graphical_rect.center, 1, text_color / 255.0f);
                    grid_colors.push_back(ngs_start_pos_color);
                } else {
                    grid_colors.push_back(unrevelead_cell_color);
                }

                if (is_point_in_rectangle(graphical_rect, cursor_pos)) {
                    bool trying_to_mine_all =
                        (lmb_pressed && !lmb_pressed_last_tick) || (mine_all_pressed && !mine_all_pressed_last_tick);

                    bool trying_to_flag_all =
                        (rmb_pressed && !rmb_pressed_last_tick) || (flag_all_pressed && !flag_all_pressed_last_tick);

                    if (trying_to_mine_all) {
                        int row_idx = flat_idx / row.size();
                        int col_idx = flat_idx % row.size();

                        if (!cell.is_revealed) {
                            std::cout << "mining one" << std::endl;
                            sucessfully_mined = reveal_cell(board, row_idx, col_idx);
                        } else {
                            std::cout << "mining all" << std::endl;
                            sucessfully_mined = reveal_adjacent_cells(board, row_idx, col_idx);
                        }
                        sound_system.play_sound("cell", "mine");
                    }

                    if (trying_to_flag_all) {
                        int row_idx = flat_idx / row.size();
                        int col_idx = flat_idx % row.size();
                        if (!cell.is_revealed) {
                            std::cout << "flagging one" << std::endl;
                            toggle_flag_cell(board, row_idx, col_idx);
                        } else {
                            std::cout << "flagging all" << std::endl;
                            flag_adjacent_cells(board, row_idx, col_idx);
                        }
                        sound_system.play_sound("cell", "flag");
                    }
                }
                flat_idx += 1;
            }
        }

        grid_colors = generate_colors_for_indices(grid_colors);
        grid_colors = normalize_rgb_colors(grid_colors);

        // Render FPS
        std::stringstream fps_ss;
        fps_ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
        std::string fps_text = fps_ss.str();
        glm::vec2 fps_dims = text_renderer.get_text_dimensions_in_ndc(fps_text, 1);
        text_renderer.render_text(fps_text, glm::vec2(1, 1) - fps_dims, 1, {0.5, 0.5, 0.5});

        // Render elapsed times and average at the top left of the screen
        if (!game_times.empty()) {
            double average_time = total_time / games_played;

            std::stringstream ss;
            ss << "avg: " << std::fixed << std::setprecision(2) << average_time << "s\n";
            for (size_t i = 0; i < game_times.size(); ++i) {
                ss << "t" << i + 1 << ": " << std::fixed << std::setprecision(2) << game_times[i] << "s\n";
            }

            std::string times_text = ss.str();

            // Split the string by newlines
            std::istringstream stream(times_text);
            std::string line;
            float margin = 0.1;
            glm::vec2 start_pos = glm::vec2(-0.8f, 1.0f); // Top-left corner of the screen
            glm::vec2 current_pos = start_pos;

            int line_count = 0;
            // Loop through each line and render it
            while (std::getline(stream, line)) {
                if (not show_times) {
                    if (line_count >= 1) {
                        break;
                    }
                }
                // Get the dimensions of the current line
                glm::vec2 line_dims = text_renderer.get_text_dimensions_in_ndc(line, 1);

                // Render the current line at the current position
                text_renderer.render_text(line, current_pos - glm::vec2(0.0f, line_dims.y), 1, {1, 0, 0});

                // Move the current position down by the height of the current line
                current_pos.y -= (line_dims.y + margin);
                line_count++;
            }
        }

        lmb_pressed_last_tick = lmb_pressed;
        rmb_pressed_last_tick = rmb_pressed;
        flag_all_pressed_last_tick = flag_all_pressed;
        flag_one_pressed_last_tick = flag_one_pressed;
        mine_all_pressed_last_tick = mine_all_pressed;
        mine_one_pressed_last_tick = mine_one_pressed;

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame limiting
        double frame_end_time = glfwGetTime();
        double frame_duration = frame_end_time - frame_start_time;
        if (frame_duration < max_frame_time) {
            std::this_thread::sleep_for(std::chrono::duration<double>(max_frame_time - frame_duration));
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
