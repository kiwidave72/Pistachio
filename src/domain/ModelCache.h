#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <regex>
#include <sstream>
#include <fstream>

 

#include <nlohmann/json.hpp>  
 
#include <glm/glm.hpp>

namespace glm {
    inline void to_json(nlohmann::json& j, const vec3& v) {
        j = nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
    }
    inline void from_json(const nlohmann::json& j, vec3& v) {
        j.at("x").get_to(v.x);
        j.at("y").get_to(v.y);
        j.at("z").get_to(v.z);
    }
}


namespace domain::v1{

    class Vertex
    {
    public:
        glm::vec3 position;
        glm::vec3 normal;
    };

    //refactor to Vertex
    struct RawTriangle
    {
        glm::vec3 normal;
        glm::vec3 v[3];
    };

    struct TriangleView
    {
        const Vertex* v1;
        const Vertex* v2;
        const Vertex* v3;
    };


    class BoundingBox
    {
    public:
        glm::vec3 min;
        glm::vec3 max;

        glm::vec3 size() const
        {
            return {
                max.x - min.x,
                max.y - min.y,
                max.z - min.z
            };
        }

        glm::vec3 center() const
        {
            return {
                (min.x + max.x) * 0.5f,
                (min.y + max.y) * 0.5f,
                (min.z + max.z) * 0.5f
            };
        }
    };

    class Mesh
    {
    public:

        int Id = 0;

        std::string name;
        std::string fileName;
        std::string fileLocation;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        BoundingBox bounds;

        glm::vec3 center;

        float cameradistance = 0.0f;
        float radius = 0.0f;

        TriangleView Mesh::triangle(size_t index) const
        {
            size_t i = index * 3;

            return
            {
                &vertices[indices[i]],
                &vertices[indices[i + 1]],
                &vertices[indices[i + 2]]
            };
        }
    };

   

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


    class Model{
        
    public:
        std::string Id;
        std::string name;
        std::string label;
        std::string fileName;
        std::string fileLocation;
        std::shared_ptr<Mesh> mesh;
       
    };

    // 1. Convert Model instance to JSON
    inline void to_json(nlohmann::json& j, const Model& m) {
        j["Id"] = m.Id;
        j["name"] = m.name;
        j["label"] = m.label;
        j["fileName"] = m.fileName;
        j["fileLocation"] = m.fileLocation;
          
    }

    // 2. Convert JSON back into a Model instance
    inline void from_json(const nlohmann::json& j, Model& m) {
  
        // Deserialize primitive and string types safely
        if (j.contains("Id")) j["Id"].get_to(m.Id);
        if (j.contains("name")) j["name"].get_to(m.name);
        if (j.contains("label")) j["label"].get_to(m.label);
        if (j.contains("fileName")) j["fileName"].get_to(m.fileName);
        if (j.contains("fileLocation")) j["fileLocation"].get_to(m.fileLocation);
          
    }



    class ImportedAsset
    {
    public:
        std::string         Id ;
        std::string name;          // clean display name (no xN/[A])
        std::string label;         // original filename stem
        std::string fileName;      // filename with extension
        std::string fileLocation;
        std::string description;   // e.g. "Print x3 | Accent colour [A]"
        int         quantity = 1;
        bool        isAccent = false;
    };


    class ModelCache
    {
    public:
        std::unordered_map<std::string, ImportedAsset*> assets;
        std::unordered_map<std::string, std::shared_ptr<Model>> models;
        std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;

        // Models
        std::shared_ptr<Model> getModel(const std::string& hash) const;
  
   

    };
    // ModelCache.cpp

 
   
}