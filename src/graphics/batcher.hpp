#ifndef BATCHER_HPP
#define BATCHER_HPP

#include <vector>
#include <unordered_map>
#include <glm/vec3.hpp>

#include <iostream>
#include "sbpt_generated_includes.hpp"

/**
 * Always buffering data is wasteful for static vertices.
 * E.g., Initially we draw a grid by buffering the vertex positions once at the start of the program
 * but now we have to do that every frame.
 *
 * glBufferSubData
 *
 * In the context of a static object, we are afraid buffering that object's vertices every single frame
 * would be worse than buffering it once. One solution is by using IDs for each object to determine
 * if it has been drawn in the previous frame. If it has, we would come up with a way to make sure we
 * don't have to buffer that data again. We decided not to work on this atm because this will be a
 * complex solution with a lot of changes necessary.
 *
 *
 *
 */

struct DrawInfoPerShader {
    GLuint VAO;
    GLuint VBO;
    GLuint CBO;
    GLuint IBO;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> colors;
    std::vector<unsigned int> indices;
};

class Batcher {
  public:
    Batcher(std::vector<ShaderType> requested_shaders, ShaderCache &shader_cache) : shader_cache{shader_cache} {
        for (const auto &requested_shader : requested_shaders) {
            DrawInfoPerShader draw_info{};
            shader_type_to_draw_info_this_tick[requested_shader] = draw_info;

            glGenVertexArrays(1, &shader_type_to_draw_info_this_tick[requested_shader].VAO);

            // TODO: generalize later
            glGenBuffers(1, &shader_type_to_draw_info_this_tick[requested_shader].VBO); // For vertices
            glGenBuffers(1, &shader_type_to_draw_info_this_tick[requested_shader].CBO); // For colors
            glGenBuffers(1, &shader_type_to_draw_info_this_tick[requested_shader].IBO); // For indices

            glBindVertexArray(shader_type_to_draw_info_this_tick[requested_shader].VAO);

            glBindBuffer(GL_ARRAY_BUFFER, shader_type_to_draw_info_this_tick[requested_shader].VBO);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, shader_type_to_draw_info_this_tick[requested_shader].CBO);

            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(1);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader_type_to_draw_info_this_tick[requested_shader].IBO);

            glBindVertexArray(0);
        }
    }

    void queue_draw(std::vector<glm::vec3> &vertices, std::vector<glm::vec3> &colors,
                    std::vector<unsigned int> &indices, ShaderType type) {
        if (shader_type_to_draw_info_this_tick.find(type) == shader_type_to_draw_info_this_tick.end()) {
            throw std::runtime_error("ShaderType not requested upon initialization!");
        }

        shader_type_to_draw_info_this_tick[type].vertices.insert(
            shader_type_to_draw_info_this_tick[type].vertices.end(), vertices.begin(), vertices.end());

        shader_type_to_draw_info_this_tick[type].colors.insert(shader_type_to_draw_info_this_tick[type].colors.end(),
                                                               colors.begin(), colors.end());

        std::vector<std::vector<unsigned int>> all_indices = {shader_type_to_draw_info_this_tick[type].indices,
                                                              indices};
        shader_type_to_draw_info_this_tick[type].indices = flatten_and_increment_indices(all_indices);
    }

    void draw_everything() {
        for (const auto &[type, draw_info] : shader_type_to_draw_info_this_tick) {
            // Print the ShaderType (key)
            // std::cout << "ShaderType: " << shader_type_to_string(type) << std::endl;
            shader_cache.use_shader_program(type);

            glBindVertexArray(draw_info.VAO);

            glBindBuffer(GL_ARRAY_BUFFER, draw_info.VBO);
            glBufferData(GL_ARRAY_BUFFER, draw_info.vertices.size() * sizeof(glm::vec3), draw_info.vertices.data(),
                         GL_STATIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, draw_info.CBO);
            glBufferData(GL_ARRAY_BUFFER, draw_info.colors.size() * sizeof(glm::vec3), draw_info.colors.data(),
                         GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_info.IBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, draw_info.indices.size() * sizeof(unsigned int),
                         draw_info.indices.data(), GL_STATIC_DRAW);

            glDrawElements(GL_TRIANGLES, draw_info.indices.size(), GL_UNSIGNED_INT, 0);

            glBindVertexArray(0);

            shader_cache.stop_using_shader_program();

            // // Print the vector of glm::vec3 (first element of the pair)
            // std::cout << "glm::vec3 values:" << std::endl;
            // for (const auto &vec : pair.second.first) {
            //     std::cout << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")" << std::endl;
            // }

            // // Print the vector of unsigned int (second element of the pair)
            // std::cout << "unsigned int values:" << std::endl;
            // for (unsigned int index : pair.second.second) {
            //     std::cout << index << " ";
            // }
            // std::cout << std::endl;
        }

        for (auto &[type, draw_info] : shader_type_to_draw_info_this_tick) {
            draw_info.vertices.clear();
            draw_info.colors.clear();
            draw_info.indices.clear();
        }
    };

  private:
    std::unordered_map<ShaderType, DrawInfoPerShader> shader_type_to_draw_info_this_tick;
    ShaderCache shader_cache;
};

#endif
