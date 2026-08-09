#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "core/GuidUtils.h"
#include "domain/dataContext.h"
#include "BuildPlateRenderer.h"
#include "FolderScanner.h" 
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include "backends/imgui_impl_opengl3.h"  
#include "backends/imgui_impl_glfw.h"     
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include "Project.h"
#include "stb_image.h"
#include "ModelInstanceFactory.h"

 using namespace adapters::scanning;


namespace slicer {

    // -----------------------------------------------------------------------
    // Shaders
    // -----------------------------------------------------------------------

    static const char* k_bgVS = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main()
{
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

    static const char* k_bgFS = R"GLSL(
#version 330 core
in vec2 vUV;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
out vec4 FragColor;
void main()
{
    vec3 color = mix(uBottomColor, uTopColor, vUV.y);
    FragColor = vec4(color, 1.0);
}
)GLSL";

    static const char* k_lineVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

    static const char* k_lineFS = R"GLSL(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

    static const char* k_gizmoVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat3 uNormalMat;

out vec3 vNrm;
out vec2 vUV;

void main()
{
    vNrm = normalize(uNormalMat * aNrm);
    vUV  = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

    static const char* k_gizmoFS = R"GLSL(
#version 330 core
in vec3 vNrm;
in vec2 vUV;

uniform vec3  uFaceColor;
uniform vec3  uLightDir;   // normalised, world space
uniform float uAmbient;
uniform int   uHovered;    // 1 = highlight this face

out vec4 FragColor;

void main()
{
    vec3 N    = normalize(vNrm);
    float diff = max(dot(N, -uLightDir), 0.0);
    float light = uAmbient + (1.0 - uAmbient) * diff;

    vec3 col = uFaceColor * light;
    if (uHovered == 1)
        col = mix(col, vec3(1.0), 0.28);

    FragColor = vec4(col, 1.0);
}
)GLSL";

    static const char* k_meshVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCamPos;

out vec3 vPosW;
out vec3 vNrmW;

void main()
{
    vec4 posW  = uModel * vec4(aPos, 1.0);
    vPosW      = posW.xyz;
    vNrmW      = normalize(mat3(transpose(inverse(uModel))) * aNrm);
    gl_Position = uProj * uView * posW;
}
)GLSL";

    static const char* k_meshFSBasic = R"GLSL(
#version 330 core
in vec3 vPosW;
in vec3 vNrmW;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uBaseColor;
uniform float uGhostFactor;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNrmW);
    if (!gl_FrontFacing)
        N = -N;

    vec3 V = normalize(uCamPos - vPosW);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // Simple ambient + diffuse + a single specular highlight
    vec3 ambient  = uBaseColor * 0.18;
    vec3 diffuse  = uBaseColor * NdotL * 0.75;
    float specPow = pow(NdotH, 24.0);
    vec3 specular = vec3(0.35) * specPow;

    // Cheap secondary fill light from above, so undersides aren't pure black
    vec3 fillDir = normalize(vec3(0.3, 0.8, 0.3));
    float fillNdotL = max(dot(N, fillDir), 0.0);
    vec3 fill = uBaseColor * fillNdotL * 0.12;

    vec3 color = ambient + diffuse + fill + specular;

    // Simple gamma correction, no full ACES tonemap curve
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, uGhostFactor);
}
)GLSL";

    static const char* k_meshFS = R"GLSL(
#version 330 core
in vec3 vPosW;
in vec3 vNrmW;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uBaseColor;
uniform float uGhostFactor;

const float uRoughness = 0.25;
const float uMetallic  = 0.1;

out vec4 FragColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 N = normalize(vNrmW);
    if (!gl_FrontFacing) {
        N = -N;
    }

    vec3 V = normalize(uCamPos - vPosW);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, uBaseColor, uMetallic);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    float NDF = DistributionGGX(N, H, uRoughness);   
    float G   = GeometrySmith(N, V, L, uRoughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - uMetallic;	  

    vec3 numerator   = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;
    vec3 specular = numerator / max(denominator, 0.000001);  
    
    vec3 diffuse = kD * uBaseColor / PI;
    vec3 mainLightResult = (diffuse + specular) * vec3(1.5) * NdotL;

    vec3 bounceDir = normalize(vec3(0.4, 0.8, 0.4));
    float bounceNdotL = max(dot(N, bounceDir), 0.0);
    vec3 bounceResult = (uBaseColor / PI) * vec3(0.25) * bounceNdotL;

    vec3 ambient = vec3(0.08) * uBaseColor;

    vec3 color = ambient + mainLightResult + bounceResult;

    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);

    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, uGhostFactor);
}
)GLSL";


