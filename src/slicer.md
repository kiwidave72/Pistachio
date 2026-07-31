# Kinetica Architecture

## Overview

Kinetica is a Spatial Motion Engine.

Unlike traditional slicers, Kinetica does not treat G-code as the primary output.

Instead, Kinetica converts geometry into machine-independent motion representations which can later be compiled into:

- Planar G-code
- Non-planar toolpaths
- 5-axis toolpaths
- Streaming motion systems
- Simulation backends

The architecture follows a compiler-style pipeline:

```text
CAD
 ↓
Geometry
 ↓
Topology
 ↓
Motion
 ↓
Backend
```

---

# Core Concepts

## Project

Top-level document.

Contains:

- Asset Library
- Build Plates
- Shared Profiles
- Generated Outputs

```text
Project
├── Assets
├── Build Plates
├── Profiles
└── Outputs
```

---

## Build Plate

Primary unit of work.

Every Kinetica operation runs against a single build plate.

```text
Build Plate
├── Model A
├── Model B
└── Model C
```

A project may contain multiple build plates.

Each build plate can generate independent output.

---

## Model

Represents imported geometry.

Supported formats:

- STL
- STEP

A model may contain one or more geometries.

---

## Geometry

Triangulated mesh data.

```cpp
Geometry
{
    Vertices
    Triangles
}
```

All slicing operations ultimately work against geometry.

---

# Pipeline Overview

```text
Phase 0  Import
Phase 1  Geometry Normalization
Phase 2  Spatial Acceleration
Phase 3  Slice Field Evaluation
Phase 4  Segment Extraction
Phase 5  Topology Reconstruction
Phase 6  Toolpath IR Generation
Phase 7  Motion Planning
Phase 8  Motion Validation
Phase 9  Backend Compilation
```

---

# Phase 0 — Import

## Goal

Load external CAD files.

## Inputs

- STL
- STEP

## Output

```text
Model
 └── Geometry
```

## Tasks

### STL

- Load triangles
- Create Geometry

### STEP

- Load OCCT shape
- Tessellate surfaces
- Create Geometry

## Deliverables

- ImportService
- STLImporter
- STEPImporter

---

# Phase 1 — Geometry Normalization

## Goal

Prepare geometry for processing.

## Input

```text
Geometry
```

## Output

```text
Normalized Geometry
```

## Tasks

### Remove Degenerate Triangles

Remove triangles with:

- Zero area
- Duplicate vertices

### Validate Indices

Ensure:

```cpp
triangle[i] < vertexCount
```

### Compute Bounding Box

Store:

```cpp
bboxMin
bboxMax
```

### Optional

- Weld duplicate vertices
- Repair winding

## Deliverables

- GeometryValidator
- BoundingBoxCalculator

---

# Phase 2 — Spatial Acceleration

## Goal

Provide fast geometry lookup.

## Input

```text
Normalized Geometry
```

## Output

```text
Acceleration Structure
```

## Initial Implementation

### Z-Buckets

Each bucket contains:

```cpp
Bucket
{
    zMin
    zMax

    Triangles[]
}
```

## Future

### BVH

Bounding Volume Hierarchy.

## Deliverables

- ZBucketBuilder
- ZBucketIndex

---

# Phase 3 — Slice Field Evaluation

## Goal

Determine where geometry intersects a sampling field.

## Input

```text
Acceleration Structure
Slice Strategy
```

## Output

```text
Intersection Samples
```

## Slice Strategies

### Planar

```text
Z = Constant
```

### Non-Planar

```text
Surface Following
```

### 5-Axis

```text
Position + Orientation Field
```

## Deliverables

- ISliceStrategy
- PlanarSliceStrategy

Future:

- NonPlanarSliceStrategy
- FiveAxisSliceStrategy

---

# Phase 4 — Segment Extraction

## Goal

Convert intersection samples into segments.

## Input

```text
Intersection Samples
```

## Output

```text
Segment[]
```

Example:

```text
A ----- B
```

## Segment

