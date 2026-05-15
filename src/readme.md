# C++ Parametric CAD & Slicing Research Platform

This application is a **C++-based parametric CAD and slicing research platform**.
It builds on a 2D sketching and constraint-based geometry system and is evolving toward full **feature-based solid modeling** and **CLI-driven slicing workflows**.

The system is intentionally designed for **fast iteration** using an **adapter-first architecture**, allowing the core domain to mature over time while keeping rebuilds quick and experimentation safe.

This document describes **current behavior, architectural intent, and near-term direction**.

---

## 1. Domain Model Overview

### Core Domain
The core domain contains:
- 2D sketches and geometric entities
- Constraints and constraint solving
- Feature definitions (e.g. sketches, extrusions)
- Deterministic model rebuild logic

The domain:
- Contains **no UI, rendering, OpenGL, ImGui, filesystem, or slicer logic**
- Is deterministic and replayable
- Owns *definitions*, not generated geometry

The domain is expected to become more specific and constrained over time.

---

### Adapter-First Architecture
The system is structured around adapters implemented as plug-ins:
- UI and input handling
- Rendering and visualization
- Mesh generation
- Import/export (STL, STEP)
- CLI slicer integration (PrusaSlicer)

**Rule:**
> The core domain must not depend on adapters.  
> Adapters may depend on the domain, but never on each other.

This enables frequent compilation, rapid experimentation, and clean separation of concerns.

---

## 2. Sketching Model

### Sketch
A sketch is a **feature** and acts as a container for:
- 2D geometric entities
- Constraints
- A user-selected sketch plane

Closed profiles are detected **automatically**.

Infinite-line solving is currently used by the solver as a temporary simplification and will be replaced with finite geometry as the modeling pipeline matures.

Each sketch has a stable identity and is rebuilt deterministically.

---

### Geometric Entities

| Entity | Description |
|------|-------------|
| Point | 2D point (x, y) |
| Line | Line segment (solver currently treats as infinite) |
| Circle | Center point + radius |

---

## 3. Tools

### Selection Tool
- Hit-tests points, lines, and circles
- Hover highlights
- Picked entities remain selected during tool workflows

### Sketch Creation Tools
- Point tool
- Line tool (two-point)
- Circle tool (center + radius)

### Constraint Tools – General Workflow
1. Activate constraint tool
2. Pick required entities
3. Constraint commits
4. Solver runs
5. Geometry updates immediately

---

## 4. Constraints

Constraints are first-class domain objects stored in the sketch.

The solver philosophy prioritizes:
- Robustness
- Diagnostics
- Predictable behavior aligned with Fusion 360

### Fixed
Locks an entity’s parameters.

Supported on:
- Point
- Line
- Circle

Rule:
- Fixed entities are not modified by the solver

---

### Coincident
Forces shared position.

Supported:
- Point–Point
- Point–Line
- Point–Circle

---

### Horizontal
Forces a line horizontal.

Rule:
- y1 = y2

Status: Working

---

### Vertical
Forces a line vertical.

Rule:
- x1 = x2

Status: Partially implemented

---

### Parallel
Forces two lines to be parallel.

Rule:
- direction(line1) ∥ direction(line2)

Status: Partially implemented

---

### Tangent
Supported:
- Line–Circle
- Circle–Circle (external)

Rules:
- distance(center, line) = radius  
- distance(c1, c2) = r1 + r2

Limits:
- External tangency only
- Solver-side simplifications still apply

---

## 5. Solver Responsibility Matrix

| Responsibility | Guaranteed | Not Guaranteed |
|---------------|------------|---------------|
| Determinism | Yes | Global optimality |
| Constraint order | Sequential | Order independence |
| Fixed respect | Full entity | Partial DOF |
| Conflict detection | Planned | Currently incomplete |
| Over-constraint detection | Planned | Currently incomplete |
| DOF analysis | Planned | Currently incomplete |

---

## 6. Feature-Based Modeling

Modeling is **feature-driven**.

- Features describe *how* geometry is created
- Features do **not** own geometry
- Geometry is generated during a build phase

### Initial Features
- Sketch
- Extrude (from closed 2D profiles)

### Planned Features
- Boolean operations (Join, Cut, Intersect)
- Additional feature types as required

Feature history is currently **linear**, with stable feature IDs to allow future reordering.

---

## 7. Mesh Generation & Export

Mesh generation is performed via **mesh adapters**.

- Mesh engines are plug-ins
- Guarantees (watertight, manifold, etc.) will improve over time
- Mesh generation occurs at final build/export stage

### Export Formats
- STL (required)
- STEP (required)

Export functionality is experimental and not yet guaranteed slicer-safe.

---

## 8. Slicing & CLI Integration

Slicing is performed via **CLI slicer adapters**.

- PrusaSlicer is the initial reference slicer
- Integration is currently internal-only
- A/B slicing comparisons are a first-class goal

Future work includes:
- Additional slicer adapters
- G-code visualization tools

---

## 9. Rendering & UI

Rendering is an adapter used for **editing and visualization**.

- The 3D view is intended to become the primary editor
- Sketch overlays are view-aligned and screen-space scaled
- Both orthographic and perspective cameras are supported

Rendering must never modify domain state.

---

## 10. Units & Coordinate System

- Internal units are **millimeters**
- Coordinate system is **right-handed with Z-up**
- Units may become configurable in the future
- Internal tolerances are implementation-defined

---

## 11. Persistence

- File formats may change during development
- Breaking changes are expected
- Backward compatibility is not guaranteed at this stage

---

## 12. Roadmap (High-Level)

### Phase 1 – Adapter-First Foundations
- Stable sketching and constraints
- Feature tree with extrusion
- Experimental mesh adapter
- PrusaSlicer CLI adapter

### Phase 2 – Domain Tightening
- Finite geometry
- Improved solver robustness and diagnostics
- Profile validation
- Boolean operations

### Phase 3 – Slicing & Tooling Research
- A/B slicing workflows
- G-code visualization
- Internal slicer experimentation

---

This document reflects **current behavior and architectural intent**, not a frozen specification.

