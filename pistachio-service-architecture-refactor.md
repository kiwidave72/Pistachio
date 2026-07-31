# Pistachio Service Architecture Refactor

Status: design complete, not yet started
Owner: Dave
Last updated: 2026-07-31

## Why

Two goals are driving this:

1. **Headless mode** — run `pistachio_core` plus a set of `IServiceModule`
   plugins (slicer logic, asset library, etc.) with **no** `IUiModule`
   plugins loaded at all. No ImGui, no GLFW window, no render loop. This
   becomes the test harness for exercising the slicer service directly.
2. **Clean separation of UI from logic** at the *plugin* level, so a
   `slicer_editor` UI plugin can exist alongside (or be swapped for) other
   UI plugins, all talking to the same underlying services.

### Confirmed starting point

`pistachio_core`'s CMake target is **already** ImGui-free today — verified
directly against the CMakeLists source list, not assumed:

```
add_library(pistachio_core SHARED
    ${DOMAIN_SOURCES}
    src/core/Application.cpp
    src/core/ConfigStore.cpp
    src/core/TaskRunner.cpp
    src/adapters/rendering/OcctRenderer.cpp
    src/adapters/loaders/...
    src/adapters/exporters/...
    src/adapters/persistence/...
    src/adapters/slicers/planarSlicer.cpp
    ${SOLVER_SOURCES}
)
```

None of `adapters/ui/*` is in this list. `ImGuiHost.cpp`, `ImGuiAdapter.cpp`,
etc. compile into the main executable or into `pistachio_ui`, never into
`pistachio_core`. The `adapters/ui/` folder living next to
`adapters/loaders/` etc. in the source tree is a **folder layout**
observation, not a build-dependency one — the real separation already
exists at the target level.

**What still needs work is entirely at the plugin/adapter layer:**
1. `SlicerCorePlugin.cpp` mixes logic and UI in one file (see breakdown
   below).
2. `ContributionRegistry` is ImGui-coupled by design (documented in its own
   header comment: "Include in ImGuiHost.cpp only — depends on ImGui").

### Confirmed shape of `SlicerCorePlugin.cpp` (1,485 lines)

Checked directly — the file is already halfway split, just not physically:

| Range | Contents | ImGui calls |
|---|---|---|
| Lines 1–575 | `SceneController`, `SnapshotCommand`, `TestCommand`, `IWorkspaceService`/`WorkspaceService`, `ISlicerService`/`SlicerService` | **0** |
| Lines 576–1485 | `SlicerCorePlugin` (`IUiModule`): `onLoad`, `render`, tree view, properties panel | **118** (all of them) |

`BuildPlateRenderer.cpp/h` (2,677 + 436 lines) is unambiguously
rendering/GL/ImGui — stays in the UI plugin wholesale, no splitting needed
inside it.

---

## Design

### 1. Service Registry

Owned by `Application`, resolved by every plugin (service or UI) the same
way.

```cpp
class ServiceRegistry
{
public:
    template<typename TInterface>
    void registerService(TInterface* service) { m_services[typeid(TInterface)] = service; }

    template<typename TInterface>
    TInterface* resolve() const
    {
        auto it = m_services.find(typeid(TInterface));
        return (it != m_services.end()) ? static_cast<TInterface*>(it->second) : nullptr;
    }

private:
    std::unordered_map<std::type_index, void*> m_services;
};
```

Decisions:
- **Single-winner registration for v1** — no priority/override logic yet.
  API shaped so `resolveAll<T>()` can be added later without breaking
  callers (needed eventually for A/B slicer testing — see Future Work).
- **Late-bound resolution** — plugins hold the registry/app pointer and
  call `resolve<T>()` at point of use, every time. Never cache a resolved
  pointer long-term. This is what makes hot-reload safe: a service being
  swapped mid-session doesn't leave consumers holding dangling pointers.

### 2. `IApplication`

Narrow interface plugins depend on, instead of casting to the concrete
`Application` class.

```cpp
namespace core
{
    class IApplication
    {
    public:
        virtual ~IApplication() = default;
        virtual ServiceRegistry& services() = 0;
    };
}
```

`Application` implements it. `Application::registerCoreServices()` (new
method) registers `ConfigStore`, `TaskRunner`, and the new `EventBus` into
its own `m_services`, called once at startup before any plugin loads.

