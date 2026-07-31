#include "StlLoaderAdapter.h"

StlLoaderAdapter::StlLoaderAdapter(ModelCache& cache)
    : m_cache(cache)
{
}


bool StlLoaderAdapter::loadBinaryStl(const std::string& path, std::vector<RawTriangle>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char header[80];
    f.read(header, 80);
    if (!f) return false;

    uint32_t triCount = 0;
    f.read(reinterpret_cast<char*>(&triCount), 4);
    if (!f || triCount == 0) return false;

    out.reserve(triCount);
    for (uint32_t i = 0; i < triCount; ++i)
    {
        RawTriangle t;
        float buf[12];
        f.read(reinterpret_cast<char*>(buf), 48);
        uint16_t attr;
        f.read(reinterpret_cast<char*>(&attr), 2);
        if (!f) break;

        t.normal = { buf[0], buf[1], buf[2] };
        t.v[0] = { buf[3], buf[4], buf[5] };
        t.v[1] = { buf[6], buf[7], buf[8] };
        t.v[2] = { buf[9], buf[10], buf[11] };
        out.push_back(t);
    }
    return !out.empty();
}

bool StlLoaderAdapter::isAsciiStl(const std::string& path)
{
    std::ifstream f(path);
    std::string word;
    f >> word;
    return (word == "solid");
}

bool StlLoaderAdapter::loadAsciiStl(const std::string& path, std::vector<RawTriangle>& out)
{
    std::ifstream f(path);
    if (!f) return false;

    std::string tok;
    RawTriangle tri{};
    int vertIdx = 0;
    bool inFacet = false;

    while (f >> tok)
    {
        if (tok == "facet")
        {
            std::string norm; float nx, ny, nz;
            f >> norm >> nx >> ny >> nz;
            tri.normal = { nx,ny,nz };
            vertIdx = 0;
            inFacet = true;
        }
        else if (tok == "vertex" && inFacet && vertIdx < 3)
        {
            float x, y, z;
            f >> x >> y >> z;
            tri.v[vertIdx++] = { x,y,z };
        }
        else if (tok == "endfacet" && inFacet)
        {
            if (vertIdx == 3) out.push_back(tri);
            inFacet = false;
        }
    }
    return !out.empty();
}



std::vector<RawTriangle> StlLoaderAdapter::Triangle() {
    return tris;
}

std::shared_ptr<Mesh> StlLoaderAdapter::getMesh() { return m_mesh; }

bool StlLoaderAdapter::load(const std::string& filePath)
{
    auto result = m_cache.meshes.find(filePath);

    if (result != m_cache.meshes.end())
    {
        m_mesh = result->second;
        return true;
    }


    std::vector<RawTriangle> rawTris;
    bool ok = isAsciiStl(filePath)
        ? loadAsciiStl(filePath, rawTris)
        : loadBinaryStl(filePath, rawTris);

    if (!ok || rawTris.empty()) return false;

    m_mesh = std::make_shared<Mesh>();

    m_mesh->fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    m_mesh->fileLocation = filePath;

    glm::vec3 minP{ 1e30f }, maxP{ -1e30f };

    for (const auto& raw : rawTris)
    {
        // Recompute face normal for robustness
        glm::vec3 e1 = raw.v[1] - raw.v[0];
        glm::vec3 e2 = raw.v[2] - raw.v[0];
        glm::vec3 n = glm::normalize(glm::cross(e1, e2));
        if (glm::length(n) < 0.5f) n = raw.normal;

        // Flat (non-deduplicated) — each triangle gets 3 unique vertices
        // so face normals are sharp. Swap to a map for smooth normals later.
        uint32_t baseIdx = (uint32_t)m_mesh->vertices.size();

        Vertex v0, v1, v2;
        v0.position = raw.v[0]; v0.normal = n;
        v1.position = raw.v[1]; v1.normal = n;
        v2.position = raw.v[2]; v2.normal = n;

        m_mesh->vertices.push_back(v0);
        m_mesh->vertices.push_back(v1);
        m_mesh->vertices.push_back(v2);

        m_mesh->indices.push_back(baseIdx + 0);
        m_mesh->indices.push_back(baseIdx + 1);
        m_mesh->indices.push_back(baseIdx + 2);

        minP = glm::min(minP, glm::min(raw.v[0], glm::min(raw.v[1], raw.v[2])));
        maxP = glm::max(maxP, glm::max(raw.v[0], glm::max(raw.v[1], raw.v[2])));
    }

    // Bounding box
    m_mesh->bounds.min = minP;
    m_mesh->bounds.max = maxP;

    // Derived properties
    glm::vec3 ext = maxP - minP;
    m_mesh->center = (minP + maxP) * 0.5f;
    m_mesh->radius = glm::length(ext) * 0.5f;
    if (m_mesh->radius < 1e-5f) m_mesh->radius = 1.0f;
    m_mesh->cameradistance = m_mesh->radius * 3.0f;

    //printf("[StlLoader] Loaded %zu triangles, %zu vertices from %s\n",
    //    rawTris.size(), m_mesh->vertices->size(), filePath.c_str());

    m_cache.meshes[filePath] = m_mesh;

    return true;
}