GLuint LoadTexture(const std::string& filename)
{
    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

    if (!data)
    {
        printf("Failed to load %s\n", filename.c_str());
        return 0;
    }

    printf("Loaded texture: %s, %dx%d, channels: %d (forced to 4)\n",
        filename.c_str(), width, height, channels);

    // For PNGs without alpha, set alpha to 255
    if (channels == 3) {
        for (int i = 0; i < width * height; i++) {
            data[i * 4 + 3] = 255; // Force alpha to opaque
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    return texture;
}

// Add this class before SlicerCorePlugin
 
 
    TreeViewRenderer::TreeViewRenderer(NavigationManager* navigation, domain::v1::WorkspaceStore* workspaceStore, ModelCache* cache)
        : m_navigation(navigation),  m_cache(cache) {
        
        m_workspaceStore = workspaceStore;

    }
    TreeViewRenderer::~TreeViewRenderer()
    {
    }

    void TreeViewRenderer::render() {
        m_workspaceStore->read([&](const domain::v1::Workspace& ws)
            {
                if (ws.projects.empty()) {   // fixed: was !ws.projects.empty()
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    float availHeight = ImGui::GetContentRegionAvail().y;
                    const char* message = "No projects loaded.";
                    float textWidth = ImGui::CalcTextSize(message).x;
                    float textHeight = ImGui::GetTextLineHeight();
                    ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
                    ImGui::SetCursorPosY((availHeight - textHeight) * 0.5f);
                    ImGui::TextDisabled("%s", message);
                    return;
                }

                const auto& selectedIds = m_navigation->selection().getSelectedIds();
                ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick |
                    ImGuiTreeNodeFlags_SpanAvailWidth;

                for (auto* project : ws.projects) {
                    if (!project) continue;
                    std::string projectLabel = project->name.empty() ? "Project" : project->name;
                    ImGuiTreeNodeFlags projectFlags = baseFlags | ImGuiTreeNodeFlags_DefaultOpen;
                    bool projectOpen = ImGui::TreeNodeEx(project->Id.c_str(), projectFlags, "%s", projectLabel.c_str());
                    if (projectOpen) {
                        std::string platesLabel = "Build Plates (" + std::to_string(project->buildPlates.size()) + ")";
                        bool platesOpen = ImGui::TreeNodeEx((project->Id + "_plates").c_str(),
                            baseFlags | ImGuiTreeNodeFlags_DefaultOpen,
                            "%s", platesLabel.c_str());
                        if (platesOpen) {
                            for (auto* buildPlate : project->buildPlates) {
                                if (!buildPlate) continue;
                                renderBuildPlateNode(buildPlate, selectedIds, baseFlags);
                            }
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                }
            });
    }

 
    void TreeViewRenderer::renderBuildPlateNode(domain::v1::BuildPlate* buildPlate,
        const std::unordered_set<std::string>& selectedIds,
        ImGuiTreeNodeFlags baseFlags) {
        if (!buildPlate) return;

        ImGui::PushID(buildPlate->Id.c_str());

        // Check if this build plate is currently selected in navigation
        bool isPlateSelected = m_navigation->currentBuildPlateId() == buildPlate->Id;

        // Build plate node
        std::string plateLabel = buildPlate->name.empty() ? "Build Plate" : buildPlate->name;
        plateLabel += " (" + std::to_string(buildPlate->modelInstances.size()) + ")";

        ImGuiTreeNodeFlags plateFlags = baseFlags | ImGuiTreeNodeFlags_DefaultOpen;
        if (isPlateSelected) {
            plateFlags |= ImGuiTreeNodeFlags_Selected;
        }

        bool plateOpen = ImGui::TreeNodeEx("plate_node", plateFlags, "%s", plateLabel.c_str());

        // Handle click on build plate
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_navigation->setBuildPlate(buildPlate->Id);
            // Update the build plate renderer
            // m_buildPlateRenderer->updateViewModel(project->buildPlates);
        }

        if (plateOpen) {
            // Render model instances
            for (auto& instance : buildPlate->modelInstances) {
                if (!instance) continue;
                renderInstanceNode(instance.get(), selectedIds, baseFlags);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void TreeViewRenderer::renderInstanceNode(ModelInstance* instance,
        const std::unordered_set<std::string>& selectedIds,
        ImGuiTreeNodeFlags baseFlags) {
        if (!instance) return;

        ImGui::PushID(instance->id.c_str());

        // Check if this instance is selected
        bool isSelected = selectedIds.count(instance->id) > 0;

        std::string label = instance->name.empty() ? "Instance" : instance->name;

        // Leaf node - no tree arrow
        ImGuiTreeNodeFlags flags = baseFlags |
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Get color based on selection or other state
        ImVec4 textColor = isSelected ?
            ImVec4(1.0f, 0.8f, 0.3f, 1.0f) : // Selected color
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f);   // Default color

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::TreeNodeEx("instance_node", flags, "%s", label.c_str());
        ImGui::PopStyleColor();

        // Handle selection
        if (ImGui::IsItemClicked()) {
            ImGuiIO& io = ImGui::GetIO();
            auto& selection = m_navigation->selection();

            if (io.KeyCtrl) {
                // Ctrl+click - toggle selection
                if (isSelected) {
                    selection.select(instance->id,true);
                }
                else {
                    selection.select(instance->id, true);
                }
            }
            else if (io.KeyShift && !selection.getSelectedIds().empty() ) {
                // Shift+click - range selection
                // Note: You'd need to implement range selection logic here
                selection.select(instance->id, true);
            }
            else {
                // Normal click - select only this
                selection.clear();
                selection.select(instance->id,true);
            }

            // Update the build plate renderer to reflect selection
            // m_buildPlateRenderer->updateViewModel(project->buildPlates);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                // Delete instance logic
                // This should trigger a command
            }
            if (ImGui::MenuItem("Duplicate")) {
                // Duplicate instance logic
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All")) {
                // Select all instances in this build plate
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

 



    // -----------------------------------------------------------------------
    // BoundingBoxRenderer Implementation
    // -----------------------------------------------------------------------

        void BoundingBoxRenderer::create()
        {
            // Unit cube corners, -0.5 to 0.5 on each axis — scaled/positioned per-instance via the MVP matrix
            static const glm::vec3 corners[8] = {
                {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
                {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
            };
            // 12 edges, as line-list index pairs
            static const uint32_t edges[24] = {
                0,1, 1,2, 2,3, 3,0,   // bottom face
                4,5, 5,6, 6,7, 7,4,   // top face
                0,4, 1,5, 2,6, 3,7    // verticals
            };

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            glBindVertexArray(vao);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(edges), edges, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            glBindVertexArray(0);
            indexCount = 24;
        }

        void BoundingBoxRenderer::render(const glm::mat4& mvp, glm::vec3 color, GLuint lineShader) const
        {
            if (!vao) return;

            glUseProgram(lineShader);
            glUniformMatrix4fv(glGetUniformLocation(lineShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(glGetUniformLocation(lineShader, "uColor"), color.r, color.g, color.b);

            glBindVertexArray(vao);
            glDrawElements(GL_LINES, indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        BoundingBoxRenderer::BoundingBoxRenderer() {

        }
        BoundingBoxRenderer::~BoundingBoxRenderer()
        {
            if (ebo) glDeleteBuffers(1, &ebo);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);
        }

   
    // -----------------------------------------------------------------------
    // SceneLayout Implementation
    // -----------------------------------------------------------------------
    SceneLayout::SceneLayout()
    {
    }

    SceneLayout::~SceneLayout()
    {
    }

    // -----------------------------------------------------------------------
    std::vector<std::shared_ptr<RenderModel>> SceneLayout::getRenderModels()
    {
        return m_renderModels;
    }

    // -----------------------------------------------------------------------
    SceneBounds SceneLayout::getSceneBounds()
    {
        return m_sceneBounds;
    }

    // -----------------------------------------------------------------------
    void SceneLayout::preRender()
    {
        for (auto& model : m_renderModels)
            model->createVertixBuffer();
    }

    // -----------------------------------------------------------------------
    void SceneLayout::createLayout(std::vector<domain::v1::BuildPlate*> buildPlates, ModelCache& cache)
    {

      
        int nItems = (int)buildPlates.size();
        int aspectW = 3, aspectH = 1;
        int cols, rows;

        float padX = 100.0f;
        float padY = 100.0f;

        bestGrid(nItems, aspectW, aspectH, cols, rows);

        float totalWidth  = cols * m_itemWidth  + (cols - 1) * padX;
        float totalHeight = rows * m_itemHeight + (rows - 1) * padY;

        float leftEdge = -totalWidth  / 2.0f;
        float topEdge  =  totalHeight / 2.0f;

        m_renderModels.clear();
        m_plateRenderModels.clear();
        m_plateEntries.clear();

        int i = 0;
        for (auto* buildPlate : buildPlates)
        {

            printf("[createLayout] plate=%p instanceCount=%zu\n", (void*)buildPlate, buildPlate->modelInstances.size());


            int col = i % cols;
            int row = i / cols;

            glm::vec2 center = {
                leftEdge + col * (m_itemWidth  + padX) + m_itemWidth  / 2.0f,
                topEdge  - row * (m_itemHeight + padY) - m_itemHeight / 2.0f
            };

            m_plateEntries.push_back({ center, i });

            auto bpModel = std::make_shared<RenderModel>();
            bool ok = bpModel->create(buildPlate->buildPlateModel, center, true);
            printf("[createLayout] buildPlateModel create() = %d, mesh=%p\n", ok, (void*)buildPlate->buildPlateModel->mesh.get());
            
             m_plateRenderModels.push_back(bpModel);
       
             for (auto& instance : buildPlate->modelInstances)
             {
                 auto it = cache.models.find(instance->modelHash);
                 if (it == cache.models.end())
                 {
                     printf("[BuildPlateRender] CreateLayout: cant find model for instance=%s hash=%s\n",
                         instance->id.c_str(), instance->modelHash.c_str());
                     continue;
                 }

                 std::shared_ptr<domain::v1::Model> model = it->second;
                 auto rm = std::make_shared<RenderModel>();
                 if (rm->create(model, center))
                 {
                     rm->instanceId = instance->id;
                     m_renderModels.push_back(rm);
                 }
                 else
                 {
                     printf("[BuildPlateRender] CreateLayout: rm->create() failed for instance=%s\n", instance->id.c_str());
                 }
             }

            i++;
        }

        BoundingBox sceneBounds;
        if (buildPlates.size() == 1) {
            sceneBounds.min = glm::vec3(-m_itemWidth  * 0.5f, 0.0f, -m_itemHeight * 0.5f);
            sceneBounds.max = glm::vec3( m_itemWidth  * 0.5f, 0.0f,  m_itemHeight * 0.5f);
        }
        else {
            sceneBounds.min = glm::vec3(-totalWidth  * 0.4f, 0.0f, -totalHeight * 0.4f);
            sceneBounds.max = glm::vec3( totalWidth  * 0.4f, 0.0f,  totalHeight * 0.4f);
        }
        m_fullSceneBounds.reset();
        m_fullSceneBounds.grow(sceneBounds);
        m_sceneBounds.reset();
        m_sceneBounds.grow(sceneBounds);

        m_ghostFactors.assign(buildPlates.size(), 1.0f);
        m_ghostTargets.assign(buildPlates.size(), 1.0f);

        // Settle camera immediately at overview — no animation on first load
        m_selectedPlate   = -1;
        m_cameraCurrent   = computeTargetCamera(-1);
        m_cameraFrom      = m_cameraCurrent;
        m_cameraTo        = m_cameraCurrent;
        m_transitionT     = 1.0f;

        printf("[createLayout] FINAL m_renderModels.size() = %zu\n", m_renderModels.size());
    }

    // -----------------------------------------------------------------------
    void SceneLayout::selectPlate(int index)
    {
        if (index == m_selectedPlate)
            return;

        m_selectedPlate = index;
        m_cameraFrom    = m_cameraCurrent;
        m_cameraTo      = computeTargetCamera(index);
        m_transitionT   = 0.0f;

        for (int i = 0; i < (int)m_ghostTargets.size(); i++)
            m_ghostTargets[i] = (index == -1 || i == index) ? 1.0f : 0.0f;
    }

    // -----------------------------------------------------------------------
    RenderModel* SceneLayout::getRenderModel(const std::string& instanceId) const
    {
        for (auto& rm : m_renderModels)
        {
            //if (rm) printf("[getRenderModel] checking rm->instanceId='%s' against target='%s'\n", rm->instanceId.c_str(), instanceId.c_str());
            if (rm && rm->instanceId == instanceId)
                return rm.get();
        }
        //printf("[getRenderModel] NOT FOUND for '%s'\n", instanceId.c_str());
        return nullptr;
    }

    // -----------------------------------------------------------------------
    void SceneLayout::setYawPitch(float yaw, float pitch)
    {
        m_cameraCurrent.yaw   = yaw;
        m_cameraCurrent.pitch = pitch;
        // Also update destination so the next selectPlate snaps from here
        m_cameraTo.yaw   = yaw;
        m_cameraTo.pitch = pitch;
        m_cameraFrom.yaw   = yaw;
        m_cameraFrom.pitch = pitch;
    }

    // -----------------------------------------------------------------------
    CameraState SceneLayout::computeTargetCamera(int plateIndex) const
    {
        CameraState s;
        s.yaw   = m_defaultYaw;
        s.pitch = m_defaultPitch;

        if (plateIndex == -1 || m_plateEntries.empty()) {
            glm::vec3 center = m_fullSceneBounds.getCenter();
            float radius     = m_fullSceneBounds.getRadius();
            s.target   = center;
            s.distance = glm::clamp(radius * 2.0f, 400.0f, 2500.0f);
        }
        else {
            const auto& entry = m_plateEntries[plateIndex];
            s.target   = glm::vec3(entry.center.x, 0.0f, entry.center.y);
            s.distance = m_itemWidth * 1.5f;
        }
        return s;
    }

    // -----------------------------------------------------------------------
    void SceneLayout::update(float dt)
    {
        if (m_transitionT >= 1.0f)
            return;

        m_transitionT = glm::clamp(m_transitionT + dt / m_transitionDuration, 0.0f, 1.0f);

        // Ease in-out cubic
        float t = m_transitionT;
        float e = t < 0.5f ? 4*t*t*t : 1.0f - std::pow(-2.0f*t + 2.0f, 3.0f) / 2.0f;

        m_cameraCurrent.target   = glm::mix(m_cameraFrom.target,   m_cameraTo.target,   e);
        m_cameraCurrent.distance = glm::mix(m_cameraFrom.distance, m_cameraTo.distance, e);
        m_cameraCurrent.pitch    = glm::mix(m_cameraFrom.pitch,    m_cameraTo.pitch,    e);

        // Shortest-path yaw lerp
        float yawDelta = m_cameraTo.yaw - m_cameraFrom.yaw;
        if (yawDelta >  glm::pi<float>()) yawDelta -= glm::two_pi<float>();
        if (yawDelta < -glm::pi<float>()) yawDelta += glm::two_pi<float>();
        m_cameraCurrent.yaw = m_cameraFrom.yaw + yawDelta * e;

        // Lerp ghost factors toward targets
        for (int i = 0; i < (int)m_ghostFactors.size(); i++)
            m_ghostFactors[i] = glm::mix(m_ghostFactors[i], m_ghostTargets[i], e);
    }

    // -----------------------------------------------------------------------
    CameraState SceneLayout::getCurrentCamera() const
    {
        return m_cameraCurrent;
    }

    // -----------------------------------------------------------------------
    float SceneLayout::getPlateGhostFactor(int plateIndex) const
    {
        if (plateIndex < 0 || plateIndex >= (int)m_ghostFactors.size())
            return 1.0f;
        return m_ghostFactors[plateIndex];
    }

    // -----------------------------------------------------------------------
    bool SceneLayout::isAnimating() const
    {
        return m_transitionT < 1.0f;
    }
    // -----------------------------------------------------------------------
    void SceneLayout::adjustZoom(float delta)
    {
        m_cameraCurrent.distance = glm::clamp(m_cameraCurrent.distance + delta, 20.0f, 3000.0f);
        m_cameraTo.distance = m_cameraCurrent.distance;   // keep the "to" target in sync so it doesn't snap back
        m_cameraFrom.distance = m_cameraCurrent.distance;
    }

    // -----------------------------------------------------------------------
    void SceneLayout::bestGrid(int nItems, int aspectW, int aspectH, int& bestCols, int& bestRows)
    {
        double targetRatio = static_cast<double>(aspectW) / aspectH;
        double bestDiff    = std::numeric_limits<double>::max();

        bestCols = 1;
        bestRows = nItems;

        for (int cols = 1; cols <= nItems; ++cols) {
            int rows    = static_cast<int>(std::ceil(static_cast<double>(nItems) / cols));
            double ratio = static_cast<double>(cols) / rows;
            double diff  = std::fabs(ratio - targetRatio);

            if (diff < bestDiff) {
                bestDiff = diff;
                bestCols = cols;
                bestRows = rows;
            }
        }
    }

    // -----------------------------------------------------------------------
    // RenderModel Implementation
    // -----------------------------------------------------------------------
    RenderModel::RenderModel() {}
    RenderModel::~RenderModel() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
    }

    bool RenderModel::raycastBoundsOnly(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::mat4 modelMatrix) const
    {
        if (!model || !model->mesh) return false;

        const auto& b = model->mesh->bounds;
        glm::vec3 center = (b.max + b.min) * 0.5f;
        float radius = glm::length(b.max - b.min) * 0.5f;

        glm::vec4 worldCenter4 = modelMatrix * glm::vec4(center, 1.0f);
        glm::vec3 worldCenter = glm::vec3(worldCenter4) / worldCenter4.w;

        // Account for scale — radius should reflect the largest scaled extent
        glm::vec3 scale(glm::length(glm::vec3(modelMatrix[0])), glm::length(glm::vec3(modelMatrix[1])), glm::length(glm::vec3(modelMatrix[2])));
        float worldRadius = radius * std::max({ scale.x, scale.y, scale.z });

        // Ray-sphere intersection
        glm::vec3 oc = rayOrigin - worldCenter;
        float b2 = glm::dot(oc, rayDirection);
        float c = glm::dot(oc, oc) - worldRadius * worldRadius;
        float discriminant = b2 * b2 - c;

        return discriminant >= 0.0f;   // ray passes through (or near) the sphere
    }
    void RenderModel::createVertixBuffer()
    {
        std::shared_ptr<Mesh> mesh = model->mesh;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            mesh->vertices.size() * sizeof(Vertex),
            mesh->vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            mesh->indices.size() * sizeof(uint32_t),
            mesh->indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, normal));

        glBindVertexArray(0);

        indexCount = (uint32_t)mesh->indices.size();
    }

    // -----------------------------------------------------------------------
    bool RenderModel::create(std::shared_ptr<domain::v1::Model> sourceModel, glm::vec2& center, bool isBuildPlateModel)
    {
        if (!sourceModel->mesh)
            return false;

        model = std::make_shared<domain::v1::Model>();
         model->fileName = sourceModel->fileName;
        model->fileLocation = sourceModel->fileLocation;
        model->label = sourceModel->label;
        model->Id = sourceModel->Id;
        model->mesh = sourceModel->mesh;
        createVertixBuffer();  

        return true;
    }
    // -----------------------------------------------------------------------
    glm::mat4 RenderModel::getModelMatrix(const Transform& transform, glm::vec2 layoutOffset, bool isPlate) const
    {
        if (!model || !model->mesh) return glm::mat4(1.0f);
        glm::vec3 boundsCenter = (model->mesh->bounds.max + model->mesh->bounds.min) * 0.5f;

        float liftZ = model->mesh->bounds.size().z * 0.5f * transform.scale.z;
        float verticalOffset = isPlate ? -liftZ : liftZ;

        // World-space position: layoutOffset (plate placement) + transform.position (arrange placement)
        glm::vec3 worldPos(
            layoutOffset.x + transform.position.x,
            verticalOffset,
            layoutOffset.y + transform.position.y);
        glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);

        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -boundsCenter * transform.scale);
        glm::mat4 axisFix = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));

        glm::mat4 rotScale = glm::mat4(1.0f);
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
        rotScale = glm::scale(rotScale, transform.scale);

        return worldPosMat * axisFix * rotScale * toOrigin;
    }
    // -----------------------------------------------------------------------
    glm::mat4 RenderModel::getBoundingBoxMatrix(const Transform& transform, glm::vec2 layoutOffset) const
    {
        if (!model || !model->mesh) return glm::mat4(1.0f);

        const auto& bounds = model->mesh->bounds;
        glm::vec3 boundsCenter = (bounds.max + bounds.min) * 0.5f;
        glm::vec3 boundsSize = bounds.max - bounds.min;

        float liftZ = boundsSize.z * 0.5f * transform.scale.z;

        glm::vec3 worldPos(
            layoutOffset.x + transform.position.x,
            liftZ,
            layoutOffset.y + transform.position.y);
        glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);

        glm::mat4 axisFix = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));

        glm::mat4 rotScale = glm::mat4(1.0f);
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
        rotScale = glm::scale(rotScale, transform.scale);

        // Same centering step getModelMatrix() already uses successfully — unchanged, reused exactly.
        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -boundsCenter * transform.scale);

        // Maps the unit cube (-0.5..0.5) into mesh-space, spanning boundsMin..boundsMax —
        // i.e. the box's "shape" expressed the same way real mesh vertices are.
        glm::mat4 boxLocalMatrix = glm::translate(glm::mat4(1.0f), boundsCenter) * glm::scale(glm::mat4(1.0f), boundsSize);

        return worldPosMat * axisFix * rotScale * toOrigin * boxLocalMatrix;
    }
    // -----------------------------------------------------------------------
    void RenderModel::render(   glm::vec3 color, 
                                const Transform& transform,
                                GLuint shader, 
                                glm::mat4 view,
                                glm::mat4 proj,
                                glm::vec3 camPos, 
                                glm::vec2 center, 
                                float ghostFactor, 
                                bool isPlate 
                            ) const
    {
        if (!model) return;

        glm::mat4 modelMat = getModelMatrix(transform,center,isPlate);

        glUseProgram(shader);
        
        glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMat));


        glUniformMatrix4fv(glGetUniformLocation(shader, "uView"),  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"),  1, GL_FALSE, glm::value_ptr(proj));
        glUniform3f(glGetUniformLocation(shader, "uCamPos"),   camPos.x, camPos.y, camPos.z);
        glUniform3f(glGetUniformLocation(shader, "uLightDir"), -0.4f, -0.8f, -0.4f);
        glUniform3f(glGetUniformLocation(shader, "uBaseColor"),
             color.r,  color.g,  color.b);
        glUniform1f(glGetUniformLocation(shader, "uGhostFactor"), ghostFactor);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        //printf("[RenderModel::render] vao=%u indexCount=%u shader=%u pos=(%.2f,%.2f,%.2f)\n",
        //    vao, indexCount, shader, modelMat[3][0], modelMat[3][1], modelMat[3][2]);

    }

    // -----------------------------------------------------------------------
    bool RenderModel::intersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
        float& t, float& u, float& v) const
    {
        const float EPSILON = 0.000001f;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(rayDirection, edge2);
        float a = glm::dot(edge1, h);

        if (a > -EPSILON && a < EPSILON) return false;

        float f = 1.0f / a;
        glm::vec3 s = rayOrigin - v0;
        u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) return false;

        glm::vec3 q = glm::cross(s, edge1);
        v = f * glm::dot(rayDirection, q);
        if (v < 0.0f || u + v > 1.0f) return false;

        t = f * glm::dot(edge2, q);
        return (t > EPSILON);
    }

    // -----------------------------------------------------------------------
    bool RenderModel::raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
        RaycastHit& outHit, glm::mat4 modelMatrix) const
    {
        if (!model || !model->mesh || model->mesh->vertices.empty()) return false;

        glm::mat4 invModelMatrix = glm::inverse(modelMatrix);
        glm::vec4 localRayOrigin4 = invModelMatrix * glm::vec4(rayOrigin, 1.0f);
        glm::vec3 localRayOrigin  = glm::vec3(localRayOrigin4) / localRayOrigin4.w;
        glm::vec4 localRayDir4    = invModelMatrix * glm::vec4(rayDirection, 0.0f);
        glm::vec3 localRayDir     = glm::normalize(glm::vec3(localRayDir4));

        const auto& vertices = model->mesh->vertices;
        const auto& indices  = model->mesh->indices;

        float    closestT = std::numeric_limits<float>::max();
        bool     hit      = false;
        uint32_t hitTriangleIndex = 0;
        glm::vec3 hitPoint, hitNormal;

        for (size_t i = 0; i < indices.size(); i += 3) {
            if (indices[i]   >= vertices.size() ||
                indices[i+1] >= vertices.size() ||
                indices[i+2] >= vertices.size()) continue;

            const auto& v0 = vertices[indices[i]].position;
            const auto& v1 = vertices[indices[i+1]].position;
            const auto& v2 = vertices[indices[i+2]].position;

            float t, u, v;
            if (intersectTriangle(localRayOrigin, localRayDir, v0, v1, v2, t, u, v)) {
                if (t < closestT && t > 0.0f) {
                    closestT = t;
                    hit = true;
                    hitTriangleIndex = (uint32_t)(i / 3);
                    hitPoint  = localRayOrigin + localRayDir * t;
                    hitNormal = glm::normalize(glm::mix(
                        glm::mix(vertices[indices[i]].normal, vertices[indices[i+1]].normal, u),
                        vertices[indices[i+2]].normal, v));
                }
            }
        }

        if (hit) {
            glm::vec4 worldHitPoint = modelMatrix * glm::vec4(hitPoint, 1.0f);
            outHit.point  = glm::vec3(worldHitPoint) / worldHitPoint.w;
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
            outHit.normal = glm::normalize(normalMatrix * hitNormal);
            outHit.hit          = true;
            outHit.renderModel  = const_cast<RenderModel*>(this);
            outHit.model        = model;
            outHit.distance     = glm::length(outHit.point - rayOrigin);
            outHit.triangleIndex = hitTriangleIndex;
			outHit.instanceId = instanceId;
            return true;
        }

        return false;
    }

    // -----------------------------------------------------------------------
    // BuildPlateRenderer
    // -----------------------------------------------------------------------
    BuildPlateRenderer::BuildPlateRenderer()
    {

        m_sceneLayout    = SceneLayout();
      
        
        m_lastFrameTime  = std::chrono::steady_clock::now();
    }

    BuildPlateRenderer::~BuildPlateRenderer()
    {
        destroyGl();
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::initialize(domain::v1::WorkspaceStore& workspaceStore, domain::v1::Project* project, ModelCache& cache, NavigationManager& navigationManager)
    {
        //printf("[BuildPlateRenderer] initialize() START\n");
        m_modelCache = &cache;
        m_navigation = &navigationManager;
        m_workspaceStore = &workspaceStore;

        //m_selectionManager = m_navigation->selection();
        m_treeViewRenderer = std::make_unique<TreeViewRenderer>(m_navigation, m_workspaceStore , m_modelCache);

        ensureGl();
        //printf("[BuildPlateRenderer] after ensureGl()\n");
        m_sceneLayout.createLayout(project->buildPlates, *m_modelCache);
        //printf("[BuildPlateRenderer] after createLayout()\n");
        //m_sceneLayout.preRender();
        //printf("[BuildPlateRenderer] after preRender()\n");

        m_buildPlates = project->buildPlates;
        m_singleIcon = LoadTexture("C:\\github\\Pistachio-config\\Assets\\Icons\\buildplate_single.png");
        //printf("[BuildPlateRenderer] after LoadTexture single, handle=%u\n", m_singleIcon);
        m_multiIcon = LoadTexture("C:\\github\\Pistachio-config\\Assets\\Icons\\buildplate_multi.png");
        //printf("[BuildPlateRenderer] after LoadTexture multi, handle=%u\n", m_multiIcon);

        m_isLoaded = true;
        //printf("[BuildPlateRenderer] initialize() DONE, m_isLoaded=%d\n", m_isLoaded);
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::updateViewModel(std::vector<domain::v1::BuildPlate*> buildPlates)
    {
       // printf("[BuildPlateRenderer] updateViewModel called, %zu plates\n", buildPlates.size());

        m_buildPlates = buildPlates;
        m_sceneLayout.createLayout(buildPlates,*m_modelCache);
        //m_sceneLayout.preRender();
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::clear()
    {
        m_distance = 3.0f;
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::tick(float dtSeconds)
    {
        // Intentionally empty — animation is driven by renderOpenGL's dt
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::renderWindow()
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        uint32_t w = (uint32_t)std::max(4.0f, avail.x);
        uint32_t h = (uint32_t)std::max(4.0f, avail.y);

        render(w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);

        if (m_fboColor)
        {
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();

            ImGui::Image(
                (ImTextureID)(intptr_t)m_fboColor,
                ImVec2((float)w, (float)h),
                ImVec2(0, 1), ImVec2(1, 0));

            ImRect imageRect(cursorPos, ImVec2(cursorPos.x + w, cursorPos.y + h));
            bool isHovered = ImGui::IsMouseHoveringRect(imageRect.Min, imageRect.Max);

            if (isHovered)
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                float relX = (mousePos.x - cursorPos.x) / w;
                float relY = (mousePos.y - cursorPos.y) / h;
                relX = std::max(0.0f, std::min(1.0f, relX));
                relY = std::max(0.0f, std::min(1.0f, relY));

                m_lastMouseX = relX * w;
                m_lastMouseY = relY * h;
                m_mouseHovering = true;

                float pixelX = relX * w;
                float pixelY = (1.0f - relY) * h;

                // --- Hover raycast, throttled to only re-run when the mouse actually moved ---
                static glm::vec2 lastHoverPixel(-1000.0f, -1000.0f);
                glm::vec2 currentPixel(pixelX, pixelY);
                if (glm::distance(currentPixel, lastHoverPixel) > 1.0f)
                {
                    lastHoverPixel = currentPixel;

                    float ndcX = (2.0f * pixelX / w) - 1.0f;
                    float ndcY = (2.0f * pixelY / h) - 1.0f;
                    glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                    glm::vec4 rayEye = glm::inverse(m_proj) * rayClip;
                    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
                    glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(m_view) * rayEye));

                    RaycastHit hoverHit = raycast(m_camPos, rayWorld);
                    m_navigation->selection().setHovered(hoverHit.hit && !hoverHit.isPlateHit ? hoverHit.instanceId : "");
                }

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    handleMouseClick(pixelX, pixelY, w, h);
                }

                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f && !m_treeHovered)
                {
                    m_sceneLayout.adjustZoom(-wheel * 30.0f);   // negative wheel = zoom in, tune the multiplier to taste
                }
            }
            else
            {
                m_mouseHovering = false;
                m_navigation->selection().clearHovered();
            }

            m_registry->renderDragDropTargets("ASSET_PATHS");
           
            
            //renderCameraGizmo();


            ImGuiWindowFlags flags = 
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoCollapse |
                //ImGuiWindowFlags_NoResize |
                //ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoBackground |
                (m_treeHovered ? 0 : ImGuiWindowFlags_NoScrollbar);
            bool open = true;
            
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::SetNextWindowBgAlpha(0.0f);
            
            if (!m_treeHovered) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

            }
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);  // Force no docking
            ImGui::SetNextWindowPos(ImVec2(windowPos.x + 50, windowPos.y + 25), ImGuiCond_Always);
            //ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_Always);

            if (!ImGui::Begin("##slicer_window", nullptr, flags))
            {
                ImGui::End();
                return;
            }

            if (!m_treeHovered) {
                ImGui::PopStyleColor();
            }
            
           

            const float splitterH = 6.0f;
            const float minTree = 300.0f;
            const float minProps = 0.0f;
            const float minPreview = 0.0f;
            const float avail = ImGui::GetContentRegionAvail().y-30;

            float m_treeHeight = avail;// 446.0f;
            float m_previewHeight = 0.0f;

            // Clamp tree height
           // m_treeHeight = ImClamp(m_treeHeight, minTree, avail - minProps - minPreview - splitterH * 2);
            // Clamp preview height
            m_previewHeight = ImClamp(m_previewHeight, minPreview, avail - m_treeHeight - minProps - splitterH * 2);

          
            // ---- Tree ----
            ImGui::BeginChild("##slicer_tree",
                ImVec2(0, m_treeHeight),
                false,
                (m_treeHovered ? 0 : ImGuiWindowFlags_NoScrollbar));
 
            // Set background color for the tree
            //ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));


            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 8.0f);

            // Render tree content
            if (m_treeViewRenderer) {
                m_treeViewRenderer->render();
            }
            else {
                // Center "No projects loaded" text
                float availWidth = ImGui::GetContentRegionAvail().x;
                const char* message = "No projects loaded.";
                float textWidth = ImGui::CalcTextSize(message).x;
                ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
                ImGui::TextDisabled("%s", message);
            }
			ImGui::PopStyleVar(); // end indentation spacing

            m_treeHovered = ImGui::IsWindowHovered();

            //ImGui::PopStyleColor();  // Pop background color
            ImGui::EndChild();  // End project tree
           
            ImGui::End(); // End slicer window
            ImGui::PopStyleColor();
            

            DrawViewportToolbar(m_singleIcon,m_multiIcon,true);
                ;
   //         // ---- Tree ----
   //         ImGui::BeginChild("##slicer_tree", ImVec2(0, m_treeHeight), true,
   //             ImGuiWindowFlags_HorizontalScrollbar);


   //         m_treeViewRenderer->render(m_workspace );

   //         ImGui::EndChild();// end project tree


			//ImGui::End(); // end slicer window

            // --- FPS overlay, bottom-right of the viewport image ---
            char fpsText[32];
            snprintf(fpsText, sizeof(fpsText), "%.1f FPS", m_fpsDisplay);
            ImVec2 textSize = ImGui::CalcTextSize(fpsText);
            ImVec2 textPos(cursorPos.x + w - textSize.x - 10.0f, cursorPos.y + h - textSize.y - 8.0f);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(
                ImVec2(textPos.x - 4, textPos.y - 2),
                ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                IM_COL32(0, 0, 0, 130), 3.0f);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 230), fpsText);
        }
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::render(uint32_t width, uint32_t height)
    {
        renderOpenGL(width, height);
    }

    // -----------------------------------------------------------------------
     // -----------------------------------------------------------------------
    void BuildPlateRenderer::renderOpenGL(uint32_t width, uint32_t height)
    {
        if (!ensureGl()) return;

        auto t0 = std::chrono::high_resolution_clock::now();
        
        const glm::vec3 kSelectedHighlight = { 1.0f, 0.8f, 0.0f }; // gold

        size_t plateCount = std::min(m_buildPlates.size(), m_sceneLayout.m_plateEntries.size());
        //printf("[renderOpenGL] m_buildPlates.size()=%zu m_plateEntries.size()=%zu plateCount=%zu\n",
        //    m_buildPlates.size(), m_sceneLayout.m_plateEntries.size(), plateCount);

        width = std::max(1u, width);
        height = std::max(1u, height);
        ensureFbo(width, height);

        // --- Delta time ---
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;
        dt = std::min(dt, 0.1f); // clamp to avoid huge jumps after stalls/breakpoints

        dt = std::min(dt, 0.1f);

        m_fpsAccumTime += dt;
        m_fpsFrameCount++;
        if (m_fpsAccumTime >= 0.5f)   // refresh twice a second — a raw per-frame number is too jittery to read
        {
            m_fpsDisplay = m_fpsFrameCount / m_fpsAccumTime;
            m_fpsAccumTime = 0.0f;
            m_fpsFrameCount = 0;
        }
         

        // --- Advance animation ---
        m_sceneLayout.update(dt);

        // --- Build view/proj from current animated camera state ---
        updateCamera(width, height);



        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, (int)width, (int)height);
        
        
        //glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClear(GL_DEPTH_BUFFER_BIT);   // still clear depth — color gets overwritten by the gradient quad below

        glDisable(GL_DEPTH_TEST);   // draw background behind everything, ignore depth
        
        glUseProgram(m_bgProgram);
        //glUniform3f(glGetUniformLocation(m_bgProgram, "uTopColor"), 0.14f, 0.14f, 0.18f);
        //glUniform3f(glGetUniformLocation(m_bgProgram, "uBottomColor"), 0.06f, 0.06f, 0.08f);
        //glUniform3f(glGetUniformLocation(m_bgProgram, "uTopColor"), 1.0f, 0.0f, 1.0f);      // bright magenta
        //glUniform3f(glGetUniformLocation(m_bgProgram, "uBottomColor"), 0.0f, 1.0f, 1.0f);   // bright cyan
        glUniform3f(glGetUniformLocation(m_bgProgram, "uTopColor"), 0.16f, 0.17f, 0.22f);     // lighter slate blue-grey
        glUniform3f(glGetUniformLocation(m_bgProgram, "uBottomColor"), 0.03f, 0.03f, 0.05f);  // near-black
        
        
        glBindVertexArray(m_bgVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
 
        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);   // NEW — wireframe test
        
        auto t1 = std::chrono::high_resolution_clock::now();

        // First pass: opaque instances (ghostFactor == 1.0)
        for (size_t i = 0; i < plateCount; ++i)
        {
            auto* buildPlate = m_buildPlates[i];
            if (!buildPlate) continue;

            auto& plateEntry = m_sceneLayout.m_plateEntries[i];
            float ghost = m_sceneLayout.getPlateGhostFactor(plateEntry.plateIndex);
            if (ghost < 0.99f) continue; // deferred to translucent pass below

            Transform plateTransform;
            plateTransform.reset(); // scale = {1,1,1} — critical, see below

            // render loop, per plate, before the instance loop:
            if (i < m_sceneLayout.m_plateRenderModels.size() && m_sceneLayout.m_plateRenderModels[i])
            {

                const glm::vec3 kSelectedPlateColor = { 0.35f, 0.55f, 0.95f };  // blue tint, distinct from gold instance highlight
                const glm::vec3 kDefaultPlateColor = { 0.8f, 0.8f, 0.8f };

                glm::vec3 plateColor = (buildPlate->Id == m_navigation->currentBuildPlateId())
                    ? kSelectedPlateColor
                    : kDefaultPlateColor;

                

                m_sceneLayout.m_plateRenderModels[i]->render(
                    plateColor,
                    plateTransform, m_meshProgram,
                    m_view, 
                    m_proj, 
                    m_camPos, 
                    plateEntry.center, 
                    1.0f,true
                );
            }

            for (auto& instance : buildPlate->modelInstances)
            {
                RenderModel* rm = m_sceneLayout.getRenderModel(instance->id);
                if (!rm) continue;

                glm::vec3 color = instance->color;
                
                if (m_navigation->selection().isSelected(instance->id) || m_navigation->selection().isHovered(instance->id))
                    color = kSelectedHighlight;

                rm->render(color,
                    instance->transform,
                    m_meshProgram,
                    m_view,
                    m_proj,
                    m_camPos,
                    plateEntry.center,
                    1.0f
                );
              
                if (m_navigation->selection().isSelected(instance->id))
                {
                    const glm::vec3 kBoxOutlineColor = { 1.0f, 1.0f, 1.0f };  // white
                    glm::mat4 boxMatrix = rm->getBoundingBoxMatrix(instance->transform, plateEntry.center);
                    glm::mat4 mvp = m_proj * m_view * boxMatrix;
                    m_boundingBoxRenderer.render(mvp, kBoxOutlineColor, m_lineShader);
                }

                
            }
        }
        auto t2 = std::chrono::high_resolution_clock::now();

        // Second pass: ghosted translucent instances (ghostFactor < 1.0)
        bool anyGhosted = false;
        for (size_t i = 0; i < plateCount; ++i)
            if (m_sceneLayout.getPlateGhostFactor(m_sceneLayout.m_plateEntries[i].plateIndex) < 0.99f) { anyGhosted = true; break; }

        if (anyGhosted)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            for (size_t i = 0; i < plateCount; ++i)
            {
                auto* buildPlate = m_buildPlates[i];
                if (!buildPlate) continue;

                auto& plateEntry = m_sceneLayout.m_plateEntries[i];
                float ghost = m_sceneLayout.getPlateGhostFactor(plateEntry.plateIndex);
                if (ghost >= 0.99f) continue;

                // render loop, per plate, before the instance loop:
                if (i < m_sceneLayout.m_plateRenderModels.size() && m_sceneLayout.m_plateRenderModels[i])
                {
                    m_sceneLayout.m_plateRenderModels[i]->render(glm::vec3(0.8f), Transform{}, m_meshProgram,
                        m_view, m_proj, m_camPos, plateEntry.center, 1.0f,true);
                }

                for (auto& instance : buildPlate->modelInstances)
                {
                    RenderModel* rm = m_sceneLayout.getRenderModel(instance->id);
                    if (!rm) continue;

                    glm::vec3 color = m_navigation->selection().isSelected(instance->id) ? kSelectedHighlight : instance->color;

                    rm->render(color,
                        instance->transform,
                        m_meshProgram, 
                        m_view, 
                        m_proj, 
                        m_camPos,
                        plateEntry.center, 
                        ghost);
                }
            }

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        auto t3 = std::chrono::high_resolution_clock::now();

        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        static int frameCounter = 0;
        if (++frameCounter % 30 == 0)   // print every 30 frames so it's readable
        {
            auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
            printf("[perf] setup=%.2fms opaque=%.2fms translucent=%.2fms total=%.2fms\n",
                ms(t0, t1), ms(t1, t2), ms(t2, t3), ms(t0, t3));
        }
    }

    // -----------------------------------------------------------------------
    // Raycasting and Selection
    // -----------------------------------------------------------------------
    RaycastHit BuildPlateRenderer::raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
    {
        RaycastHit closestHit;
        closestHit.hit = false;
        closestHit.distance = std::numeric_limits<float>::max();

        size_t plateCount = std::min(m_buildPlates.size(), m_sceneLayout.m_plateEntries.size());

        // Pass 1: instances (parts always take priority over the bare plate underneath them)
        for (size_t i = 0; i < plateCount; ++i)
        {
            auto* buildPlate = m_buildPlates[i];
            if (!buildPlate) continue;

            glm::vec2 layoutOffset = m_sceneLayout.m_plateEntries[i].center;

            for (auto& instance : buildPlate->modelInstances)
            {
                RenderModel* rm = m_sceneLayout.getRenderModel(instance->id);
                if (!rm) continue;

                glm::mat4 modelMatrix = rm->getModelMatrix(instance->transform, layoutOffset);

                // Cheap bounding-sphere reject before the expensive per-triangle test
                if (!rm->raycastBoundsOnly(rayOrigin, rayDirection, modelMatrix))
                    continue;

                RaycastHit hit;
                if (rm->raycast(rayOrigin, rayDirection, hit, modelMatrix) && hit.distance < closestHit.distance)
                {
                    closestHit = hit;
                    closestHit.instanceId = instance->id;
                    closestHit.buildPlateId = buildPlate->Id;
                    closestHit.isPlateHit = false;
                }
            }
        }

        if (closestHit.hit) return closestHit;   // an instance was hit — done, don't test plates

        // Pass 2: plate meshes — only reached if no part was hit
        for (size_t i = 0; i < plateCount; ++i)
        {
            auto* buildPlate = m_buildPlates[i];
            if (!buildPlate) continue;
            if (i >= m_sceneLayout.m_plateRenderModels.size() || !m_sceneLayout.m_plateRenderModels[i]) continue;

            glm::vec2 layoutOffset = m_sceneLayout.m_plateEntries[i].center;
            Transform plateTransform;
            plateTransform.reset();

            glm::mat4 modelMatrix = m_sceneLayout.m_plateRenderModels[i]->getModelMatrix(plateTransform, layoutOffset, true);

            if (!m_sceneLayout.m_plateRenderModels[i]->raycastBoundsOnly(rayOrigin, rayDirection, modelMatrix))
                continue;

            RaycastHit hit;
            if (m_sceneLayout.m_plateRenderModels[i]->raycast(rayOrigin, rayDirection, hit, modelMatrix) && hit.distance < closestHit.distance)
            {
                closestHit = hit;
                closestHit.buildPlateId = buildPlate->Id;
                closestHit.isPlateHit = true;
            }
        }

        return closestHit;
    }

    // -----------------------------------------------------------------------
    
    void BuildPlateRenderer::selectInstance(const std::string& instanceId, bool additive)
    {
        m_navigation->selection().select(instanceId, additive);
    }

    void BuildPlateRenderer::deselectAll()
    {
        m_navigation->selection().clear();
    }

    // -----------------------------------------------------------------------
    RenderModel* BuildPlateRenderer::getSelectedModel() const
    {
        return m_selectedModel;
    }

    // -----------------------------------------------------------------------
   
    void BuildPlateRenderer::handleMouseClick(float mouseX, float mouseY, uint32_t width, uint32_t height)
    {
        if (!m_glReady || width == 0 || height == 0) return;
        auto beforeHover = std::chrono::high_resolution_clock::now();

        float ndcX = (2.0f * mouseX / width) - 1.0f;
        float ndcY = (2.0f * mouseY / height) - 1.0f;

        glm::vec4 rayClip = glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 rayEye = glm::inverse(m_proj) * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
        glm::vec4 rayWorld4 = glm::inverse(m_view) * rayEye;
        glm::vec3 rayWorld = glm::normalize(glm::vec3(rayWorld4));

        glm::vec3 rayOrigin = m_camPos;
        glm::vec3 rayDirection = rayWorld;

        RaycastHit hit = raycast(rayOrigin, rayDirection);
        bool shiftHeld = ImGui::GetIO().KeyShift;

        if (hit.hit && hit.isPlateHit)
        {
            m_navigation->setBuildPlate(hit.buildPlateId);   // single-select, not additive — plates aren't multi-select
            printf("[Raycast] Selected plate=%s\n", hit.buildPlateId.c_str());
        }
        else if (hit.hit)   // instance hit
        {
            if (shiftHeld)
                m_navigation->selection().toggle(hit.instanceId);
            else
                m_navigation->selection().select(hit.instanceId, false);
            printf("[Raycast] HIT instance=%s\n", hit.instanceId.c_str());
        }
        else
        {
            if (!shiftHeld)
                m_navigation->selection().clear();
            printf("[Raycast] MISS\n");
        }

        auto afterHover = std::chrono::high_resolution_clock::now();
        static int wc = 0;
        if (++wc % 30 == 0)
            printf("[perf] hoverClickBlock=%.2fms\n", std::chrono::duration<double, std::milli>(afterHover - beforeHover).count());
    }
    // -----------------------------------------------------------------------
    void* BuildPlateRenderer::getTexture() const
    {
        return (void*)(intptr_t)m_fboColor;
    }

    // -----------------------------------------------------------------------
    // updateCamera — thin consumer of SceneLayout camera state
    // -----------------------------------------------------------------------
    /*void BuildPlateRenderer::updateCamera(uint32_t w, uint32_t h)
    {
        CameraState cam = m_sceneLayout.getCurrentCamera();

        float pitchRad   = glm::radians(cam.pitch);
        float radiusOnXZ = cam.distance * std::cos(pitchRad);
        float heightOff  = cam.distance * std::sin(pitchRad);

        glm::vec3 camPos = {
            cam.target.x + std::cos(cam.yaw) * radiusOnXZ,
            cam.target.y + heightOff,
            cam.target.z + std::sin(cam.yaw) * radiusOnXZ
        };

        m_view   = glm::lookAt(camPos, cam.target, glm::vec3(0, 1, 0));
        m_camPos = camPos;

        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
        m_proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, cam.distance * 3.0f);
    }*/
    void BuildPlateRenderer::updateCamera(uint32_t w, uint32_t h)
    {

        if (!m_viewportController) return;
        const CameraState& cam = m_viewportController->camera();   // was: m_sceneLayout.getCurrentCamera()

        // pitch is already radians in the new global CameraState — no
        // glm::radians() conversion needed here anymore (was needed for the
        // old slicer::CameraState, which stored pitch in degrees).
        float radiusOnXZ = cam.distance * std::cos(cam.pitch);
        float heightOff = cam.distance * std::sin(cam.pitch);

        glm::vec3 camPos = {
            cam.target.x + std::cos(cam.yaw) * radiusOnXZ,
            cam.target.y + heightOff,
            cam.target.z + std::sin(cam.yaw) * radiusOnXZ
        };

        m_view = glm::lookAt(camPos, cam.target, glm::vec3(0, 1, 0));
        m_camPos = camPos;

        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
        m_proj = glm::perspective(cam.fovYRadians, aspect, 0.1f, cam.distance * 3.0f);   // adaptive far-plane preserved
    }
    // -----------------------------------------------------------------------
    // Camera Gizmo
    // -----------------------------------------------------------------------
    void BuildPlateRenderer::renderCameraGizmo()
    {
        const uint32_t gizmoW = 200;
        const uint32_t gizmoH = 200;
        const float    pad    = 14.0f;

        renderGizmoFbo(gizmoW, gizmoH);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);

        ImVec2 panelMin = ImGui::GetItemRectMin();
        ImVec2 panelMax = ImGui::GetItemRectMax();

        ImVec2 gizmoPos = ImVec2(panelMax.x - gizmoW - pad, panelMin.y + pad);
        ImVec2 centre   = ImVec2(gizmoPos.x + gizmoW * 0.5f, gizmoPos.y + gizmoH * 0.5f);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::SetCursorScreenPos(gizmoPos);
        ImGui::Image(
            (ImTextureID)(intptr_t)m_gizmoColor,
            ImVec2((float)gizmoW, (float)gizmoH),
            ImVec2(0, 1), ImVec2(1, 0));

        // Build gizmo camera matrices from scene camera state
        CameraState cam = m_sceneLayout.getCurrentCamera();
        float cy = std::cos(cam.yaw),            sy = std::sin(cam.yaw);
        float cp = std::cos(glm::radians(cam.pitch)), sp = std::sin(glm::radians(cam.pitch));

        float dist = 5.2f;
        glm::vec3 gizmoCamPos(dist * cy * cp, dist * sp, dist * sy * cp);
        glm::mat4 view = glm::lookAt(gizmoCamPos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(22.0f), (float)gizmoW / (float)gizmoH, 0.1f, 20.0f);
        glm::mat4 vp   = proj * view;
        glm::vec3 camFwd = glm::normalize(-gizmoCamPos);

        auto project2D = [&](glm::vec3 p) -> ImVec2 {
            glm::vec4 clip = vp * glm::vec4(p, 1.0f);
            glm::vec3 ndc  = glm::vec3(clip) / clip.w;
            return ImVec2(
                gizmoPos.x + (ndc.x * 0.5f + 0.5f) * gizmoW,
                gizmoPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * gizmoH);
        };

        struct LabelDef { glm::vec3 centre; glm::vec3 normal; const char* text; };
        LabelDef labels[] = {
            {{ 0,  1,  0}, { 0, 1, 0}, "TOP"  },
            {{ 0,  0,  1}, { 0, 0, 1}, "FRONT"},
            {{ 1,  0,  0}, { 1, 0, 0}, "RIGHT"},
            {{ 0,  0, -1}, { 0, 0,-1}, "BACK" },
            {{-1,  0,  0}, {-1, 0, 0}, "LEFT" },
            {{ 0, -1,  0}, { 0,-1, 0}, "BTM"  },
        };

        const float faceS = 0.72f;
        for (auto& lb : labels)
        {
            if (glm::dot(lb.normal, camFwd) >= -0.1f) continue;

            ImVec2 screenPt = project2D(lb.normal * faceS);
            ImVec2 ts       = ImGui::CalcTextSize(lb.text);
            float bpad      = 3.0f;
            ImVec2 bMin(screenPt.x - ts.x*0.5f - bpad, screenPt.y - ts.y*0.5f - bpad);
            ImVec2 bMax(screenPt.x + ts.x*0.5f + bpad, screenPt.y + ts.y*0.5f + bpad);
            dl->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 90), 3.0f);
            dl->AddText(ImVec2(screenPt.x - ts.x*0.5f, screenPt.y - ts.y*0.5f),
                IM_COL32(255, 255, 255, 245), lb.text);
        }

        // ---- Input handling ----
        ImVec2 mousePos = ImGui::GetMousePos();
        m_gizmoHoveredFace = -1;

        ImGui::SetCursorScreenPos(gizmoPos);
        ImGui::InvisibleButton("##camgizmo", ImVec2((float)gizmoW, (float)gizmoH));

        bool clicked  = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        bool dragging = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

        if (dragging)
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float newYaw   = cam.yaw   + delta.x * 0.005f;
            float newPitch = cam.pitch - delta.y * 0.005f * (180.0f / glm::pi<float>());
            newPitch = std::max(5.0f, std::min(89.0f, newPitch));
            m_sceneLayout.setYawPitch(newYaw, newPitch);
        }
        else if (clicked)
        {
            float nx = (mousePos.x - gizmoPos.x) / gizmoW * 2.0f - 1.0f;
            float ny = 1.0f - (mousePos.y - gizmoPos.y) / gizmoH * 2.0f;

            glm::vec4 rayClip(nx, ny, -1, 1);
            glm::vec4 rayEye = glm::inverse(proj) * rayClip;
            rayEye = { rayEye.x, rayEye.y, -1, 0 };
            glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));

            float bestDot = -1.0f;
            int   bestFace = -1;
            for (size_t i = 0; i < m_gizmoFaces.size(); i++) {
                const auto& f = m_gizmoFaces[i];
                if (glm::dot(f.normal, camFwd) < -0.05f) {
                    float d = glm::dot(f.normal, -rayDir);
                    if (d > bestDot) { bestDot = d; bestFace = (int)i; }
                }
            }
            if (bestFace >= 0)
                m_sceneLayout.setYawPitch(m_gizmoFaces[bestFace].snapYaw, m_gizmoFaces[bestFace].snapPitch);
        }

        // ---- Orbit arrows ----
        float arrowR = gizmoW * 0.52f;
        struct Arrow { float angle; float dYaw; float dPitch; };
        Arrow arrows[] = {
            { 0.0f,                   0.25f,  0.0f },
            { glm::pi<float>(),      -0.25f,  0.0f },
            { glm::half_pi<float>(),  0.0f,  15.0f },
            {-glm::half_pi<float>(),  0.0f, -15.0f },
        };
        for (auto& arr : arrows)
        {
            float ax = centre.x + std::cos(arr.angle) * arrowR;
            float ay = centre.y - std::sin(arr.angle) * arrowR;
            float as = 9.0f;
            float tipAngle = arr.angle + glm::pi<float>();

            ImVec2 tip(ax + std::cos(tipAngle)*as, ay - std::sin(tipAngle)*as);
            ImVec2 lft(ax + std::cos(tipAngle + glm::half_pi<float>())*as*0.5f,
                       ay - std::sin(tipAngle + glm::half_pi<float>())*as*0.5f);
            ImVec2 rgt(ax + std::cos(tipAngle - glm::half_pi<float>())*as*0.5f,
                       ay - std::sin(tipAngle - glm::half_pi<float>())*as*0.5f);

            bool hov = glm::length(glm::vec2(mousePos.x - ax, mousePos.y - ay)) < as * 1.5f;
            ImU32 col = hov ? IM_COL32(230,230,230,255) : IM_COL32(160,160,165,200);
            dl->AddTriangleFilled(tip, lft, rgt, col);

            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                float newYaw   = cam.yaw   + arr.dYaw;
                float newPitch = std::max(5.0f, std::min(89.0f, cam.pitch + arr.dPitch));
                m_sceneLayout.setYawPitch(newYaw, newPitch);
            }
        }

        // ---- XYZ axis lines ----
        {
            ImVec2 origin(gizmoPos.x + 16.0f, gizmoPos.y + gizmoH - 16.0f);
            float  axLen = 20.0f;

            struct AxisDef { glm::vec3 dir; ImU32 col; const char* lbl; };
            AxisDef axDefs[] = {
                {{1,0,0}, IM_COL32(220, 60, 60, 255), "X"},
                {{0,1,0}, IM_COL32( 60,200, 60, 255), "Y"},
                {{0,0,1}, IM_COL32( 60,120,220, 255), "Z"},
            };

            glm::vec3 camR = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
            glm::vec3 camU = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

            for (auto& ax : axDefs) {
                float sx = glm::dot(ax.dir, camR);
                float sy = -glm::dot(ax.dir, camU);
                ImVec2 tip(origin.x + sx*axLen, origin.y + sy*axLen);
                dl->AddLine(origin, tip, ax.col, 2.0f);
                dl->AddCircleFilled(tip, 3.0f, ax.col);
                ImVec2 ts = ImGui::CalcTextSize(ax.lbl);
                dl->AddText(ImVec2(tip.x - ts.x*0.5f - 1, tip.y - ts.y*0.5f - 1), IM_COL32(0,0,0,160), ax.lbl);
                dl->AddText(ImVec2(tip.x - ts.x*0.5f,     tip.y - ts.y*0.5f),     ax.col,               ax.lbl);
            }
        }
    }

    // -----------------------------------------------------------------------
    /*void BuildPlateRenderer::DrawViewportToolbar(GLuint singleIconTexture,
        GLuint multiIconTexture, bool multiBuildPlateView)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        constexpr float PaddingTop  = 8.0f;
        constexpr float ButtonSize  = 34.0f;
        constexpr float Spacing     = 4.0f;
        const float toolbarWidth    = ButtonSize * 2.0f + Spacing;
        const float toolbarHeight   = ButtonSize;

        ImVec2 pos(
            viewport->Pos.x + (viewport->Size.x - toolbarWidth) * 0.5f,
            viewport->Pos.y + PaddingTop);

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration  | ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoResize      | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4, 0));
        ImGui::Begin("##ViewportToolbar", nullptr, flags);

        if (!multiBuildPlateView)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        ImGui::PushID("SingleView");
        if (ImGui::ImageButton((ImTextureID)(intptr_t)singleIconTexture, ImVec2(24, 24)))
            multiBuildPlateView = false;
        ImGui::PopID();
        if (!multiBuildPlateView) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Single Build Plate");

        ImGui::SameLine();

        if (multiBuildPlateView)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        ImGui::PushID("MultiView");
        if (ImGui::ImageButton((ImTextureID)(intptr_t)multiIconTexture, ImVec2(24, 24)))
            multiBuildPlateView = true;
        ImGui::PopID();
        if (multiBuildPlateView) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Multi Build Plate");

        ImGui::End();
        ImGui::PopStyleVar(3);
    }*/
    //void BuildPlateRenderer::DrawViewportToolbar(GLuint singleIconTexture,
    //    GLuint multiIconTexture, bool multiBuildPlateView)
    //{
    //    ImGuiViewport* viewport = ImGui::GetMainViewport();

    //    ImVec2 windowPos = ImGui::GetWindowPos();
    //    ImVec2 windowSize = ImGui::GetWindowSize();

    //    constexpr float PaddingTop = 8.0f;
    //    constexpr float ButtonSize = 34.0f;
    //    constexpr float Spacing = 4.0f;
    //    const float toolbarWidth = ButtonSize * 2.0f + Spacing;
    //    const float toolbarHeight = ButtonSize;

    //    ImVec2 pos(
    //        windowPos.x + (windowSize.x - toolbarWidth) * 0.5f,
    //        windowPos.y + PaddingTop);

    //    ImGui::SetNextWindowPos(pos);
    //    ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight));

    //    ImGuiWindowFlags flags =
    //        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    //        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
    //        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
    //        ImGuiWindowFlags_NoNav |
    //        ImGuiWindowFlags_NoBackground;  // Transparent background

    //    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    //    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    //    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    //    ImGui::Begin("##ViewportToolbar", nullptr, flags);

    //    ImDrawList* drawList = ImGui::GetWindowDrawList();
    //  /*  ImVec2 windowPos = ImGui::GetWindowPos();
    //    ImVec2 windowSize = ImGui::GetWindowSize();*/

    //    // Background with rounded corners
    //    drawList->AddRectFilled(
    //        windowPos,
    //        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
    //        IM_COL32(40, 40, 45, 200),
    //        6.0f
    //    );

    //    // Single button
    //    ImVec2 button1Pos = ImVec2(windowPos.x + 4, windowPos.y + 4);
    //    ImVec2 button1Size = ImVec2(ButtonSize - 8, ButtonSize - 8);

    //    bool isSingleHovered = false;
    //    bool isSingleClicked = false;

    //    // Draw button background
    //    ImU32 button1Color = !multiBuildPlateView ?
    //        IM_COL32(80, 130, 255, 255) :
    //        IM_COL32(60, 60, 70, 255);

    //    drawList->AddRectFilled(
    //        button1Pos,
    //        ImVec2(button1Pos.x + button1Size.x, button1Pos.y + button1Size.y),
    //        button1Color,
    //        4.0f
    //    );

    //    // Draw icon - square (representing single build plate)
    //    float iconPadding = 4.0f;
    //    ImVec2 icon1Pos = ImVec2(button1Pos.x + iconPadding, button1Pos.y + iconPadding);
    //    ImVec2 icon1Size = ImVec2(button1Size.x - iconPadding * 2, button1Size.y - iconPadding * 2);

    //    drawList->AddRect(
    //        icon1Pos,
    //        ImVec2(icon1Pos.x + icon1Size.x, icon1Pos.y + icon1Size.y),
    //        IM_COL32(255, 255, 255, 200),
    //        2.0f,
    //        0,
    //        2.0f
    //    );

    //    // Add "1" text
    //    const char* singleLabel = "1";
    //    ImVec2 labelSize = ImGui::CalcTextSize(singleLabel);
    //    ImVec2 labelPos = ImVec2(
    //        button1Pos.x + (button1Size.x - labelSize.x) * 0.5f,
    //        button1Pos.y + (button1Size.y - labelSize.y) * 0.5f
    //    );
    //    drawList->AddText(labelPos, IM_COL32(255, 255, 255, 200), singleLabel);

    //    // Check click
    //    ImVec2 mousePos = ImGui::GetIO().MousePos;
    //    if (ImGui::IsMouseClicked(0) &&
    //        mousePos.x >= button1Pos.x && mousePos.x <= button1Pos.x + button1Size.x &&
    //        mousePos.y >= button1Pos.y && mousePos.y <= button1Pos.y + button1Size.y) {
    //        multiBuildPlateView = false;
    //    }

    //    // Hover state
    //    if (mousePos.x >= button1Pos.x && mousePos.x <= button1Pos.x + button1Size.x &&
    //        mousePos.y >= button1Pos.y && mousePos.y <= button1Pos.y + button1Size.y) {
    //        isSingleHovered = true;
    //        if (ImGui::IsMouseHoveringRect(button1Pos, ImVec2(button1Pos.x + button1Size.x, button1Pos.y + button1Size.y))) {
    //            ImGui::SetTooltip("Single Build Plate");
    //        }
    //    }

    //    // Multi button
    //    ImVec2 button2Pos = ImVec2(windowPos.x + ButtonSize + 4, windowPos.y + 4);

    //    ImU32 button2Color = multiBuildPlateView ?
    //        IM_COL32(80, 130, 255, 255) :
    //        IM_COL32(60, 60, 70, 255);

    //    drawList->AddRectFilled(
    //        button2Pos,
    //        ImVec2(button2Pos.x + button1Size.x, button2Pos.y + button1Size.y),
    //        button2Color,
    //        4.0f
    //    );

    //    // Draw icon - multiple squares (representing multi build plate)
    //    float smallSquareSize = (button1Size.x - iconPadding * 3) * 0.5f;

    //    // Top-left square
    //    drawList->AddRect(
    //        ImVec2(button2Pos.x + 2, button2Pos.y + 2),
    //        ImVec2(button2Pos.x + 2 + smallSquareSize, button2Pos.y + 2 + smallSquareSize),
    //        IM_COL32(255, 255, 255, 200),
    //        1.0f,
    //        0,
    //        1.0f
    //    );

    //    // Top-right square
    //    drawList->AddRect(
    //        ImVec2(button2Pos.x + 2 + smallSquareSize + 2, button2Pos.y + 2),
    //        ImVec2(button2Pos.x + 2 + smallSquareSize * 2 + 2, button2Pos.y + 2 + smallSquareSize),
    //        IM_COL32(255, 255, 255, 200),
    //        1.0f,
    //        0,
    //        1.0f
    //    );

    //    // Bottom-left square
    //    drawList->AddRect(
    //        ImVec2(button2Pos.x + 2, button2Pos.y + 2 + smallSquareSize + 2),
    //        ImVec2(button2Pos.x + 2 + smallSquareSize, button2Pos.y + 2 + smallSquareSize * 2 + 2),
    //        IM_COL32(255, 255, 255, 200),
    //        1.0f,
    //        0,
    //        1.0f
    //    );

    //    // Add "N" text
    //    const char* multiLabel = "N";
    //    labelSize = ImGui::CalcTextSize(multiLabel);
    //    labelPos = ImVec2(
    //        button2Pos.x + (button1Size.x - labelSize.x) * 0.5f,
    //        button2Pos.y + (button1Size.y - labelSize.y) * 0.5f
    //    );
    //    drawList->AddText(labelPos, IM_COL32(255, 255, 255, 200), multiLabel);

    //    // Check click for multi button
    //    if (ImGui::IsMouseClicked(0) &&
    //        mousePos.x >= button2Pos.x && mousePos.x <= button2Pos.x + button1Size.x &&
    //        mousePos.y >= button2Pos.y && mousePos.y <= button2Pos.y + button1Size.y) {
    //        multiBuildPlateView = true;
    //    }

    //    // Hover state for multi button
    //    if (mousePos.x >= button2Pos.x && mousePos.x <= button2Pos.x + button1Size.x &&
    //        mousePos.y >= button2Pos.y && mousePos.y <= button2Pos.y + button1Size.y) {
    //        if (ImGui::IsMouseHoveringRect(button2Pos, ImVec2(button2Pos.x + button1Size.x, button2Pos.y + button1Size.y))) {
    //            ImGui::SetTooltip("Multi Build Plate");
    //        }
    //    }

    //    ImGui::End();
    //    ImGui::PopStyleVar(3);
    //}
    // 
