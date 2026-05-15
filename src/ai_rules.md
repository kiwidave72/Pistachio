# AI_RULES.md — Rules for AI-Assisted Development

This file defines **mandatory rules and expectations** for any AI tool used on this repository (ChatGPT, Copilot, local LLMs, etc.).

The goal is to ensure:
- Architectural consistency
- Fast iteration
- No accidental coupling
- No domain pollution
- Long-term evolvability

These rules override convenience, speed hacks, or speculative design.

---

## 1. Architectural Non-Negotiables

### Adapter-First Rule
- The **core domain must not depend on adapters**
- Adapters may depend on the domain
- Adapters must not depend on each other

If a change violates this rule, it is invalid.

---

### Domain Purity
The domain layer:
- Must contain **no UI, rendering, OpenGL, ImGui, filesystem, OS, CLI, or slicer logic**
- Must be deterministic and replayable
- Must only describe *definitions*, never generated geometry

If a concern can be implemented as an adapter, it must be.

---

## 2. Compilation & Iteration Discipline

- Prefer **small, frequently compilable changes**
- Avoid large refactors unless explicitly requested
- Introduce new behavior behind adapters first
- Assume rebuild speed matters more than abstraction elegance

AI should bias toward **incremental, reversible changes**.

---

## 3. Feature & Geometry Rules

- Features describe *how geometry is created*
- Features do **not** own geometry
- Geometry is produced during a build phase only

Never store meshes, triangles, or generated solids in the domain.

---

## 4. Sketching & Constraints

- Constraints are first-class domain objects
- Solvers may be imperfect, but must be:
  - Deterministic
  - Predictable
  - Diagnosable

AI must not:
- Introduce solver “magic”
- Hide constraint failures
- Silently ignore over-constraints

If behavior is undefined, it must be documented as such.

---

## 5. Rendering & Visualization

Rendering:
- Is an adapter
- Must never mutate domain state
- Must tolerate incomplete or invalid models

Visual correctness must not be used to justify domain hacks.

---

## 6. CLI Tools & Slicing

- All slicers are accessed via **CLI adapters**
- No slicer-specific logic in the domain
- A/B slicing comparison is a first-class goal

AI must not bake assumptions about any slicer into core logic.

---

## 7. File Formats & Persistence

- Persistence formats are unstable by design
- Backward compatibility is *not* guaranteed
- Explicit breaking changes are acceptable

AI must not promise long-term file stability.

---

## 8. Naming & Scope Discipline

- Prefer precise, boring names over clever ones
- Avoid speculative abstractions
- Do not generalize early “just in case”

If a concept does not yet exist in the domain, do not invent it.

---

## 9. Documentation Rules

When modifying or generating documentation, AI must:
- Reflect **current behavior**, not aspirations
- Clearly label planned or incomplete features
- Avoid marketing language

Docs should describe reality, not intent.

---

## 10. When in Doubt

If unclear:
- Ask questions
- Propose multiple options
- Default to the **simplest adapter-based solution**

Never assume hidden requirements.

---

**Violation of these rules should be treated as a bug.**