`ContributionRegistry` is owned by `ImGuiHost`, not `Application` — it gets
registered from `main.cpp`/`HotReloadUiAdapter`, not from
`Application::registerCoreServices()`, since `Application` has no reference
to it. (Ownership of `ContributionRegistry` itself is a known, deferred
smell — see "Deferred / explicitly not doing now" below.)

### 3. `UiHostServices` — additive change only

No breaking changes. Existing fields (`app`, `config`, `registry`,
`guiHost`, `taskRunner`) stay exactly as they are. One new field added:

```cpp
struct UiHostServices
{
    // existing fields, unchanged
    void* app;
    void* config;
    void* registry;
    void* guiHost;
    void* taskRunner;

    // new
    core::IApplication* application = nullptr;
};
```

Populated alongside the existing fields in `HotReloadUiAdapter::initialize()`:
```cpp
m_svc.application = m_app;   // same object as m_svc.app, typed as IApplication*
```

Plugins migrate to the new pattern (`svc.application->services().resolve<T>()`)
opportunistically, not all at once.

### 4. `IServiceModule` — new plugin kind

```cpp
class IServiceModule
{
public:
    virtual ~IServiceModule() = default;
    virtual void onLoad(core::IApplication& app) = 0;
    virtual void onUnload(core::IApplication& app) = 0;
    // no render() — services never touch ImGui
};
```

Distinct from `IUiModule` (which keeps `render()`). Plugin loader
distinguishes kind by which interface a plugin's factory returns.

### 5. Two-phase plugin loading

```
Phase 1 — Registration: load every IServiceModule plugin, call onLoad()
          (each one registers its services into app.services())
Phase 2 — UI: load every IUiModule plugin, call onLoad()
          (each one resolves whatever services it needs)
```

Guarantees no UI plugin resolves before a service it depends on has
registered. Combined with late-binding, edge cases degrade gracefully
(a `nullptr` resolve now, succeeding next frame) rather than failing hard.

**This is what makes headless mode possible**: headless just runs Phase 1
and stops — no Phase 2 at all.

### 6. `IContributionRegistry` — interface split

`ContributionRegistry.h` currently includes `<imgui.h>`/`<imgui_internal.h>`
directly and is documented as ImGui-only. Since it's header-only with
inline methods, **including the header at all requires ImGui on the
include path**, regardless of which methods are actually called. This
blocks service plugins from registering menu/ribbon/drag-drop
contributions without linking ImGui.

Fix: split registration (data-only) from rendering (ImGui-coupled):

```cpp
// ports/IContributionRegistry.h — zero ImGui, safe for service plugins
class IContributionRegistry
{
public:
    virtual ~IContributionRegistry() = default;
    virtual ports::MenuContribution* contributeMenu(const std::string&, const std::string&, int priority = 100) = 0;
    virtual ports::RibbonContribution* contributeRibbon(const std::string&, const std::string&, int priority = 100) = 0;
    virtual ports::DragDropContribution* contributeDragDrop(const std::string&, const std::string&, int priority = 100) = 0;
    virtual void removeMenuContributions(const std::string&) = 0;
    virtual void removeRibbonContributions(const std::string&) = 0;
    virtual void removeDragDropContributions(const std::string&) = 0;
};
```

`adapters::ContributionRegistry` implements it, keeping
`renderMenuBar()`/`renderRibbonBar()`/`renderDragDropTargets()` exactly as
they are (still ImGui-coupled, still only called from UI-side code).

### 7. Event Bus

New system. Owned by `Application`, registered into the service registry
like everything else.

```cpp
class IEventBus
{
public:
    virtual ~IEventBus() = default;
    virtual void publish(const std::string& eventKey, const std::string& payload) = 0;
    virtual void subscribe(const std::string& eventKey, std::function<void(const std::string&)> handler) = 0;
};
```

Decisions:
- **v1 payloads are strings** — data-model keys/ids, or plain
  error/warning messages. Deliberately avoids raw struct pointers crossing
  the DLL boundary (layout-mismatch risk across independently
  hot-reloaded DLLs). Richer payloads can be considered later.
- **Both direct service calls (via `resolve<T>()`) and events are allowed
  side by side.** No forced pattern yet. Expected convention once there's
  real usage: request/response → direct call, notification/broadcast →
  event. Not being decided prematurely.