void BuildPlateRenderer::DrawViewportToolbar(GLuint singleIconTexture,
    GLuint multiIconTexture, bool multiBuildPlateView)
{
    // Get the current window's position and size
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    constexpr float PaddingTop = 32.0f;
    constexpr float ButtonSize = 34.0f;
    constexpr float Spacing = 4.0f;
    const float toolbarWidth = ButtonSize * 2.0f + Spacing;
    const float toolbarHeight = ButtonSize;
    ImVec2 pos(
        windowPos.x + (windowSize.x - toolbarWidth) * 0.5f,
        windowPos.y + PaddingTop);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight));
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##ViewportToolbar", nullptr, flags);

    // Draw background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 toolbarPos = ImGui::GetWindowPos();
    ImVec2 toolbarSize = ImGui::GetWindowSize();
    drawList->AddRectFilled(
        toolbarPos,
        ImVec2(toolbarPos.x + toolbarSize.x, toolbarPos.y + toolbarSize.y),
        IM_COL32(40, 40, 50, 200),
        6.0f
    );

    if (ImGui::IsWindowHovered()) {
        drawList->AddRect(
            toolbarPos,
            ImVec2(toolbarPos.x + toolbarSize.x, toolbarPos.y + toolbarSize.y),
            IM_COL32(100, 200, 255, 100),
            6.0f
        );
    }

    // ---- Single button ----
    if (singleIconTexture != 0) {
        bool wasActive = !multiBuildPlateView;   // captured ONCE, before the click can change state

        if (wasActive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

        ImGui::PushID("SingleView");
        if (ImGui::ImageButton((ImTextureID)(intptr_t)singleIconTexture, ImVec2(24, 24))) {
            multiBuildPlateView = false;
            printf("Single clicked!\n");
        }
        ImGui::PopID();

        if (wasActive) ImGui::PopStyleColor();   // matches the SAME captured condition

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Single Build Plate");
        }
    }
    else {
        ImGui::PushID("SingleFallback");
        if (ImGui::Button("S", ImVec2(24, 24))) {
            multiBuildPlateView = false;
        }
        ImGui::PopID();
    }

    ImGui::SameLine();

    // ---- Multi button ----
    if (multiIconTexture != 0) {
        bool wasActive = multiBuildPlateView;   // captured ONCE, before the click can change state

        if (wasActive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

        ImGui::PushID("MultiView");
        if (ImGui::ImageButton((ImTextureID)(intptr_t)multiIconTexture, ImVec2(24, 24))) {
            multiBuildPlateView = true;
            printf("Multi clicked!\n");
        }
        ImGui::PopID();

        if (wasActive) ImGui::PopStyleColor();   // matches the SAME captured condition

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Multi Build Plate");
        }
    }
    else {
        ImGui::PushID("MultiFallback");
        if (ImGui::Button("M", ImVec2(24, 24))) {
            multiBuildPlateView = true;
        }
        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(4);
}
 
    // -----------------------------------------------------------------------
    void BuildPlateRenderer::ensureGizmoGl()
    {
        if (m_gizmoVao) return;

        {
            unsigned vs = compileShader(GL_VERTEX_SHADER,   k_gizmoVS);
            unsigned fs = compileShader(GL_FRAGMENT_SHADER, k_gizmoFS);
            m_gizmoProgram = linkProgram(vs, fs);
            glDeleteShader(vs); glDeleteShader(fs);
        }

        const float S  = 1.0f;
        const float C  = 0.28f;
        const float Sc = S - C;

        std::vector<GizmoVertex> verts;
        std::vector<uint32_t>    idx;
        glm::vec3 n;

        // Main faces
        n = { 0,1,0 };  pushQuad(verts, idx, {-Sc,S,-Sc},{Sc,S,-Sc},{Sc,S,Sc},{-Sc,S,Sc}, n);
        n = { 0,-1,0 }; pushQuad(verts, idx, {-Sc,-S,Sc},{Sc,-S,Sc},{Sc,-S,-Sc},{-Sc,-S,-Sc}, n);
        n = { 0,0,1 };  pushQuad(verts, idx, {-Sc,-Sc,S},{Sc,-Sc,S},{Sc,Sc,S},{-Sc,Sc,S}, n);
        n = { 0,0,-1 }; pushQuad(verts, idx, {Sc,-Sc,-S},{-Sc,-Sc,-S},{-Sc,Sc,-S},{Sc,Sc,-S}, n);
        n = { 1,0,0 };  pushQuad(verts, idx, {S,-Sc,-Sc},{S,Sc,-Sc},{S,Sc,Sc},{S,-Sc,Sc}, n);
        n = { -1,0,0 }; pushQuad(verts, idx, {-S,-Sc,Sc},{-S,Sc,Sc},{-S,Sc,-Sc},{-S,-Sc,-Sc}, n);

        // Top edges
        n = glm::normalize(glm::vec3(0,1,1));   pushQuad(verts,idx,{-Sc,Sc,S},{Sc,Sc,S},{Sc,S,Sc},{-Sc,S,Sc},n);
        n = glm::normalize(glm::vec3(0,1,-1));  pushQuad(verts,idx,{-Sc,S,-Sc},{Sc,S,-Sc},{Sc,Sc,-S},{-Sc,Sc,-S},n);
        n = glm::normalize(glm::vec3(1,1,0));   pushQuad(verts,idx,{Sc,S,Sc},{Sc,S,-Sc},{S,Sc,-Sc},{S,Sc,Sc},n);
        n = glm::normalize(glm::vec3(-1,1,0));  pushQuad(verts,idx,{-S,Sc,Sc},{-S,Sc,-Sc},{-Sc,S,-Sc},{-Sc,S,Sc},n);

        // Bottom edges
        n = glm::normalize(glm::vec3(0,-1,1));  pushQuad(verts,idx,{-Sc,-S,Sc},{Sc,-S,Sc},{Sc,-Sc,S},{-Sc,-Sc,S},n);
        n = glm::normalize(glm::vec3(0,-1,-1)); pushQuad(verts,idx,{Sc,-Sc,-S},{-Sc,-Sc,-S},{-Sc,-S,-Sc},{Sc,-S,-Sc},n);
        n = glm::normalize(glm::vec3(1,-1,0));  pushQuad(verts,idx,{S,-Sc,Sc},{S,-Sc,-Sc},{S,-S,-Sc},{S,-S,Sc},n);
        n = glm::normalize(glm::vec3(-1,-1,0)); pushQuad(verts,idx,{-S,-Sc,-Sc},{-S,-Sc,Sc},{-S,-S,Sc},{-S,-S,-Sc},n);

        // Middle edges
        n = glm::normalize(glm::vec3(1,0,1));   pushQuad(verts,idx,{S,-Sc,Sc},{S,Sc,Sc},{Sc,Sc,S},{Sc,-Sc,S},n);
        n = glm::normalize(glm::vec3(-1,0,1));  pushQuad(verts,idx,{-Sc,-Sc,S},{-Sc,Sc,S},{-S,Sc,Sc},{-S,-Sc,Sc},n);
        n = glm::normalize(glm::vec3(1,0,-1));  pushQuad(verts,idx,{Sc,-Sc,-S},{Sc,Sc,-S},{S,Sc,-Sc},{S,-Sc,-Sc},n);
        n = glm::normalize(glm::vec3(-1,0,-1)); pushQuad(verts,idx,{-S,-Sc,-Sc},{-S,Sc,-Sc},{-Sc,Sc,-S},{-Sc,-Sc,-S},n);

        // Corner triangles
        n = glm::normalize(glm::vec3(1,1,1));    pushTri(verts,idx,{S,Sc,Sc},{Sc,S,Sc},{Sc,Sc,S},n);
        n = glm::normalize(glm::vec3(-1,1,1));   pushTri(verts,idx,{-Sc,S,Sc},{-S,Sc,Sc},{-Sc,Sc,S},n);
        n = glm::normalize(glm::vec3(1,1,-1));   pushTri(verts,idx,{Sc,S,-Sc},{S,Sc,-Sc},{Sc,Sc,-S},n);
        n = glm::normalize(glm::vec3(-1,1,-1));  pushTri(verts,idx,{-S,Sc,-Sc},{-Sc,S,-Sc},{-Sc,Sc,-S},n);
        n = glm::normalize(glm::vec3(1,-1,1));   pushTri(verts,idx,{Sc,-Sc,S},{Sc,-S,Sc},{S,-Sc,Sc},n);
        n = glm::normalize(glm::vec3(-1,-1,1));  pushTri(verts,idx,{-S,-Sc,Sc},{-Sc,-S,Sc},{-Sc,-Sc,S},n);
        n = glm::normalize(glm::vec3(1,-1,-1));  pushTri(verts,idx,{S,-Sc,-Sc},{Sc,-S,-Sc},{Sc,-Sc,-S},n);
        n = glm::normalize(glm::vec3(-1,-1,-1)); pushTri(verts,idx,{-Sc,-Sc,-S},{-Sc,-S,-Sc},{-S,-Sc,-Sc},n);

        m_gizmoFaces.clear();
        uint32_t off = 0;

        auto addFace = [&](uint32_t count, glm::vec3 nrm, glm::vec3 col,
            const char* lbl, float snapYaw, float snapPitch)
        {
            m_gizmoFaces.push_back({ off, count, nrm, col, lbl, snapYaw, snapPitch });
            off += count;
        };

        auto en = [](float x, float y, float z) { return glm::normalize(glm::vec3(x,y,z)); };

        addFace(6, {0,1,0},  {0.55f,0.65f,0.85f}, "TOP",   0.0f,              89.0f);
        addFace(6, {0,-1,0}, {0.40f,0.45f,0.55f}, "BTM",   0.0f,             -89.0f);
        addFace(6, {0,0,1},  {0.45f,0.60f,0.80f}, "FRONT", glm::radians( 90.0f), 0.5f);
        addFace(6, {0,0,-1}, {0.35f,0.45f,0.65f}, "BACK",  glm::radians(-90.0f), 0.5f);
        addFace(6, {1,0,0},  {0.75f,0.35f,0.35f}, "RIGHT", glm::radians(180.0f), 0.5f);
        addFace(6, {-1,0,0}, {0.55f,0.25f,0.25f}, "LEFT",  0.0f,               0.5f);

        glm::vec3 edgeCol   = {0.52f,0.54f,0.60f};
        glm::vec3 cornerCol = {0.45f,0.47f,0.52f};

        addFace(6, en(0,1,1),   edgeCol, nullptr, glm::radians( 90.f),  45.0f);
        addFace(6, en(0,1,-1),  edgeCol, nullptr, glm::radians(-90.f),  45.0f);
        addFace(6, en(1,1,0),   edgeCol, nullptr, glm::radians(180.f),  45.0f);
        addFace(6, en(-1,1,0),  edgeCol, nullptr, 0.0f,                 45.0f);
        addFace(6, en(0,-1,1),  edgeCol, nullptr, glm::radians( 90.f), -45.0f);
        addFace(6, en(0,-1,-1), edgeCol, nullptr, glm::radians(-90.f), -45.0f);
        addFace(6, en(1,-1,0),  edgeCol, nullptr, glm::radians(180.f), -45.0f);
        addFace(6, en(-1,-1,0), edgeCol, nullptr, 0.0f,                -45.0f);
        addFace(6, en(1,0,1),   edgeCol, nullptr, glm::radians(135.f),  0.0f);
        addFace(6, en(-1,0,1),  edgeCol, nullptr, glm::radians( 45.f),  0.0f);
        addFace(6, en(1,0,-1),  edgeCol, nullptr, glm::radians(-135.f), 0.0f);
        addFace(6, en(-1,0,-1), edgeCol, nullptr, glm::radians(-45.f),  0.0f);

        addFace(3, en(1,1,1),    cornerCol, nullptr, glm::radians( 135.f),  35.0f);
        addFace(3, en(-1,1,1),   cornerCol, nullptr, glm::radians(  45.f),  35.0f);
        addFace(3, en(1,1,-1),   cornerCol, nullptr, glm::radians(-135.f),  35.0f);
        addFace(3, en(-1,1,-1),  cornerCol, nullptr, glm::radians( -45.f),  35.0f);
        addFace(3, en(1,-1,1),   cornerCol, nullptr, glm::radians( 135.f), -35.0f);
        addFace(3, en(-1,-1,1),  cornerCol, nullptr, glm::radians(  45.f), -35.0f);
        addFace(3, en(1,-1,-1),  cornerCol, nullptr, glm::radians(-135.f), -35.0f);
        addFace(3, en(-1,-1,-1), cornerCol, nullptr, glm::radians( -45.f), -35.0f);

        glGenVertexArrays(1, &m_gizmoVao);
        glGenBuffers(1, &m_gizmoVbo);
        glGenBuffers(1, &m_gizmoEbo);
        glBindVertexArray(m_gizmoVao);

        glBindBuffer(GL_ARRAY_BUFFER, m_gizmoVbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(GizmoVertex), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gizmoEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, nrm));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, uv));

        glBindVertexArray(0);
        m_gizmoIndexCount = (uint32_t)idx.size();
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::ensureGizmoFbo(uint32_t w, uint32_t h)
    {
        if (m_gizmoFboW == w && m_gizmoFboH == h && m_gizmoFbo) return;

        if (m_gizmoFbo) {
            glDeleteFramebuffers(1, &m_gizmoFbo);
            glDeleteTextures(1, &m_gizmoColor);
            glDeleteRenderbuffers(1, &m_gizmoDepth);
            m_gizmoFbo = m_gizmoColor = m_gizmoDepth = 0;
        }

        glGenFramebuffers(1, &m_gizmoFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_gizmoFbo);

        glGenTextures(1, &m_gizmoColor);
        glBindTexture(GL_TEXTURE_2D, m_gizmoColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gizmoColor, 0);

        glGenRenderbuffers(1, &m_gizmoDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_gizmoDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_gizmoDepth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_gizmoFboW = w;
        m_gizmoFboH = h;
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::renderGizmoFbo(uint32_t w, uint32_t h)
    {
        ensureGizmoGl();
        ensureGizmoFbo(w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, m_gizmoFbo);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        float aspect = (float)w / (float)h;
        glm::mat4 proj = glm::perspective(glm::radians(28.0f), aspect, 0.1f, 20.0f);

        // Use scene camera yaw/pitch so the gizmo tracks the main camera
        CameraState cam = m_sceneLayout.getCurrentCamera();
        float cy = std::cos(cam.yaw),            sy = std::sin(cam.yaw);
        float cp = std::cos(glm::radians(cam.pitch)), sp = std::sin(glm::radians(cam.pitch));

        float dist = 8.5f;
        glm::vec3 camPos(dist*cy*cp, dist*sp, dist*sy*cp);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 vp   = proj * view;

        glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1.5f, -1));

        glUseProgram(m_gizmoProgram);
        glUniform3fv(glGetUniformLocation(m_gizmoProgram, "uLightDir"), 1, glm::value_ptr(lightDir));
        glUniform1f(glGetUniformLocation(m_gizmoProgram, "uAmbient"), 0.35f);

        glBindVertexArray(m_gizmoVao);

        for (size_t i = 0; i < m_gizmoFaces.size(); i++)
        {
            const GizmoFace& f = m_gizmoFaces[i];
            glm::mat4 model  = glm::mat4(1.0f);
            glm::mat4 mvp    = vp * model;
            glm::mat3 nmat   = glm::transpose(glm::inverse(glm::mat3(model)));

            glUniformMatrix4fv(glGetUniformLocation(m_gizmoProgram, "uMVP"),       1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix3fv(glGetUniformLocation(m_gizmoProgram, "uNormalMat"), 1, GL_FALSE, glm::value_ptr(nmat));
            glUniform3fv(glGetUniformLocation(m_gizmoProgram, "uFaceColor"),       1, glm::value_ptr(f.color));
            glUniform1i(glGetUniformLocation(m_gizmoProgram, "uHovered"), (int)(m_gizmoHoveredFace == (int)i));

            glDrawElements(GL_TRIANGLES, f.idxCount, GL_UNSIGNED_INT,
                (void*)(uintptr_t)(f.idxOffset * sizeof(uint32_t)));
        }

        glBindVertexArray(0);
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

       
    }

    // -----------------------------------------------------------------------
    bool BuildPlateRenderer::ensureGl()
    {
        if (m_glReady) return true;

#ifdef _WIN32
        auto loader = [](const char* name) -> void* {
            void* p = (void*)wglGetProcAddress(name);
            if (!p) {
                static HMODULE gl32 = LoadLibraryA("opengl32.dll");
                if (gl32) p = (void*)GetProcAddress(gl32, name);
            }
            return p;
        };
        if (!gladLoadGLLoader((GLADloadproc)loader)) {
            printf("[BuildPlateRenderer] gladLoadGLLoader failed\n");
            return false;
        }
#endif

        if (!glad_glCreateProgram) {
            printf("[BuildPlateRenderer] GL not available after gladLoadGLLoader\n");
            return false;
        }

        try {
            createShaders();
            m_glReady = true;
            printf("[BuildPlateRenderer] GL initialised OK\n");
        }
        catch (const std::exception& e) {
            printf("[BuildPlateRenderer] GL init failed: %s\n", e.what());
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::createShaders()
    {
        unsigned int vs = compileShader(GL_VERTEX_SHADER,   k_meshVS);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_meshFS);
        m_meshProgram   = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        unsigned int lvs = compileShader(GL_VERTEX_SHADER, k_lineVS);
        unsigned int lfs = compileShader(GL_FRAGMENT_SHADER, k_lineFS);
        m_lineShader = linkProgram(lvs, lfs);
        glDeleteShader(lvs);
        glDeleteShader(lfs);

        m_boundingBoxRenderer.create();


        unsigned int bvs = compileShader(GL_VERTEX_SHADER, k_bgVS);
        unsigned int bfs = compileShader(GL_FRAGMENT_SHADER, k_bgFS);
        m_bgProgram = linkProgram(bvs, bfs);
        glDeleteShader(bvs);
        glDeleteShader(bfs);

        ensureBackgroundQuad();
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::ensureBackgroundQuad()
    {
        if (m_bgVao) return;
        float verts[] = { -1.0f, -3.0f,   3.0f, 1.0f,   -1.0f, 1.0f };  // covers full screen via a single oversized triangle
        glGenVertexArrays(1, &m_bgVao);
        glGenBuffers(1, &m_bgVbo);
        glBindVertexArray(m_bgVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_bgVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glBindVertexArray(0);
    }
    // -----------------------------------------------------------------------

    void BuildPlateRenderer::ensureFbo(uint32_t w, uint32_t h)
    {
        if (m_fboWidth == w && m_fboHeight == h && m_fbo) return;

        if (m_fbo) {
            glDeleteFramebuffers(1, &m_fbo);
            glDeleteTextures(1, &m_fboColor);
            glDeleteRenderbuffers(1, &m_fboDepth);
            m_fbo = m_fboColor = m_fboDepth = 0;
        }

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        glGenTextures(1, &m_fboColor);
        glBindTexture(GL_TEXTURE_2D, m_fboColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)w, (int)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboColor, 0);

        glGenRenderbuffers(1, &m_fboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)w, (int)h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_fboDepth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_fboWidth  = w;
        m_fboHeight = h;
    }

    // -----------------------------------------------------------------------
    void BuildPlateRenderer::destroyGl()
    {
        if (!m_glReady) return;

        if (m_meshProgram) { glDeleteProgram(m_meshProgram); m_meshProgram = 0; }
        if (m_meshVao)     { glDeleteVertexArrays(1, &m_meshVao); m_meshVao = 0; }
        if (m_meshVbo)     { glDeleteBuffers(1, &m_meshVbo); m_meshVbo = 0; }
        if (m_gridProgram) { glDeleteProgram(m_gridProgram); m_gridProgram = 0; }
        if (m_gridVao)     { glDeleteVertexArrays(1, &m_gridVao); m_gridVao = 0; }
        if (m_gridVbo)     { glDeleteBuffers(1, &m_gridVbo); m_gridVbo = 0; }

        if (m_fbo) {
            glDeleteFramebuffers(1, &m_fbo);
            glDeleteTextures(1, &m_fboColor);
            glDeleteRenderbuffers(1, &m_fboDepth);
            m_fbo = m_fboColor = m_fboDepth = 0;
        }

        if (m_gizmoProgram) { glDeleteProgram(m_gizmoProgram); m_gizmoProgram = 0; }
        if (m_gizmoVao)     { glDeleteVertexArrays(1, &m_gizmoVao); m_gizmoVao = 0; }
        if (m_gizmoVbo)     { glDeleteBuffers(1, &m_gizmoVbo); m_gizmoVbo = 0; }
        if (m_gizmoEbo)     { glDeleteBuffers(1, &m_gizmoEbo); m_gizmoEbo = 0; }
        if (m_gizmoFbo) {
            glDeleteFramebuffers(1, &m_gizmoFbo);
            glDeleteTextures(1, &m_gizmoColor);
            glDeleteRenderbuffers(1, &m_gizmoDepth);
            m_gizmoFbo = m_gizmoColor = m_gizmoDepth = 0;
        }

        if (m_bgProgram) { glDeleteProgram(m_bgProgram); m_bgProgram = 0; }
        if (m_bgVao) { glDeleteVertexArrays(1, &m_bgVao); m_bgVao = 0; }
        if (m_bgVbo) { glDeleteBuffers(1, &m_bgVbo); m_bgVbo = 0; }

        m_glReady = false;
    }

    // -----------------------------------------------------------------------
    unsigned int BuildPlateRenderer::compileShader(unsigned int type, const char* src)
    {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            int len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetShaderInfoLog(s, len, nullptr, log.data());
            glDeleteShader(s);
            throw std::runtime_error("Shader compile failed: " + log);
        }
        return s;
    }

    // -----------------------------------------------------------------------
    unsigned int BuildPlateRenderer::linkProgram(unsigned int vs, unsigned int fs)
    {
        unsigned int p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        int ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            int len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p);
            throw std::runtime_error("Program link failed: " + log);
        }
        return p;
    }

} // namespace slicer
