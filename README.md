# C++ 2D Sketching & Constraint Solver

This application is a **2D sketching and constraint-based geometry system** written in modern C++.
It is being developed as the foundation for a future parametric CAD and 3D modeling environment.

The system follows a **domain-driven design** approach:
- Geometry, constraints, and solver logic live in the core domain
- UI, rendering, and input tools are adapters
- The solver is intentionally simple, explicit, and testable

This document describes **what the application can do today**.

---

## 1. Domain Model Overview

### Sketch
A sketch is a container for:
- 2D geometric entities
- Geometric and dimensional constraints
- Command history (undo / redo)
- Change serials for solver invalidation

Each sketch has a stable identity and is solved incrementally.

---

### Geometric Entities

| Entity | Description |
|------|-------------|
| Point | 2D point (x, y) |
| Line | Infinite line defined by two endpoints |
| Circle | Center point + radius |

**Important domain rule:**  
Lines are treated as **infinite** for constraint solving. Rendering clips them only for display.

---

## 2. Tools

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
2. Pick required entities (highlighted)
3. Constraint commits
4. Solver runs
5. Geometry updates immediately

---

## 3. Constraints

Constraints are first-class domain objects stored in the sketch.

### Fixed
Locks an entity’s parameters.

Supported on:
- Point
- Line
- Circle

Rule:
- Fixed entities are not modified by the solver

Limit:
- Entire entity only (no partial DOF locking)

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
y1 = y2

Status: Working

---

### Vertical (Not working)
Intended rule:
x1 = x2

Status:
- Constraint stored
- Solver does not modify geometry

---

### Parallel (Not working)
Intended rule:
direction(line1) ∥ direction(line2)

Status:
- Constraint stored
- Solver does not modify geometry

---

### Tangent
Supported:
- Line–Circle
- Circle–Circle (external)

Rules:
distance(center, line) = radius  
distance(c1, c2) = r1 + r2

Limits:
- Infinite lines
- External tangency only
- One-entity motion only

---

## 4. Constraint Status Matrix

| Constraint | Entities | Solver Effect | Status |
|----------|----------|---------------|--------|
| Fixed | P, L, C | Locks entity | Working |
| Coincident | P–P, P–L, P–C | Forces coincidence | Working |
| Horizontal | Line | Forces horizontal | Working |
| Vertical | Line | No effect | Not working |
| Parallel | Line–Line | No effect | Not working |
| Tangent | L–C, C–C | Forces tangency | Working |

---

## 5. Solver Responsibility Matrix

| Responsibility | Guaranteed | Not Guaranteed |
|---------------|------------|---------------|
| Determinism | Yes | Global optimality |
| Constraint order | Sequential | Order independence |
| Fixed respect | Full entity | Partial DOF |
| Conflict detection | No | Yes |
| Over-constraint detection | No | Yes |
| DOF analysis | No | Yes |
| Motion distribution | Single entity | Balanced motion |

---

## 6. Known Issues

- Vertical constraint is stored but does not modify geometry
- Parallel constraint is stored but does not modify geometry
- No constraint conflict detection
- No DOF analysis
- Lines treated as infinite for solving

---

This document intentionally describes **current behavior only**.
