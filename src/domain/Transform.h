// domain/Transform.h
#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <regex>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>  
#include "domain/GlmJson.h"

#include <glm/glm.hpp>

struct Transform
{
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    glm::mat4 matrix() const
    {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);

        glm::mat4 r = glm::mat4(1.0f);
        r = glm::rotate(r, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        r = glm::rotate(r, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        r = glm::rotate(r, glm::radians(rotation.z), glm::vec3(0, 0, 1));

        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

        return t * r * s;
    }

    void translate(const glm::vec3& delta)
    {
        position += delta;
    }

    void rotate(const glm::vec3& delta)
    {
        rotation += delta;
    }

    void setScale(const glm::vec3& value)
    {
        scale = value;
    }

    void reset()
    {
        position = { 0.0f, 0.0f, 0.0f };
        rotation = { 0.0f, 0.0f, 0.0f };
        scale = { 1.0f, 1.0f, 1.0f };
    }
};
inline void to_json(nlohmann::json& j, const Transform& t)
{
    j =
    {
        {"position", t.position},
        {"rotation", t.rotation},
        {"scale", t.scale}
    };
}

inline void from_json(const nlohmann::json& j, Transform& t)
{
    if (j.contains("position")) j.at("position").get_to(t.position);
    if (j.contains("rotation")) j.at("rotation").get_to(t.rotation);
    if (j.contains("scale")) j.at("scale").get_to(t.scale);
}
