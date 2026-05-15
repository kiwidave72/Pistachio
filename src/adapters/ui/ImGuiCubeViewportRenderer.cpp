#include "adapters/ui/ImGuiCubeViewportRenderer.h"
#include "imgui.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace
{
    struct V3 { float x, y, z; };
    inline V3 v3(float x, float y, float z) { return { x,y,z }; }
    inline V3 add(V3 a, V3 b) { return { a.x + b.x,a.y + b.y,a.z + b.z }; }
    inline V3 sub(V3 a, V3 b) { return { a.x - b.x,a.y - b.y,a.z - b.z }; }
    inline V3 mul(V3 a, float s) { return { a.x * s,a.y * s,a.z * s }; }
    inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    inline float len(V3 a) { return std::sqrt(dot(a, a)); }
    inline V3 norm(V3 a) { float l = len(a); return (l > 0.00001f) ? mul(a, 1.0f / l) : v3(0, 0, 0); }

    struct V2 { float x, y; operator ImVec2() const { return ImVec2(x, y); } };

    inline V3 rotateX(V3 p, float a)
    {
        float c = std::cos(a), s = std::sin(a);
        return { p.x, c * p.y - s * p.z, s * p.y + c * p.z };
    }
    inline V3 rotateY(V3 p, float a)
    {
        float c = std::cos(a), s = std::sin(a);
        return { c * p.x + s * p.z, p.y, -s * p.x + c * p.z };
    }

    inline ImU32 shade(ImU32 base, float k)
    {
        // CPU "tonemapped" shading for the background cube.
        // Goal: bright enough on a dark UI, but still preserves face colour.
        int r = (base >> IM_COL32_R_SHIFT) & 0xFF;
        int g = (base >> IM_COL32_G_SHIFT) & 0xFF;
        int b = (base >> IM_COL32_B_SHIFT) & 0xFF;

        // Clamp lighting factor to avoid muddy shadows OR full washout.
        float kk = ImClamp(k, 0.75f, 1.35f);

        // CAD-like tonemap: more exposure + more lift so it reads in a dark UI.
        const float exposure = 2.15f;
        const float lift     = 40.0f;
float rf = r * kk * exposure + lift;
        float gf = g * kk * exposure + lift;
        float bf = b * kk * exposure + lift;

        // Preserve saturation a bit (otherwise lift/exposure can drift toward grey/white).
        const float sat = 1.35f;
        float y = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
        rf = y + (rf - y) * sat;
        gf = y + (gf - y) * sat;
        bf = y + (bf - y) * sat;

        auto clamp255 = [](float v) -> int {
            if (v < 0.0f) return 0;
            if (v > 255.0f) return 255;
            return (int)(v + 0.5f);
        };

        r = clamp255(rf);
        g = clamp255(gf);
        b = clamp255(bf);

        return IM_COL32(r, g, b, 255); // FORCE opaque
    }


    // Write a triangle with per-vertex colors (Gouraud shading).
    inline void AddTriColored(ImDrawList* dl, const ImVec2& a, ImU32 ca, const ImVec2& b, ImU32 cb, const ImVec2& c, ImU32 cc)
    {
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        dl->PrimReserve(3, 3);
        const ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;
        dl->PrimWriteIdx((ImDrawIdx)(idx + 0));
        dl->PrimWriteIdx((ImDrawIdx)(idx + 1));
        dl->PrimWriteIdx((ImDrawIdx)(idx + 2));
        dl->PrimWriteVtx(a, uv, ca);
        dl->PrimWriteVtx(b, uv, cb);
        dl->PrimWriteVtx(c, uv, cc);
    }

    static void DrawShadedCube(ImDrawList* dl,
                           const ImVec2& regionPos,
                           const ImVec2& regionSize,
                           float yaw,
                           float pitch,
                           float dist)
{
    if (!dl) return;
    if (regionSize.x < 10.0f || regionSize.y < 10.0f) return;

    // Slight gradient background so the viewport doesn't feel 'pitch black'
    dl->AddRectFilledMultiColor(
        regionPos,
        ImVec2(regionPos.x + regionSize.x, regionPos.y + regionSize.y),
        IM_COL32(95, 98, 102, 255),  // TL
        IM_COL32(95, 98, 102, 255),  // TR
        IM_COL32(60, 63, 68, 255),   // BR
        IM_COL32(60, 63, 68, 255)    // BL
    );


        // Cube verts (unit cube centered)
        std::array<V3, 8> P = { {
            v3(-1,-1,-1), v3(1,-1,-1), v3(1, 1,-1), v3(-1, 1,-1),
            v3(-1,-1, 1), v3(1,-1, 1), v3(1, 1, 1), v3(-1, 1, 1)
        } };

        // Transform: orbit rotations
        std::array<V3, 8> W;
        for (int i = 0;i < 8;i++)
        {
            V3 p = P[i];
            p = rotateY(p, yaw);
            p = rotateX(p, pitch);
            W[i] = p;
        }

        // Simple perspective projection
        const float fov = 55.0f * 3.1415926f / 180.0f;
        const float f = 1.0f / std::tan(fov * 0.5f);
        const float aspect = regionSize.x / regionSize.y;

        auto project = [&](V3 p)->std::pair<V2, float>
            {
                // camera at (0,0,dist) looking at origin
                V3 v = add(p, v3(0, 0, dist));
                float z = std::max(0.01f, v.z);
                float ndcX = (v.x * f / aspect) / z;
                float ndcY = (v.y * f) / z;

                // to screen
                float sx = regionPos.x + (ndcX * 0.5f + 0.5f) * regionSize.x;
                float sy = regionPos.y + (-ndcY * 0.5f + 0.5f) * regionSize.y;
                return { {sx, sy}, z };
            };

        struct Tri
        {
            ImVec2 a, b, c;
            ImU32 ca, cb, cc;
            float depth;
        };

        // Faces (as 2 triangles each) with vertex indices
        const int F[6][4] = {
            {0,1,2,3}, // back  (-Z)
            {4,5,6,7}, // front (+Z)
            {0,1,5,4}, // bottom(-Y)
            {3,2,6,7}, // top   (+Y)
            {1,2,6,5}, // right (+X)
            {0,3,7,4}  // left  (-X)
        };

        // Different base color per face (CAD-ish, readable under shading)
        const ImU32 faceBaseCol[6] = {
            IM_COL32(220,  80,  80, 255), // back  (-Z)  red
            IM_COL32(90, 190,  90, 255), // front (+Z)  green
            IM_COL32(90, 140, 230, 255), // bottom(-Y)  blue
            IM_COL32(230, 200,  90, 255), // top   (+Y)  yellow
            IM_COL32(90, 210, 210, 255), // right (+X)  cyan
            IM_COL32(200,  90, 210, 255)  // left  (-X)  magenta
        };
        const V3 lightDir = norm(v3(-0.4f, 0.7f, 0.55f));

        std::vector<Tri> tris;
        tris.reserve(12);

        // Camera is effectively at (0,0,-dist) because we add +dist in projection.
        const V3 cameraPos = v3(0.0f, 0.0f, -dist);

        // Build tris with stronger CAD-like lighting + correct backface cull.
        // This produces a SOLID cube (no "see-through" back faces) and nicer shading.
        for (int fi = 0; fi < 6; fi++)
        {
            V3 p0 = W[F[fi][0]];
            V3 p1 = W[F[fi][1]];
            V3 p2 = W[F[fi][2]];
            V3 p3 = W[F[fi][3]];

            V3 n = norm(cross(sub(p1, p0), sub(p2, p0)));
            V3 c = mul(add(add(p0, p1), add(p2, p3)), 0.25f); // face center
            V3 v = norm(sub(cameraPos, c));                   // face->camera

            // Backface cull (keep only faces oriented toward the camera)
            float ndotv = dot(n, v);
            if (ndotv <= 0.0f)
                continue;

            // Lighting (hemisphere + key + headlight + rim + spec) + per-vertex colors.
        // We keep face-level backface culling for "solid" behavior, but shade per-vertex for nicer gradients.
        for (int fi = 0; fi < 6; fi++)
        {
            V3 p0 = W[F[fi][0]];
            V3 p1 = W[F[fi][1]];
            V3 p2 = W[F[fi][2]];
            V3 p3 = W[F[fi][3]];

            V3 n = norm(cross(sub(p1, p0), sub(p2, p0)));
            V3 c = mul(add(add(p0, p1), add(p2, p3)), 0.25f); // face center
            V3 vFace = norm(sub(cameraPos, c));               // face->camera

            // Backface cull (keep only faces oriented toward the camera)
            float ndotv_face = dot(n, vFace);
            if (ndotv_face <= 0.0f)
                continue;

            // Per-vertex "smoothed" normals (corner direction) for gradient shading.
            V3 nv0 = norm(p0);
            V3 nv1 = norm(p1);
            V3 nv2 = norm(p2);
            V3 nv3 = norm(p3);

            auto vertexColor = [&](const V3& nv, const V3& wp, ImU32 base, float u, float v) -> ImU32
            {
                // Procedural "material" to make the cube pop without a GPU texture.
                // u/v expected in 0..1. We'll tile it a bit.
                auto fract = [](float x) { return x - std::floor(x); };
                auto hash2 = [&](float x, float y)
                {
                    // cheap deterministic hash -> 0..1
                    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
                    return fract(h);
                };

                float uu = u * 6.0f;   // tiling
                float vv = v * 6.0f;

                int cx = (int)std::floor(uu);
                int cy = (int)std::floor(vv);
                float checker = ((cx ^ cy) & 1) ? 1.0f : 0.0f;

                // "Brushed" micro-stripes + subtle noise
                float stripes = 0.5f + 0.5f * std::sin(uu * 22.0f + vv * 3.0f);
                float noise = (hash2(uu, vv) - 0.5f) * 0.12f;

                // Combined albedo modulation (keeps face colour but adds texture)
                float tex = 1.0f
                    + (checker - 0.5f) * 0.10f
                    + (stripes - 0.5f) * 0.06f
                    + noise;
                tex = ImClamp(tex, 0.75f, 1.25f);

                // Decode base colour
                int r = (base >> IM_COL32_R_SHIFT) & 0xFF;
                int g = (base >> IM_COL32_G_SHIFT) & 0xFF;
                int b = (base >> IM_COL32_B_SHIFT) & 0xFF;

                float rf = (float)r * tex;
                float gf = (float)g * tex;
                float bf = (float)b * tex;

                // Repack base with texture baked in (alpha forced opaque)
                ImU32 baseTextured = IM_COL32((int)ImClamp(rf, 0.0f, 255.0f),
                                             (int)ImClamp(gf, 0.0f, 255.0f),
                                             (int)ImClamp(bf, 0.0f, 255.0f),
                                             255);

                // Lighting
                V3 vv3 = norm(sub(cameraPos, wp)); // vertex->camera
                float ndotv = ImClamp(dot(nv, vv3), 0.0f, 1.0f);
                float ndotl = ImClamp(dot(nv, lightDir), 0.0f, 1.0f);

                // Hemisphere: "sky" when normal points up, "ground bounce" when down
                float up = (nv.y * 0.5f + 0.5f);   // 0..1
                float sky = 0.78f * up;
                float ground = 0.34f * (1.0f - up);

                // Key + fill
                float ambient = 0.16f;
                float diffuse = 0.70f * ndotl;
                float head = 0.70f * ndotv;

                // Rim light: silhouette pop
                float rim = std::pow(1.0f - ndotv, 2.0f) * 0.33f;

                // Specular (Blinn-Phong) — lowered exponent for a larger, more visible highlight
                V3 h = norm(add(lightDir, vv3));
                float spec = std::pow(ImClamp(dot(nv, h), 0.0f, 1.0f), 38.0f) * 0.85f;

                float k = ambient + diffuse + head + sky + ground + rim;
                k = ImClamp(k + spec, 0.95f, 1.85f);
                k = std::sqrt(k);

                return shade(baseTextured, k);
            };

            auto [s0, z0] = project(p0);
            auto [s1, z1] = project(p1);
            auto [s2, z2] = project(p2);
            auto [s3, z3] = project(p3);

            // Face-local UVs from the *unrotated* cube (so the pattern sticks to the cube as it rotates).
            V3 lp0 = P[F[fi][0]];
            V3 lp1 = P[F[fi][1]];
            V3 lp2 = P[F[fi][2]];
            V3 lp3 = P[F[fi][3]];

            auto faceUV = [&](int faceIndex, const V3& lp) -> std::pair<float, float>
            {
                float u = 0.0f, v = 0.0f;
                switch (faceIndex)
                {
                case 0: // back  (-Z)  map X/Y
                case 1: // front (+Z)
                    u = lp.x; v = lp.y; break;
                case 2: // bottom(-Y)  map X/Z
                case 3: // top   (+Y)
                    u = lp.x; v = lp.z; break;
                case 4: // right (+X)  map Z/Y
                case 5: // left  (-X)
                    u = lp.z; v = lp.y; break;
                default:
                    u = lp.x; v = lp.y; break;
                }
                // [-1..1] -> [0..1]
                u = u * 0.5f + 0.5f;
                v = v * 0.5f + 0.5f;
                return { u, v };
            };

            auto [u0, v0] = faceUV(fi, lp0);
            auto [u1, v1] = faceUV(fi, lp1);
            auto [u2, v2] = faceUV(fi, lp2);
            auto [u3, v3] = faceUV(fi, lp3);

            ImU32 c0 = vertexColor(nv0, p0, faceBaseCol[fi], u0, v0);
            ImU32 c1 = vertexColor(nv1, p1, faceBaseCol[fi], u1, v1);
            ImU32 c2 = vertexColor(nv2, p2, faceBaseCol[fi], u2, v2);
            ImU32 c3 = vertexColor(nv3, p3, faceBaseCol[fi], u3, v3);

            Tri t1;
            t1.a = s0; t1.b = s1; t1.c = s2;
            t1.ca = c0; t1.cb = c1; t1.cc = c2;
            t1.depth = (z0 + z1 + z2) / 3.0f;

            Tri t2;
            t2.a = s0; t2.b = s2; t2.c = s3;
            t2.ca = c0; t2.cb = c2; t2.cc = c3;
            t2.depth = (z0 + z2 + z3) / 3.0f;

            tris.push_back(t1);
            tris.push_back(t2);

            // Subtle outline for each visible face
            const ImU32 edge = IM_COL32(20, 20, 20, 170);
            dl->AddLine(s0, s1, edge, 1.0f);
            dl->AddLine(s1, s2, edge, 1.0f);
            dl->AddLine(s2, s3, edge, 1.0f);
            dl->AddLine(s3, s0, edge, 1.0f);
        }

std::sort(tris.begin(), tris.end(), [](const Tri& A, const Tri& B) { return A.depth > B.depth; });

        for (auto& t : tris)
            AddTriColored(dl, t.a, t.ca, t.b, t.cb, t.c, t.cc);
            // Draw edges
        const ImU32 edgeCol = IM_COL32(30, 30, 35, 220);
        const float edgeThick = 1.3f;

        auto drawEdge = [&](int i0, int i1)
            {
                auto [s0, z0] = project(W[i0]);
                auto [s1, z1] = project(W[i1]);
                dl->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), edgeCol, edgeThick);
            };

        // cube edges
        drawEdge(0, 1); drawEdge(1, 2); drawEdge(2, 3); drawEdge(3, 0);
        drawEdge(4, 5); drawEdge(5, 6); drawEdge(6, 7); drawEdge(7, 4);
        drawEdge(0, 4); drawEdge(1, 5); drawEdge(2, 6); drawEdge(3, 7);
    }

    }
}

namespace adapters {

void ImGuiCubeViewportRenderer::render(const ports::ViewportRect& vp,
                                       const ports::ViewportCamera& cam,
                                       const ports::ViewportRenderContext& ctx)
{
    const ImVec2 regionPos(vp.pos.x, vp.pos.y);
    const ImVec2 regionSize(vp.size.x, vp.size.y);
    DrawShadedCube(ctx.backgroundDrawList, regionPos, regionSize, cam.yaw, cam.pitch, cam.dist);
}

} // namespace adapters