### 8. Headless test target

Purpose: **test harness**, not a product feature. Loads Phase 1 only,
exercises the slicer service directly, no window, no ImGui, no GLFW.

```cpp
// new small executable, own main(), own CMake target
int main()
{
    auto app = std::make_unique<core::Application>();
    app->registerCoreServices();

    ServiceModuleLoader loader;
    loader.loadAll(app->services(), *app->getDataContext());

    auto* slicerService = app->services().resolve<ISlicerService>();
    // exercise directly — load workspace, arrange, export, assert, etc.

    loader.unloadAll(app->services(), *app->getDataContext());
}
```

CMake target explicitly does **not** link `imgui`/`glfw`/`walnut`. If it
ever fails to link because something service-side pulled in ImGui
transitively, that's the split breaking — caught at build time.

---

## Migration plan (grounded in actual file sizes)

| Step | Scope | Effort | Notes |
|---|---|---|---|
| 1 | `ServiceRegistry` + `IApplication` + `Application::registerCoreServices()` | Small | Fully designed above, mechanical |
| 2 | `UiHostServices.application` field, wire-up in `HotReloadUiAdapter` | Trivial | Additive only |
| 3 | Two-phase loader change in `UiPluginRegistry`/`UiPluginLoader` | Small–Medium | Needs care, contained blast radius |
| 4 | `IContributionRegistry` split | Small | One new header, no behavior change |
| 5 | New `pistachio_slicer_service` plugin: move `WorkspaceService`, `SlicerService`, `SnapshotCommand`, `TestCommand`, `SceneController` (lines 1–575 of `SlicerCorePlugin.cpp`) into it, implementing `IServiceModule` | Medium | Code is already ImGui-free — this is relocation + construction wiring, not a rewrite |
| 6 | New `slicer_editor` plugin: relocate `SlicerCorePlugin`'s UI half (lines 576–1485), `BuildPlateRenderer`, `NavigationManager`, `SelectionManager`. `onLoad()` changes from directly constructing `WorkspaceService`/`SlicerService` to resolving them via the service registry | Medium | Biggest behavioral-risk item — see below |
| 7 | Headless test target | Small | Validates 1–6 actually worked |

**Overall: ~2–4 sessions.** The hard analytical work (confirming the logic
classes have zero ImGui coupling) is already done — see the confirmed file
breakdown above.

**Biggest risk:** `SlicerCorePlugin::onLoad()` currently constructs
`WorkspaceService`/`SlicerService` directly and wires them to
`ModelCache`/`Workspace` in one intertwined sequence. Untangling "who
constructs what, in what order" once this is split across two DLLs with
two-phase loading is the part most likely to surface a surprise. Budget
buffer time specifically for this step, not the mechanical moves.

---

## Deferred / explicitly not doing now

- **`ContributionRegistry` ownership** currently lives on `ImGuiHost` as a
  value member (`adapters::ContributionRegistry m_registry;`), constructed
  implicitly as part of `HotReloadUiAdapter`'s construction. Real fix
  (inject via constructor, own it at `main()` level) identified but
  explicitly deferred — not touching `ContributionRegistry`/`ImGuiHost`
  structurally right now. If needed later: construct in `main()`, pass by
  reference into `HotReloadUiAdapter` → `ImGuiHost`'s constructor.
- **Multi-implementation service resolution** (`resolveAll<T>()`, needed
  for A/B slicer comparison) — API shaped to allow adding this later, not
  building it now.
- **Asset library as its own service plugin** — real, wanted, but scoped
  as a *follow-up* once the mechanism above is proven with something
  smaller first. Don't build the mechanism and migrate a real feature onto
  it in the same pass.
- **Forcing all cross-plugin communication through events** — direct
  registry-resolved calls remain allowed. No rule imposed yet.

---

## Open items unrelated to this refactor (tracked separately)

Carried over from other sessions, not part of this doc's scope:
- Shutdown crash (`0xc0000005`) on plugin unload
- Workspace JSON load not repopulating `ModelCache` (blank render on load)
- `.gitignore`: `!_deps/` currently un-ignores `_deps/` — likely should be
  `_deps/` (no `!`) if the intent is to actually ignore it
