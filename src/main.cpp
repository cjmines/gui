#include "sound_types/sound_types.hpp"
#define STB_IMAGE_IMPLEMENTATION // Define this in exactly one source file
#include <stb_image.h>

#include <glad/glad.h>
#include "shader_cache/shader_cache.hpp"
#include "vertex_geometry/vertex_geometry.hpp"
#include "window/window.hpp"
#include "sound_system/sound_system.hpp"
#include "game_logic/game_logic.hpp"
#include "game_logic/solver.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/ui/ui.hpp"
#include "graphics/colors/colors.hpp"
#include "graphics/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip> // For formatting output
#include <unordered_map>
#include <vector>
#include <glm/vec3.hpp> // Ensure you include the GLM library for glm::vec3

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

const glm::vec3 center(0.0f, 0.0f, 0.0f);
const float width = 2.0f;
const float height = 2.0f;
const float spacing = 0.01f;

const Colors colors;
const std::unordered_map<unsigned int, glm::vec3> mine_count_to_color = {
    {0, colors.grey70},         {1, colors.lightskyblue}, {2, colors.aquamarine3},  {3, colors.pastelred},
    {4, colors.mutedlimegreen}, {5, colors.maroon2},      {6, colors.mutedhotpink}, {7, colors.mustardyellow},
};

const auto unrevelead_cell_color = colors.brown;
const auto flagged_cell_color = colors.brown;
const auto ngs_start_pos_color = colors.limegreen;

const auto text_color = colors.black;
const auto flag_text_color = colors.purple;

bool is_point_in_rectangle(const Rectangle &rect, const glm::vec3 &point) {
    float half_width = rect.width / 2.0f;
    float half_height = rect.height / 2.0f;

    float left_bound = rect.center.x - half_width;
    float right_bound = rect.center.x + half_width;
    float bottom_bound = rect.center.y - half_height;
    float top_bound = rect.center.y + half_height;

    return (point.x >= left_bound && point.x <= right_bound && point.y >= bottom_bound && point.y <= top_bound);
}

std::string key_to_string(int key) {
    static const std::unordered_map<int, std::string> key_map = {
        {GLFW_KEY_A, "a"}, {GLFW_KEY_B, "b"},    {GLFW_KEY_C, "c"}, {GLFW_KEY_D, "d"}, {GLFW_KEY_E, "e"},
        {GLFW_KEY_F, "f"}, {GLFW_KEY_G, "g"},    {GLFW_KEY_H, "h"}, {GLFW_KEY_I, "i"}, {GLFW_KEY_J, "j"},
        {GLFW_KEY_K, "k"}, {GLFW_KEY_L, "l"},    {GLFW_KEY_M, "m"}, {GLFW_KEY_N, "n"}, {GLFW_KEY_O, "o"},
        {GLFW_KEY_P, "p"}, {GLFW_KEY_Q, "q"},    {GLFW_KEY_R, "r"}, {GLFW_KEY_S, "s"}, {GLFW_KEY_T, "t"},
        {GLFW_KEY_U, "u"}, {GLFW_KEY_V, "v"},    {GLFW_KEY_W, "w"}, {GLFW_KEY_X, "x"}, {GLFW_KEY_Y, "y"},
        {GLFW_KEY_Z, "z"}, {GLFW_KEY_0, "0"},    {GLFW_KEY_1, "1"}, {GLFW_KEY_2, "2"}, {GLFW_KEY_3, "3"},
        {GLFW_KEY_4, "4"}, {GLFW_KEY_5, "5"},    {GLFW_KEY_6, "6"}, {GLFW_KEY_7, "7"}, {GLFW_KEY_8, "8"},
        {GLFW_KEY_9, "9"}, {GLFW_KEY_SPACE, " "}
        // Add more keys if needed, like special characters or function keys
    };

    // Find the key in the map and return the corresponding string, or empty if not found
    auto it = key_map.find(key);
    return (it != key_map.end()) ? it->second : "";
}

