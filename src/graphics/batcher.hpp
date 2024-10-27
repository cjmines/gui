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
 * queue_draw(id, ...)
 * Map id to vertices in CPU cache
 * For every draw call compare cache with new inputted vertices to see if things changed
 * If yes, then change only those vertices (glBufferSubData)
 * If no, then keep those vertices as is
 * 
 * Newly created vertices should be appended to the list (but this requires either over-allocation at initialization,
 * i.e a bigger buffer than necessary or rebuffering)
 * What about deleted vertices? Problem: they create holes
 * You can use glBufferSubData to shift subsequent elements? Like deleting an element from the middle of a 
 * Python list.
 * 
 * 
 * From Khronos
 * Rendering with a different VAO from the last drawing command is usually a relatively expensive operation.
 * 
 * Shader {
 *   VAO {
 *      VBO-1
 *        VBO-1 attrib-1 enabled/disabled, pointers
 *        VBO-1 attrib-2 enabled/disabled, pointers
 *        ...
 *      VBO-2
 *        ...
 *      VBO-3
 *        ...
 *      (VAO only remembers VBOs if attributes are set up)
 *      ...
 *      EBO (just one; NOT part of global state)
 *   }
 * }
 * 
 * Instance rendering is for when geometry is identical but per-instance attributes vary.
 * Single VBO + per-instance buffers.
 * 
 * Multidraw is for when objects are different but share similar state.
 * Multiple VBOs
 * 
 * Attribute vs. Uniform?
 * Uniforms are shared between all vertices/fragments. Attributes are INTERPOLATED between fragments.
 */

// every shader needs batching unique to it (should every shader implement its own batching?)
// parse shader to programmatically get attribs and their types while also generating code that batches it
// is the code that batches it possible to generalize?
// ShaderVertexAttributeVariable -> vector<ShaderVertexAttributeVariable>
// vec3attribs = unordered_map<ShaderVertexAttributeVariableEnum, vector<glm::vec3>>
// uiattribs = unordered_map<ShaderVertexAttributeVariableEnum, vector<unsigned int>>
// 
// {ShaderVertexAttributeVariable::POSITION, ShaderVertexAttributeVariable::PASSTHROUGH_RGB_COLOR}
// for ([attrib, data] : vec3attribs) {
//    
// }
// 


struct DrawInfoPerShader {
    GLuint VAO;
    GLuint VBO;
    GLuint CBO;
    GLuint IBO;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> colors;
    std::vector<unsigned int> indices;
    // ShaderVertexAttributeVariable -> vector<ShaderVertexAttributeVariable>
};

// approach 1: 
// each shader implements its own batching
// Shader 
// .draw(..., ..., ...) <-- batching (different args per shader)
// 
// approach 2:
// have all vertices, colors, indices, etc. in function signature of queue_draw but use defaults
// potentially enable/disable vertex attributes based on which ones the current shader actually needs
//
// approach 3:
// lambda (?) with unique signature per shader
// like approach 1 without classes
// shader type -> shader batcher class with function called queue_draw (@override) + relevant info
// master batcher contains shader type <-> shader batcher

// one problem: we need different signatures based on the shader
// we don't want to make new functions per shader

// https://www.reddit.com/r/opengl/comments/xtsqoa/optimizing_draw_calls/
// https://www.reddit.com/r/GraphicsProgramming/comments/qvh62l/comment/hkx35i6/


/**
 * approach 1
 * queueDraw = MasterBatcher.shaderBatchers.at(ShaderType::...);
 * queueDraw(..., ..., ...);
 */






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

            // loop?
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
