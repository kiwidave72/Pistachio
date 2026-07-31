

#include "domain\dataContext.h"
 

class StlLoaderAdapter
{
public:

    explicit StlLoaderAdapter(ModelCache& cache);

    std::vector<RawTriangle> Triangle();
    std::shared_ptr<Mesh>  getMesh();
    bool load(const std::string& filePath);


private:
    std::vector<RawTriangle> tris;
    std::shared_ptr<Mesh> m_mesh;

     ModelCache& m_cache;

    bool  loadBinaryStl(const std::string& path, std::vector<RawTriangle>& out);
    bool  isAsciiStl(const std::string& path);
    bool  loadAsciiStl(const std::string& path, std::vector<RawTriangle>& out);



};