```cpp
struct Segment
{
    Vec3 start;
    Vec3 end;

    uint32_t modelId;
};
```

## Deliverables

- SegmentExtractor

---

# Phase 5 — Topology Reconstruction

## Goal

Convert segments into meaningful geometry.

## Input

```text
Segment[]
```

## Output

```text
Contours
Regions
Islands
```

## Tasks

### Endpoint Matching

```text
Segment A end
==
Segment B start
```

### Graph Construction

Build connectivity graph.

### Loop Detection

Identify:

```text
Closed Loops
```

### Classification

Determine:

- Outer contour
- Hole
- Island

### Winding Analysis

Determine:

- Clockwise
- Counter-clockwise

## Deliverables

- SegmentGraph
- LoopBuilder
- HoleClassifier

---

# Phase 6 — Toolpath IR Generation

## Goal

Convert geometry into machine-independent motion.

## Input

```text
Contours
```

## Output

```text
Toolpath IR
```

## Why IR Exists

IR = Intermediate Representation.

It separates:

```text
Geometry
```

from

```text
Machine Output
```

This allows:

- G-code generation
- Non-planar execution
- 5-axis execution
- Streaming motion

using the same motion model.

## Core Structures

```cpp
struct ToolpathPoint
{
    Vec3 position;

    Vec3 normal;

    Vec3 direction;

    float extrusion;

    float feedrate;
};
```

```cpp
struct Toolpath
{
    std::vector<ToolpathPoint> points;
};
```

## Deliverables

- ToolpathBuilder
- ToolpathIR

---

# Phase 7 — Motion Planning

## Goal

Optimize motion.

## Input

```text
Toolpath IR
```

## Output

```text
Optimized Toolpath IR
```

## Tasks

### Travel Optimization

Reduce:

```text
Non-print moves
```

### Island Ordering

Determine:

```text
Print order
```

### Path Smoothing

Convert:

```text
Sharp corners
```

into smoother motion.

### Extrusion Planning

Adjust:

```text
Flow
Speed
```

## Deliverables

- MotionPlanner

---

# Phase 8 — Motion Validation

## Goal

Verify the machine can execute the path.

## Input

```text
Optimized Toolpath IR
Machine Profile
```

## Output

```text
Validated Toolpath IR
Warnings
Errors
```

## Checks

### Build Volume

Ensure:

```text
Inside printable area
```

### Collision Detection

Future:

```text
Toolhead
Nozzle
Fan Shroud
```

### Non-Planar Validation

Ensure:

```text
Safe clearances
```

### 5-Axis Validation

Ensure:

```text
Reachability
Orientation limits
```

## Deliverables

- MotionValidator

---

# Phase 9 — Backend Compilation

## Goal

Generate machine-specific output.

## Input

```text
Validated Toolpath IR
```

## Output

Backend-specific representation.

---

## Backend A — Planar G-code

Current target.

### Output

```text
G0
G1
```

### Deliverables

- GCodeWriter

---

## Backend B — Non-Planar

Future.

### Output

```text
3D Motion Trajectories
```

---

## Backend C — 5-Axis

Future.

### Output

```text
Position
Orientation
```

---

## Backend D — Streaming

Future.

### Output

```text
Continuous Motion Stream
```

Example:

```text
X,Y,Z,E,F,T
```

---

# Visual Debugging

Every phase should be inspectable.

## Viewport Modes

### Import

- Mesh
- Wireframe
- Normals

### Acceleration

- Buckets
- BVH

### Slice Evaluation

- Slice Plane
- Intersections

### Segments

- Raw Segments

### Topology

- Loops
- Holes
- Islands

### Toolpath IR

- Travel Moves
- Extrusion Moves

### Backend

- G-code Preview

---

# Long-Term Vision

```text
Project
 ↓
Build Plate
 ↓
Kinetica
 ↓
Toolpath IR
 ↓
Backend
```

Kinetica is not a slicer.

Kinetica is a Spatial Motion Engine that converts geometry into executable motion.