void process_key_pressed_this_tick(UI &ui, int &key_pressed_this_tick) {
    if (key_pressed_this_tick != GLFW_KEY_UNKNOWN) {
        std::string key_string = key_to_string(key_pressed_this_tick);
        if (!key_string.empty()) {
            ui.process_key_press(key_string);
        }
        if (key_pressed_this_tick == GLFW_KEY_BACKSPACE) {
            ui.process_delete_action();
        }
        key_pressed_this_tick = GLFW_KEY_UNKNOWN; // Clear the key at the end of the tick
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

GLFWcursor *create_custom_cursor(const char *image_path, int hotspot_x, int hotspot_y) {
    // Load image data using stb_image
    int width, height, channels;
    unsigned char *data = stbi_load(image_path, &width, &height, &channels, 4); // 4 = RGBA channels
    if (!data) {
        std::cerr << "Failed to load cursor image: " << image_path << std::endl;
        return nullptr;
    }

    // Create an image for the GLFW cursor
    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = data;

    // Create the cursor with the given hotspot
    GLFWcursor *cursor = glfwCreateCursor(&image, hotspot_x, hotspot_y);

    // Free image data after cursor creation
    stbi_image_free(data);

    return cursor;
}

enum GameState { MAIN_MENU, OPTIONS_PAGE, IN_GAME, END_GAME };

UI create_main_menu(GLFWwindow *window, FontAtlas &font_atlas, GameState &curr_state) {
    UI main_menu_ui(font_atlas);

    std::function<void()> on_play = [&]() { curr_state = OPTIONS_PAGE; };
    std::function<void()> on_quit = [&]() { glfwSetWindowShouldClose(window, GLFW_TRUE); };

    main_menu_ui.add_textbox("Welcome to CJMines", 0, 0.75, 1, 0.25, colors.grey);
    main_menu_ui.add_clickable_textbox(on_play, "Play", 0.65, -0.65, 0.5, 0.5, colors.darkgreen, colors.green);
    main_menu_ui.add_clickable_textbox(on_quit, "Quit", -0.65, -0.65, 0.5, 0.5, colors.darkred, colors.red);

    return main_menu_ui;
}

UI create_options_page(FontAtlas &font_atlas, GameState &curr_state, Board &board, float &mine_percentage,
                       int &num_cells_x, int &num_cells_y, int &mine_count, std::vector<Rectangle> &grid_rectangles,
                       int &games_threshold) {
    UI in_game_ui(font_atlas);

    std::function<void(std::string)> on_width_confirm = [&](std::string contents) {
        if (contents.empty())
            return;
        std::cout << contents.size() << std::endl;
        num_cells_x = std::stoi(contents);
        mine_count = num_cells_x * num_cells_y * mine_percentage;
    };
    std::function<void(std::string)> on_height_confirm = [&](std::string contents) {
        if (contents.empty())
            return;
        num_cells_y = std::stoi(contents);
        mine_count = num_cells_x * num_cells_y * mine_percentage;
    };
    std::function<void(std::string)> on_mine_count_confirm = [&](std::string contents) {
        if (contents.empty())
            return;
        mine_count = std::stoi(contents);
        mine_percentage = static_cast<float>(mine_count) / static_cast<float>(num_cells_x * num_cells_y);
    };
    std::function<void(std::string)> on_game_count_confirm = [&](std::string contents) {
        if (contents.empty())
            return;
        games_threshold = std::stoi(contents);
    };
    std::function<void()> on_back = [&]() { curr_state = MAIN_MENU; };
    std::function<void()> on_play = [&]() {
        // TODO: NGS config
        board = generate_ng_solvable_board(mine_count, num_cells_x, num_cells_y);
        grid_rectangles = generate_grid_rectangles(center, width, height, num_cells_x, num_cells_y, spacing);
        curr_state = IN_GAME;
    };

    in_game_ui.add_input_box(on_width_confirm, "Board Width", 0, 0.25, 1, 0.25, colors.grey, colors.lightgrey);
    in_game_ui.add_input_box(on_height_confirm, "Board Height", 0, 0.0, 1, 0.25, colors.grey, colors.lightgrey);
    in_game_ui.add_input_box(on_mine_count_confirm, "Number of Mines", 0, -0.25, 1, 0.25, colors.grey,
                             colors.lightgrey);
    in_game_ui.add_input_box(on_game_count_confirm, "Number of Games", 0, -0.5, 1, 0.25, colors.grey, colors.lightgrey);
    in_game_ui.add_clickable_textbox(on_play, "Start Game", 0.65, -0.65, 0.5, 0.5, colors.seagreen, colors.grey);
    in_game_ui.add_clickable_textbox(on_back, "Back to Main Menu", -0.65, -0.65, 0.5, 0.5, colors.seagreen,
                                     colors.grey);

    return in_game_ui;
}

UI create_ending_page(GLFWwindow *window, FontAtlas &font_atlas, GameState &curr_state, double avg_time) {
    UI end_ui(font_atlas);

    std::function<void()> on_play = [&]() { curr_state = OPTIONS_PAGE; };
    std::function<void()> on_quit = [&]() { glfwSetWindowShouldClose(window, GLFW_TRUE); };

    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << avg_time;

    end_ui.add_textbox("Game End", 0, 0.75, 1, 0.25, colors.grey);
    end_ui.add_textbox("Average game time: " + stream.str() + " seconds", 0, 0.50, 0.6, 0.25, colors.yellow);
    end_ui.add_clickable_textbox(on_play, "Replay", 0.65, -0.65, 0.5, 0.5, colors.darkgreen, colors.green);
    end_ui.add_clickable_textbox(on_quit, "Quit", -0.65, -0.65, 0.5, 0.5, colors.darkred, colors.red);

    return end_ui;
}

SoundType get_random_mine_sound() {
    static std::vector<SoundType> mine_sounds = {SoundType::MINE_0, SoundType::MINE_1, SoundType::MINE_2,
                                                 SoundType::MINE_3, SoundType::MINE_4, SoundType::MINE_5,
                                                 SoundType::MINE_6};

    // random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, mine_sounds.size() - 1);

    return mine_sounds[dist(gen)];
}

SoundType get_random_flag_sound() {
    static std::vector<SoundType> flag_sounds = {SoundType::FLAG_0, SoundType::FLAG_1};

    // random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, flag_sounds.size() - 1);

    return flag_sounds[dist(gen)];
}

int main() {
    float mine_percentage = 0.01;
    int num_cells_x = 10;
    int num_cells_y = 10;
    int mine_count = num_cells_x * num_cells_y * mine_percentage;
    bool no_guess = true;
    bool play_field_from_path = false;
    int games_played = 0;
    int games_threshold = 1;
    /*std::string file_path = "assets/minefields/checkerboard.png"; // Change this to your desired path*/
    std::string file_path = "";

    Board board;

    bool uses_file = !file_path.empty();

    if (uses_file) {
        // Check the file extension
        std::string extension = file_path.substr(file_path.find_last_of('.') + 1);

        if (extension == "txt") {
            auto pair = read_board_from_file(file_path);
            board = pair.first;
            mine_count = pair.second;
        } else if (extension == "png") {
            auto pair = read_board_from_image_file(file_path);
            board = pair.first;
            mine_count = pair.second;
        } else {
            std::cerr << "Unsupported file format: " << extension << std::endl;
        }
    } else {
        board = generate_board(mine_count, num_cells_x, num_cells_y);
    }

    num_cells_y = board.size();
    num_cells_x = board[0].size();

    if (no_guess) {
        if (uses_file) {
            Solver solver;
            // check if the board is ng solvable
            std::optional<std::pair<int, int>> solution = solver.solve(board, mine_count);
            if (solution.has_value()) {
                std::cout << "file board is ngs" << std::endl;
                auto safe_row = solution.value().first;
                auto safe_col = solution.value().second;
                board[safe_row][safe_row].safe_start = true;
            } else {
                std::cout << "file board is not ngs" << std::endl;
            }
            if (not play_field_from_path) {
                return 0;
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

    GLFWcursor *custom_cursor = create_custom_cursor("assets/crosshair/cross_64.png", 32, 32);
    if (custom_cursor) {
        glfwSetCursor(window, custom_cursor);
    }

    std::vector<ShaderType> requested_shaders = {ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX,
                                                 ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT};
    ShaderCache shader_cache(requested_shaders);
    Batcher batcher(shader_cache);

    std::vector<Rectangle> grid_rectangles;

    /*auto copied_colors = original_colors;*/

    FontAtlas font_atlas("assets/fonts/times_64_sdf_atlas_font_info.json", "assets/fonts/times_64_sdf_atlas.json",
                         "assets/fonts/times_64_sdf_atlas.png", SCREEN_WIDTH, false, true);

    GameState curr_state = MAIN_MENU;
    std::unordered_map<GameState, UI> game_state_to_ui = {
        {MAIN_MENU, create_main_menu(window, font_atlas, curr_state)},
        {OPTIONS_PAGE, create_options_page(font_atlas, curr_state, board, mine_percentage, num_cells_x, num_cells_y,
                                           mine_count, grid_rectangles, games_threshold)}};

    std::function<void(unsigned int)> char_callback = [&](unsigned int codepoint) {};

    bool user_requested_quit = false;

    int key_pressed_this_tick = GLFW_KEY_UNKNOWN;

    bool left_shift_pressed = false;

    bool flag_all_pressed = false;
    bool flag_all_pressed_last_tick = false;

    bool unflag_all_pressed = false;
    bool unflag_all_pressed_last_tick = false;

    bool flag_one_pressed = false;
    bool flag_one_pressed_last_tick = false;

    bool mine_all_pressed = false;
    bool mine_all_pressed_last_tick = false;

    bool mine_one_pressed = false;
    bool mine_one_pressed_last_tick = false;

    bool show_times = false;

    std::function<void(int, int, int, int)> key_callback = [&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_LEFT_SHIFT) {
            if (action == GLFW_PRESS) {
                left_shift_pressed = true;
            }
            if (action == GLFW_RELEASE) {
                left_shift_pressed = false;
            }
        }

        if (key == GLFW_KEY_R) {
            if (action == GLFW_PRESS && left_shift_pressed) {
                unflag_all_pressed = true;
            }
            if (action == GLFW_RELEASE) {
                unflag_all_pressed = false;
            }
        }

        if (key == GLFW_KEY_F) {
            if (action == GLFW_PRESS) {
                if (left_shift_pressed) {
                    flag_all_pressed = true;
                } else {
                    flag_one_pressed = true;
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
        if (key == GLFW_KEY_Q) {
            if (action == GLFW_PRESS) {
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

        if (action == GLFW_PRESS) {
            key_pressed_this_tick = key; // Store the key pressed this tick
        }

        // process input text boxes
        if (game_state_to_ui.count(curr_state))
            game_state_to_ui.at(curr_state).process_confirm_action();
    };

    double mouse_x, mouse_y;

    std::function<void(double, double)> mouse_callback = [&](double xpos, double ypos) {
        mouse_x = xpos;
        mouse_y = ypos;
    };

    bool lmb_pressed = false;
    bool lmb_pressed_last_tick = false;
    bool rmb_pressed = false;
    bool rmb_pressed_last_tick = false;

    std::function<void(int, int, int)> mouse_button_callback = [&](int button, int action, int mods) {
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
    };

    GLFWLambdaCallbackManager window_callback_manager(window, char_callback, key_callback, mouse_callback,
                                                      mouse_button_callback);

    std::unordered_map<SoundType, std::string> sound_type_to_file = {
        {SoundType::FLAG_0, "assets/audio/flag/flag_0.mp3"}, {SoundType::FLAG_1, "assets/audio/flag/flag_1.mp3"},
        {SoundType::MINE_0, "assets/audio/mine/mine_0.mp3"}, {SoundType::MINE_1, "assets/audio/mine/mine_1.mp3"},
        {SoundType::MINE_2, "assets/audio/mine/mine_2.mp3"}, {SoundType::MINE_3, "assets/audio/mine/mine_3.mp3"},
        {SoundType::MINE_4, "assets/audio/mine/mine_4.mp3"}, {SoundType::MINE_5, "assets/audio/mine/mine_5.mp3"},
        {SoundType::MINE_6, "assets/audio/mine/mine_6.mp3"}, {SoundType::SUCCESS, "assets/audio/success.mp3"},
        {SoundType::EXPLOSION, "assets/audio/explosion.mp3"}};

    int max_concurrent_sounds = 100;

    SoundSystem sound_system(max_concurrent_sounds, sound_type_to_file);

    /*sound_system.load_sound_into_system_for_playback("mine", "assets/audio/mine/output_12.mp3");*/
    /*sound_system.load_sound_into_system_for_playback("flag", "assets/audio/flag/flag_0.mp3");*/
    /*sound_system.load_sound_into_system_for_playback("success", "assets/audio/success.mp3");*/
    /*sound_system.load_sound_into_system_for_playback("explosion", "assets/audio/explosion.mp3");*/

    /*sound_system.create_sound_source("cell");*/

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::mat4(1);
    auto text_color = glm::vec3(0.0, 0.0, 0.0);
    float char_width = 0.5;
    float edge_transition = 0.1;

    shader_cache.use_shader_program(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT);
    shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT, ShaderUniformVariable::TRANSFORM,
                             projection);

    shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT, ShaderUniformVariable::RGB_COLOR,
                             text_color);

    shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                             ShaderUniformVariable::CHARACTER_WIDTH, char_width);

    shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                             ShaderUniformVariable::EDGE_TRANSITION_WIDTH, edge_transition);

    double previous_time = glfwGetTime();
    int frame_count = 0;
    float fps = 0;

    bool sucessfully_mined = true;

    const double max_fps = 60.0;
    const double max_frame_time = 1.0 / max_fps;

    double game_start_time = 0;
    bool game_started = false;
    std::vector<double> game_times;
    double total_time = 0.0;

    // Main game loop
    while (!glfwWindowShouldClose(window) and !user_requested_quit) {

        double frame_start_time = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (curr_state != IN_GAME) {
            auto ndc_mouse_pos = convert_mouse_to_ndc(mouse_x, mouse_y, SCREEN_WIDTH, SCREEN_HEIGHT);
            auto &curr_ui = game_state_to_ui.at(curr_state);

            glm::vec2 ndc_mouse_pos_vec(ndc_mouse_pos.first, ndc_mouse_pos.second);
            curr_ui.process_mouse_position(ndc_mouse_pos_vec);

            if (lmb_pressed && !lmb_pressed_last_tick) {
                curr_ui.process_mouse_just_clicked(ndc_mouse_pos_vec);
            }

            process_key_pressed_this_tick(curr_ui, key_pressed_this_tick);

            for (auto &tb : curr_ui.get_text_boxes()) {
                batcher.transform_v_with_signed_distance_field_text_shader_batcher.queue_draw(
                    tb.text_drawing_data.indices, tb.text_drawing_data.xyz_positions,
                    tb.text_drawing_data.texture_coordinates);
                batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(
                    tb.background_ivpsc.indices, tb.background_ivpsc.xyz_positions, tb.background_ivpsc.rgb_colors);
            }

            for (auto &cr : curr_ui.get_clickable_text_boxes()) {
                batcher.transform_v_with_signed_distance_field_text_shader_batcher.queue_draw(
                    cr.text_drawing_data.indices, cr.text_drawing_data.xyz_positions,
                    cr.text_drawing_data.texture_coordinates);
                batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(
                    cr.ivpsc.indices, cr.ivpsc.xyz_positions, cr.ivpsc.rgb_colors);
            }

            for (auto &ib : curr_ui.get_input_boxes()) {
                batcher.transform_v_with_signed_distance_field_text_shader_batcher.queue_draw(
                    ib.text_drawing_data.indices, ib.text_drawing_data.xyz_positions,
                    ib.text_drawing_data.texture_coordinates);
                batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(
                    ib.background_ivpsc.indices, ib.background_ivpsc.xyz_positions, ib.background_ivpsc.rgb_colors);
            }

            batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();
            batcher.transform_v_with_signed_distance_field_text_shader_batcher.draw_everything();

        } else {
            // clang-format off
        // Start the game time when the first move is made
        if (!game_started && (lmb_pressed || mine_all_pressed)) {
            game_start_time = glfwGetTime();
            game_started = true;
        }

        if (field_clear(board)) {
            game_started = false;
            std::cout << "field was clear" << std::endl;
                sound_system.queue_sound(SoundType::SUCCESS, center);

            // Calculate elapsed time and store it
            double game_time = glfwGetTime() - game_start_time;
            game_times.push_back(game_time);
            total_time += game_time;
            games_played++;

            if (games_played >= games_threshold) {
                games_played = 0;

                float avg_time = 0;
                for (auto& i : game_times) {
                    std::cout << "Game Times " << std::to_string(i) << "\n";
                    avg_time += i;
                }
                avg_time /= games_threshold;

                std::cout << avg_time << "\n";
                game_times.clear();
                game_state_to_ui.insert_or_assign(END_GAME, create_ending_page(window, font_atlas, curr_state, avg_time));
                curr_state = END_GAME;
                game_started = false;
                continue;
            }

            // Generate a new board after winning
            if (no_guess) {
                board = generate_ng_solvable_board(mine_count, num_cells_x, num_cells_y);
            } else {
                board = generate_board(mine_count, num_cells_x, num_cells_y);
            }
        }

        if (!sucessfully_mined) {
                sound_system.queue_sound(SoundType::EXPLOSION, center);
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

        shader_cache.use_shader_program(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX);

        unsigned int flat_idx = 0;
        for (const auto &row : board) {
            for (const auto &cell : row) {
                Rectangle graphical_rect = grid_rectangles.at(flat_idx);

                std::vector<glm::vec3> rectangle_vertices = generate_rectangle_vertices(
                    graphical_rect.center.x, graphical_rect.center.y, graphical_rect.width, graphical_rect.height);
                std::vector<unsigned int> rectangle_indices = generate_rectangle_indices();

                std::string text;
                glm::vec3 rectangle_color;
                if (cell.is_revealed) {
                    text = std::to_string(cell.adjacent_mines);
                    rectangle_color = mine_count_to_color.at(cell.adjacent_mines);
                } else if (cell.is_flagged) {
                    text = "F";
                    rectangle_color = flagged_cell_color;
                } else if (cell.safe_start) {
                    text = "X";
                    rectangle_color = ngs_start_pos_color;
                } else {
                    text = "";
                    rectangle_color = unrevelead_cell_color;
                }

                if (text != "" && text != "0") {
                    TextMesh text_mesh = font_atlas.generate_text_mesh_size_constraints(text, graphical_rect.center.x, graphical_rect.center.y, graphical_rect.width * 0.5, graphical_rect.height * 0.5);
                    batcher.transform_v_with_signed_distance_field_text_shader_batcher.queue_draw(text_mesh.indices, text_mesh.vertex_positions, text_mesh.texture_coordinates);
                }

                std::vector<glm::vec3> rectangle_colors = generate_colors_for_indices({rectangle_color});
                batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(
                    rectangle_indices, rectangle_vertices, rectangle_colors);

                if (is_point_in_rectangle(graphical_rect, cursor_pos)) {
                    bool trying_to_mine_all =
                        (lmb_pressed && !lmb_pressed_last_tick) || (mine_all_pressed && !mine_all_pressed_last_tick);

                    bool trying_to_flag_all =
                        (rmb_pressed && !rmb_pressed_last_tick) || (flag_all_pressed && !flag_all_pressed_last_tick);

                    bool trying_to_unflag_all = unflag_all_pressed && !unflag_all_pressed_last_tick;

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
                            // todo use positional sound based on row and col idx later
                        sound_system.queue_sound(get_random_mine_sound(), center);
                    }

                    if (trying_to_flag_all) {
                        int row_idx = flat_idx / row.size();
                        int col_idx = flat_idx % row.size();
                        if (!cell.is_revealed) {
                            std::cout << "flagging one" << std::endl;
                            toggle_flag_cell(board, row_idx, col_idx);
                        } else {
                            std::cout << "flagging all" << std::endl;
                            set_adjacent_cells_flags(board, row_idx, col_idx, true);
                        }
                        sound_system.queue_sound(get_random_flag_sound(), center);
                    }

                    if (trying_to_unflag_all) {
                        int row_idx = flat_idx / row.size();
                        int col_idx = flat_idx % row.size();
                        std::cout << "unflagging all" << std::endl;
                        set_adjacent_cells_flags(board, row_idx, col_idx, false);
                        // sound_system.play_sound("cell", "unflag");
                    }
                }
                flat_idx += 1;
            }
        }

        // Render FPS
        std::stringstream fps_ss;
        fps_ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
        std::string fps_text = fps_ss.str();
        TextMesh fps_text_mesh = font_atlas.generate_text_mesh_size_constraints(fps_text, 0.9, 0.9, 0.15, 0.15);
        batcher.transform_v_with_signed_distance_field_text_shader_batcher.queue_draw(fps_text_mesh.indices, fps_text_mesh.vertex_positions, fps_text_mesh.texture_coordinates);

        batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();
        batcher.transform_v_with_signed_distance_field_text_shader_batcher.draw_everything();

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

            // int line_count = 0;
            // // Loop through each line and render it
            // while (std::getline(stream, line)) {
            //     if (not show_times) {
            //         if (line_count >= 1) {
            //             break;
            //         }
            //     }
            //     // Get the dimensions of the current line
            //     glm::vec2 line_dims = text_renderer.get_text_dimensions_in_ndc(line, 1);

            //     // Render the current line at the current position
            //     text_renderer.render_text(line, current_pos - glm::vec2(0.0f, line_dims.y), 1, {1, 0, 0});

            //     // Move the current position down by the height of the current line
            //     current_pos.y -= (line_dims.y + margin);
            //     line_count++;
            // }
                // clang-format on
            }
        }

        lmb_pressed_last_tick = lmb_pressed;
        rmb_pressed_last_tick = rmb_pressed;
        flag_all_pressed_last_tick = flag_all_pressed;
        flag_one_pressed_last_tick = flag_one_pressed;
        mine_all_pressed_last_tick = mine_all_pressed;
        mine_one_pressed_last_tick = mine_one_pressed;

        sound_system.play_all_sounds();

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame limiting
        double frame_end_time = glfwGetTime();
        double frame_duration = frame_end_time - frame_start_time;
        if (frame_duration < max_frame_time) {
            // std::this_thread::sleep_for(std::chrono::duration<double>(max_frame_time - frame_duration));
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
