# Area Lights (Quad Emitters) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a diffuse quad area light to the BDPT so caustics through specular surfaces become low-variance, while keeping point lights fully working.

**Architecture:** The emitter is real scene geometry — a quad object with an `EMISSIVE` material. The loader auto-derives a sampling `Light` (type `AREA`) from each emissive object. The BDPT's four point-light emission-convention sites become sampled area-light quantities, and a new `s=0` strategy lets camera rays hit the emitter directly.

**Tech Stack:** C++17, rapidjson scene loader, custom BVH raytracer + Veach BDPT. No test framework — verification is build + numeric probe + render, user is final judge.

**Spec:** `docs/superpowers/specs/2026-09-26-area-lights-design.md`

## Global Constraints

- YAGNI / no over-engineering / prefer simple one-liners (user CLAUDE.md). No new abstractions beyond what the spec names.
- Comments terse and code-focused — no conversational or plan-referencing comment blocks (user preference).
- No new `.cpp` files ⇒ no CMake edit (CMake globs `Scene/`+`Utils/`, lists `Pathtracer.cpp`).
- Point-light scenes (`scene2.scene`, `glass_dragon.scene`) must parse and render byte-comparably to their current committed output — POINT code paths untouched.
- Base `SourceCode/Raytracer.cpp` must keep compiling; area lights degrade to their quad centre there.
- Emitted radiance convention: `L_e = intensity / (area · π)`, one-sided (normal side only). `intensity` keeps its current radiant-power meaning.
- Use `PI_HI` (high-precision double PI already in `Pathtracer.cpp`) for all pdf/emission math, not the low-precision global `PI`.
- Build from repo root: `cmake --build build`. Run: `./build/renderer <scene> <out.ppm>`.
- Leave existing BDPT diagnostic scaffolding (`BDPT_CAUSTIC`, `BDPT_SPLIT`, `BDPT_EXPOSURE`, `BDPT_DBG`) in place — orthogonal, cleaned up separately.

## Review Focus

- **Degenerate quad (u∥v, zero area):** loader deriving `area = |u×v| = 0` → division by zero in `emittedRadiance`/pdf. Expected: reject at load with a clear error, not NaN pixels. Pinned in Task 2.
- **Back-facing emitter (normal points away from scene):** a ceiling quad wound so `u×v` points up. Expected: emits nothing (one-sided), does not silently emit downward anyway. Pinned in Task 4 (seeding) and Task 6 (s=0).
- **Camera ray grazes/hits emitter edge-on (cosθ≤0 at emitter):** Expected: zero emission for that hit, no negative/NaN contribution. Pinned in Task 6.
- **Emissive object with >2 or non-planar triangles:** loader assumes a 2-triangle planar quad. Expected: use the first triangle's plane; document the winding/shape assumption; don't crash on extra triangles. Pinned in Task 2.
- **Scene with an AREA light but MIS `s=0` double-count vs NEE:** same emitter sampled by NEE (Task 5) and hit directly (Task 6) must not sum to >1× exposure. Expected: MIS weights across `s=0`/`s=1`/connections sum to 1. Pinned in Task 7.

---

## File Structure

- `SourceCode/Scene/Material.h` — `EMISSIVE` enum value + `Vector emission` field.
- `SourceCode/Scene/Light.h` / `Light.cpp` — `LightType`, quad fields, sampling + emission API.
- `SourceCode/Scene/SceneFactory.h` / `SceneFactory.cpp` — parse emission material; auto-register AREA lights from emissive objects.
- `SourceCode/Pathtracer.h` / `Pathtracer.cpp` — 4 emission-convention sites + `s=0` branch.
- `SourceCode/Raytracer.cpp` — emissive materials return emission on hit.
- `Scenes/cornell_area.scene` — new verification scene (quad ceiling light).

---

## Task 1: `EMISSIVE` material type + emission field

**Files:**
- Modify: `SourceCode/Scene/Material.h`

**Interfaces:**
- Produces: `MaterialType::EMISSIVE`; `Material::emission` (Vector, radiance colour).

- [ ] **Step 1: Add enum value and field**

In `SourceCode/Scene/Material.h`:
```cpp
enum class MaterialType {
    DIFFUSE,
    REFLECTIVE,
    REFRACTIVE,
    EMISSIVE
};

struct Material {
    MaterialType type;
    bool constantAlbedo;
    Vector albedo;
    std::shared_ptr<Texture> texture;
    bool smoothShading;
    float ior;
    Vector emission; // radiance colour when type == EMISSIVE; else unused
};
```

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build build 2>&1 | tail -3`
Expected: builds clean (adding an unused enum value + POD field is non-breaking).

- [ ] **Step 3: Commit**

```bash
git add SourceCode/Scene/Material.h
git commit -m "Add EMISSIVE material type and emission field"
```

---

## Task 2: `Light` gains type + quad geometry + sampling API

**Files:**
- Modify: `SourceCode/Scene/Light.h`, `SourceCode/Scene/Light.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `enum class LightType { POINT, AREA };`
  - `Light(const Vector& position, float intensity)` — existing POINT ctor, unchanged.
  - `Light(const Vector& corner, const Vector& u, const Vector& v, const Vector& emission, float intensity)` — AREA ctor (computes `normal`, `area`; throws `std::logic_error` if `area <= EPSILON`).
  - `LightType getType() const;`
  - `const Vector& getPosition() const;` — POINT: the point; AREA: quad centre.
  - `float getIntensity() const;`
  - `Vector samplePoint(float& pdfArea) const;` — uniform on quad, sets `pdfArea = 1/area`. Undefined for POINT (assert type==AREA).
  - `const Vector& getNormal() const;` — AREA only.
  - `float getArea() const;`
  - `Vector emittedRadiance() const;` — `emission * (intensity / (area * PI))` for AREA.
  - `int getEmissiveMaterialIndex() const;` + field `int emissiveMaterialIndex = -1;` — set by Task 3 registration so Task 6 can map an emissive hit back to its `L_e`. (Surfaced by Task 6; add here so `Light` is edited once.)

- [ ] **Step 1: Rewrite `Light.h`**

```cpp
#pragma once
#include "../Utils/Vector.h"

enum class LightType { POINT, AREA };

class Light {
    LightType type;
    Vector position;   // POINT: point; AREA: quad corner
    float intensity;
    Vector u, v;       // AREA edge vectors
    Vector normal;     // AREA derived, normalize(u x v)
    Vector emission;   // AREA emission colour
    float area;        // AREA derived, |u x v|
    Vector center;     // AREA derived, position + (u+v)/2
public:
    Light(const Vector& position, float intensity);                 // POINT
    Light(const Vector& corner, const Vector& u, const Vector& v,
          const Vector& emission, float intensity);                 // AREA

    LightType getType() const;
    const Vector& getPosition() const;
    float getIntensity() const;

    Vector samplePoint(float& pdfArea) const;
    const Vector& getNormal() const;
    float getArea() const;
    Vector emittedRadiance() const;
};
```

- [ ] **Step 2: Rewrite `Light.cpp`**

```cpp
#include "Light.h"
#include <stdexcept>

Light::Light(const Vector& position, float intensity)
    : type(LightType::POINT), position(position), intensity(intensity),
      area(0.0f), center(position) {}

Light::Light(const Vector& corner, const Vector& u, const Vector& v,
             const Vector& emission, float intensity)
    : type(LightType::AREA), position(corner), intensity(intensity),
      u(u), v(v), emission(emission) {
    Vector n = cross(u, v);
    area = n.length();
    if (area <= 1e-6f) throw std::logic_error("area light has degenerate (zero-area) quad");
    normal = n / area;
    center = corner + (u + v) * 0.5f;
}

LightType Light::getType() const { return type; }

const Vector& Light::getPosition() const {
    return (type == LightType::AREA) ? center : position;
}

float Light::getIntensity() const { return intensity; }

Vector Light::samplePoint(float& pdfArea) const {
    float a = randFloat();
    float b = randFloat();
    pdfArea = 1.0f / area;
    return position + u * a + v * b;
}

const Vector& Light::getNormal() const { return normal; }
float Light::getArea() const { return area; }

Vector Light::emittedRadiance() const {
    return emission * (intensity / (area * 3.14159265358979323846f));
}
```

Note: confirm `randFloat()` is declared via a header `Light.cpp` already sees (it lives in `Utils/Vector.*` / a utilities header used across the tree). If not visible, add the include that `Pathtracer.cpp` uses for `randFloat`.

- [ ] **Step 3: Build to verify it compiles**

Run: `cmake --build build 2>&1 | tail -5`
Expected: builds clean; base `Raytracer.cpp` still compiles because `getPosition()`/`getIntensity()` remain.

- [ ] **Step 4: Regression render (point light unaffected)**

Run: `./build/renderer Scenes/scene2.scene /tmp/task2_scene2.ppm`
Expected: completes; identical look to current `scene2` (POINT ctor path unchanged). User judges against committed reference.

- [ ] **Step 5: Commit**

```bash
git add SourceCode/Scene/Light.h SourceCode/Scene/Light.cpp
git commit -m "Add AREA light type with quad sampling and emitted radiance"
```

---

## Task 3: Parse emissive materials + auto-register AREA lights

**Files:**
- Modify: `SourceCode/Scene/SceneFactory.h` (add key constants), `SourceCode/Scene/SceneFactory.cpp`

**Interfaces:**
- Consumes: `Material::emission`, `MaterialType::EMISSIVE` (Task 1); AREA `Light` ctor (Task 2); `Mesh::getAllTriangles()`, `Mesh::getMaterialIndex()`, `Triangle::vertices[]` (existing).
- Produces: scene files may declare a material `{"type":"emissive","emission":[r,g,b],"intensity":I,...}`; the loader appends one AREA `Light` per emissive object.

- [ ] **Step 1: Add scene-key constants**

In `SourceCode/Scene/SceneFactory.h`, near the other `sceneMeshMaterial*` consts:
```cpp
static const char* sceneMeshMaterialEmissive = "emissive";
static const char* sceneMeshMaterialEmission = "emission";
```

- [ ] **Step 2: Parse the emissive material type**

In `SceneFactory::loadMaterial` (`SceneFactory.cpp:220`), add a branch alongside the existing type checks:
```cpp
else if (strcmp(typeAsString, sceneMeshMaterialEmissive) == 0) {
    material.type = MaterialType::EMISSIVE;
}
```
And after the ior block (`:257`), parse emission colour (default white if omitted):
```cpp
if (material.type == MaterialType::EMISSIVE) {
    auto it = matVal.FindMember(sceneMeshMaterialEmission);
    material.emission = (it != matVal.MemberEnd() && it->value.IsArray())
        ? loadVector(it->value.GetArray()) : Vector(1, 1, 1);
}
```

- [ ] **Step 3: Auto-register AREA lights from emissive objects**

In `SceneFactory::factory` (`SceneFactory.cpp:277`), after `geometryObjects` and `materials` are built and before constructing `Scene` (`:293`), add:
```cpp
// Each emissive object contributes one quad area light.
// Quad derived from the object's first triangle (corner A, edges B-A, C-A);
// a standard two-triangle quad reconstructs the parallelogram.
for (const Mesh& obj : geometryObjects) {
    int mi = obj.getMaterialIndex();
    if (mi < 0 || mi >= (int)materials.size()) continue;
    if (materials[mi].type != MaterialType::EMISSIVE) continue;
    Triangle tri = obj.getAllTriangles()[0];
    Vector A = tri.vertices[0], B = tri.vertices[1], C = tri.vertices[2];
    // Intensity carried on the material's albedo.x if present, else a default;
    // simplest: read a scalar from the emissive material. Use emission * fixed
    // radiant scale — intensity lives on the light. See Step 4 for the source.
    lights.push_back(Light(A, B - A, C - A, materials[mi].emission, /*intensity*/ 1.0f));
}
```

- [ ] **Step 4: Decide where `intensity` for an area light comes from**

The point-light `intensity` lives on the light JSON. An emissive *object* has no light JSON. Simplest, YAGNI: carry intensity on the emissive **material** as an extra scalar key `"intensity"`. Add constant `static const char* sceneMeshMaterialIntensity = "intensity";` and in `loadMaterial`'s emissive block:
```cpp
auto iit = matVal.FindMember(sceneMeshMaterialIntensity);
material.ior = (iit != matVal.MemberEnd() && iit->value.IsNumber())
    ? iit->value.GetFloat() : 1.0f;   // reuse the unused-for-emissive ior slot as intensity
```
Then in Step 3 pass `materials[mi].ior` as the intensity argument. (Reusing `ior` avoids adding a field; emissive materials never refract. If this reads confusingly during review, add a dedicated `float intensity` to `Material` instead — a one-line change.)

- [ ] **Step 5: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: builds clean.

- [ ] **Step 6: Verify degenerate-quad + point-light regression**

Run: `./build/renderer Scenes/scene2.scene /tmp/task3_scene2.ppm`
Expected: unchanged (no emissive objects ⇒ no AREA lights added; point light intact).
Degenerate quad is covered by the Task 2 ctor throw; a scene with a zero-area emissive object must fail loudly at load, not render NaNs (Review Focus). User judges scene2 against reference.

- [ ] **Step 7: Commit**

```bash
git add SourceCode/Scene/SceneFactory.h SourceCode/Scene/SceneFactory.cpp
git commit -m "Parse emissive materials and auto-register quad area lights"
```

---

## Task 4: Area-light path seeding in `getLigthPath`

**Files:**
- Modify: `SourceCode/Pathtracer.cpp` (`getLigthPath` ~`:320`, and the LIGHT branch of `tracePath` seeding ~`:245`)

**Interfaces:**
- Consumes: `Light::getType`, `samplePoint`, `getNormal` (Task 2); `randomHemisphereSample` / cosine sampler (existing in `Utils/Vector`).
- Produces: for AREA lights, a light subpath whose first vertex is a sampled emitter surface point, direction cosine-sampled around the light normal; `pdf_fwd` reflects positional×directional pdf.

- [ ] **Step 1: Branch `getLigthPath` on light type**

Replace `getLigthPath` (`:320`):
```cpp
std::vector<PathVertex> Pathtracer::getLigthPath(const Light& light) const
{
    if (light.getType() == LightType::AREA) {
        float pdfA;
        Vector origin = light.samplePoint(pdfA);
        Vector dir = randomHemisphereSample(light.getNormal()); // cosine-weighted, hemisphere about normal
        Ray areaRay{ origin, dir, RayType::LIGHT, 0 };
        return tracePath(areaRay, LIGHT_PATH_LENGHT);
    }
    Vector randomDir = randomSphereSample();
    Ray randomRay{ light.getPosition(), randomDir, RayType::LIGHT, 0 };
    return tracePath(randomRay, LIGHT_PATH_LENGHT);
}
```
Confirm `randomHemisphereSample` returns a cosine-weighted sample (check `Utils/Vector.cpp:159`); if it is uniform, the directional pdf in Task 7 must match whatever it actually is. Record the true pdf used.

- [ ] **Step 2: Emission seeding note**

The LIGHT branch of `tracePath` (`:245`) seeds `beta_running = (1,1,1)` and `pendingFwdPdf = 1/(4π)`. That `1/(4π)` is the POINT sphere pdf. For AREA, the true first-vertex forward pdf is the positional×directional area pdf — but `beta` seeding stays `(1,1,1)` (emission applied at the connection edge, Task 5). Leave `beta` seeding as-is; the pdf used for MIS is fixed in Task 7. Add a terse comment marking that AREA overrides the seed pdf downstream.

- [ ] **Step 3: Build**

Run: `cmake --build build 2>&1 | tail -3`
Expected: clean.

- [ ] **Step 4: Verify point-light path unchanged**

Run: `./build/renderer Scenes/scene2.scene /tmp/task4_scene2.ppm`
Expected: identical to reference (AREA branch not taken).

- [ ] **Step 5: Commit**

```bash
git add SourceCode/Pathtracer.cpp
git commit -m "Seed light subpaths from sampled quad surface for area lights"
```

---

## Task 5: Area-light NEE + connection emission term

**Files:**
- Modify: `SourceCode/Pathtracer.cpp` (`directIllumination` `:470`; `connectVertices` `:504`; `castToImagePlane` `:552`)

**Interfaces:**
- Consumes: `Light::getType`, `samplePoint`, `getNormal`, `emittedRadiance` (Task 2); `connected`, `geometryTerm`, `evalBRDF` (existing).
- Produces: correct area-light contribution in the three transport strategies, gated on light type so POINT keeps its exact current math.

- [ ] **Step 1: Area branch in `directIllumination` (NEE)**

Wrap the existing POINT body in a type check; add the AREA branch:
```cpp
Vector Pathtracer::directIllumination(const PathVertex& data, const Light& light) const
{
    if (data.is_delta) return Vector(0, 0, 0);

    if (light.getType() == LightType::AREA) {
        float pdfA;
        Vector lp = light.samplePoint(pdfA);
        if (!connected(lp, data.position, data.normal)) return Vector(0, 0, 0);
        Vector toL = lp - data.position;
        float dist2 = toL.length2();
        toL.normalize();
        float cosSurf  = std::max(0.0f, dot(toL, data.normal));
        float cosLight = std::max(0.0f, dot(-toL, light.getNormal())); // one-sided
        if (cosSurf <= 0.0f || cosLight <= 0.0f) return Vector(0, 0, 0);
        Vector brdf = evalBRDF(data, toL);
        Vector Le = light.emittedRadiance();
        // area-measure estimator: Le * f_r * cosSurf * cosLight / (dist2 * pdfA)
        float geom = cosSurf * cosLight / (dist2 * pdfA);
        return data.beta * brdf * Le * geom;
    }

    // POINT (unchanged)
    if (!connected(light.getPosition(), data.position, data.normal))
        return Vector(0, 0, 0);
    Vector lightDir  = light.getPosition() - data.position;
    float  dist2     = lightDir.length2();
    lightDir.normalize();
    float cosLaw    = std::max(0.0f, dot(lightDir, data.normal));
    float lightE     = light.getIntensity() / (float)(4.0 * PI_HI * dist2);
    Vector brdf      = evalBRDF(data, lightDir);
    return data.beta * brdf * cosLaw * lightE;
}
```

- [ ] **Step 2: Area emission term in `connectVertices`**

In `connectVertices` (`:504`), the emission factor currently is:
```cpp
float intensityFactor = ... cosEmit * light.getIntensity() / (4π * lightDist2);
```
Gate on type. For AREA, the emitter vertex `lightPath[0]` sits *on* the light; emission is `L_e · cosEmit` with the positional pdf carried in MIS (Task 7), not here:
```cpp
Vector emitTerm;
if (light.getType() == LightType::AREA) {
    float cosEmit = std::max(0.0f, dot(lightPath[0].normal, lightPath[0].wo)); // one-sided
    emitTerm = light.emittedRadiance() * cosEmit;
} else {
    Vector lightToV0 = lightPath[0].position - light.getPosition();
    float lightDist2 = lightToV0.length2();
    float cosEmit = std::abs(dot(lightPath[0].normal, lightPath[0].wo));
    float f = (lightDist2 > 0.0f) ? cosEmit * light.getIntensity() / (float)(4.0 * PI_HI * lightDist2) : 0.0f;
    emitTerm = Vector(f, f, f);
}
```
Then `return cv.beta * lv.beta * brdfCam * brdfLight * G * emitTerm * w;` (emitTerm is a Vector now; ensure component-wise multiply is used).

- [ ] **Step 3: Same emission-term swap in `castToImagePlane`**

Apply the identical type-gated `emitTerm` swap in `castToImagePlane` (`:552`), replacing the scalar `intensityFactor`, and multiply component-wise into `contrib`.

- [ ] **Step 4: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: clean. Watch for scalar-vs-Vector multiply mismatches — fix by using the Vector `emitTerm` consistently.

- [ ] **Step 5: Point-light regression**

Run: `./build/renderer Scenes/scene2.scene /tmp/task5_scene2.ppm`
Expected: identical to reference (all AREA branches skipped for POINT).

- [ ] **Step 6: Commit**

```bash
git add SourceCode/Pathtracer.cpp
git commit -m "Area-light emission term in NEE, connection, and splat strategies"
```

---

## Task 6: New `s=0` strategy — camera rays hit the emitter

**Files:**
- Modify: `SourceCode/Pathtracer.cpp` (`computeColor` `:581`); `SourceCode/Raytracer.cpp` (`shadeDirectIllumination` `:485` — emissive returns emission on hit)

**Interfaces:**
- Consumes: `Material::type == EMISSIVE`, `Material::emission`, `Light::emittedRadiance` (Tasks 1–2); `PathVertex::materialIndex`, `PathVertex::normal`, `PathVertex::beta`, `PathVertex::wo` (existing).
- Produces: direct emitter-hit contribution in BDPT; base raytracer shows emitters as lit.

- [ ] **Step 1: Emitter hit in `computeColor` (BDPT s=0)**

This step depends on `emitterRadianceFor` (Step 2) and `misWeightS0` (Step 3) — implement all three together; the order here is for reading. In `computeColor` (`:581`), after `cameraPath` is built (`:591`) and before the strategy loops, add:
```cpp
// s=0: a camera ray that lands on an emissive surface sees the light directly.
if (debugMode == BDPTDebugMode::ALL) {
    for (int t = 0; t < (int)cameraPath.size(); ++t) {
        const Material& m = scene->getMaterial(cameraPath[t].materialIndex);
        if (m.type != MaterialType::EMISSIVE) continue;
        float cosEmit = std::max(0.0f, dot(cameraPath[t].normal, cameraPath[t].wo)); // wo points toward camera-side prev vertex
        if (cosEmit <= 0.0f) continue; // one-sided; edge-on or back-face contributes 0
        Vector Le = emitterRadianceFor(cameraPath[t].materialIndex); // Step 2
        if (Le.length2() <= 0.0f) continue;
        color += cameraPath[t].beta * Le * cosEmit * misWeightS0(cameraPath, t + 1); // weight stubbed to 1.0 until Task 7
    }
}
```

- [ ] **Step 2: Resolve `L_e` at an emissive hit**

Simplest, YAGNI: give `computeColor` access to the emitter's radiance by matching the hit's `materialIndex` to the AREA light registered from that material. Since one emissive object ⇒ one AREA light, add a small helper on `Scene` or a local lookup: iterate `scene->getLights()`, and for AREA lights compare against the emissive material. To avoid a fragile match, add during Task 3 registration a back-reference: store the source `materialIndex` on the `Light` (add `int emissiveMaterialIndex = -1;` to `Light`, set it in the AREA ctor call site). Then:
```cpp
auto emitterRadianceFor = [&](int matIdx) -> Vector {
    for (const Light& L : scene->getLights())
        if (L.getType() == LightType::AREA && L.getEmissiveMaterialIndex() == matIdx)
            return L.emittedRadiance();
    return Vector(0, 0, 0);
};
```
Use `Le = emitterRadianceFor(cameraPath[t].materialIndex)` in Step 1. (This adds a getter + one field to `Light` and one setter arg in Task 3 — fold that back into Task 2/3 interfaces when implementing; noted here so the executor doesn't miss it.)

- [ ] **Step 3: `misWeightS0` helper (stub weight until Task 7)**

`s=0` means zero stored light vertices and `t` stored camera vertices. Add a thin wrapper (declare in `Pathtracer.h`). In Task 6 it returns a raw `1.0f`; Task 7 gives it the `light` param and the real weight:
```cpp
float Pathtracer::misWeightS0(const std::vector<PathVertex>& camPath, int t) const {
    return 1.0f; // raw estimator; Task 7 replaces with the balance weight (adds a Light param)
}
```
Task 7 changes this signature to take `const Light&` and forwards to `misWeight(camPath, t, empty, 0, light)`, updating the Step 1 call site accordingly. Both the Task 6 and Task 7 signatures are spelled out; the executor edits `misWeightS0` twice (once here, once in Task 7).

- [ ] **Step 4: Base raytracer shows emitters**

In `Raytracer.cpp::shadeDirectIllumination` (`:485`), at the top, return emission for emissive hits so the base tracer doesn't render the light black:
```cpp
const Material& material = scene->getMaterial(data.materialIndex);
if (material.type == MaterialType::EMISSIVE) return material.emission.clamp(0, 1);
```

- [ ] **Step 5: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: clean.

- [ ] **Step 6: Verify — emitter is visible, exposure sane (user judges)**

Requires the Task 8 scene. If running out of order, defer this render to after Task 8. Otherwise:
Run: `./build/renderer Scenes/cornell_area.scene /tmp/task6_cornell.ppm` then convert to PNG.
Expected: the ceiling quad reads as a bright light in-frame; walls lit; no NaN/black holes. Point-light scene2 still matches reference. User is the final judge.

- [ ] **Step 7: Commit**

```bash
git add SourceCode/Pathtracer.cpp SourceCode/Pathtracer.h SourceCode/Raytracer.cpp
git commit -m "Add s=0 strategy: camera rays hit the area emitter directly"
```

---

## Task 7: MIS weights for area emitters (balance heuristic)

**Files:**
- Modify: `SourceCode/Pathtracer.cpp` (`misWeight` `:405`; `misWeightS0` from Task 6)

**Interfaces:**
- Consumes: `Light::getType`, `getArea`, `getNormal` (Task 2); cosine-hemisphere directional pdf recorded in Task 4.
- Produces: correct, normalized MIS across `s=0`, `s=1` (NEE), and vertex connections for AREA lights; POINT weights unchanged.

- [ ] **Step 1: Area emission pdf in `pathAreaPdf`**

In `misWeight` (`:405`), the light-side seed currently uses the delta sphere pdf:
```cpp
p *= pdfWtoA((float)(1.0 / (4.0 * PI_HI)), std::abs(dot(x[0].nrm, dir)), (float)d2);
```
For AREA lights the emitter vertex `x[0]` is a real surface point. Its marginal pdf is the **positional area pdf `1/area`** times the **directional cosine-hemisphere pdf converted to area measure** along the first edge. Replace the seed for AREA:
```cpp
if (light.getType() == LightType::AREA) {
    // positional pdf on the quad (already area measure)
    p *= (1.0f / light.getArea());
    // then the emission-direction pdf (cosine hemisphere) to x[1], converted W->A,
    // is folded by the same scatterEdge machinery for i>=1 using the light normal
    // as x[0].nrm — cosine/PI directional pdf.
} else {
    p *= pdfWtoA((float)(1.0 / (4.0 * PI_HI)), std::abs(dot(x[0].nrm, dir)), (float)d2);
}
```
And ensure the `x[0]` normal used by the first `scatterEdge` is the light normal with a cosine-hemisphere pdf (`cosθ/π`) matching Task 4's actual sampler.

- [ ] **Step 2: Include `s=0` in the denominator for AREA**

The denominator loop `for (int sp = 0; sp <= k; ++sp)` already includes `sp=0`. For POINT, `pathAreaPdf(0)` yields a camera path that ends by area-hitting the point light — probability 0 (delta), correctly excluded. For AREA, `sp=0` is a real strategy and `pathAreaPdf(0)` must be nonzero: the camera-side product times the emitter's `1/area` positional pdf at the terminal vertex. Verify `pathAreaPdf(0)` computes this; if it currently returns the camera product without the emitter positional pdf, add it for AREA when `x[k-1]` is on an area light.

- [ ] **Step 3: Wire real weight into `misWeightS0`**

Replace the Task 6 `return 1.0f;` with a call into the balance-heuristic machinery for the `s=0, t` strategy of the specific AREA light. Pass the light through:
```cpp
float Pathtracer::misWeightS0(const std::vector<PathVertex>& camPath, int t, const Light& light) const {
    std::vector<PathVertex> empty;
    return misWeight(camPath, t, empty, 0, light);
}
```
Update the Task 6 call site to pass `light`.

- [ ] **Step 4: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: clean.

- [ ] **Step 5: MIS consistency probe (user judges)**

Render `cornell_area.scene` in three modes and compare mean exposure of a diffuse wall patch:
```bash
./build/renderer Scenes/cornell_area.scene /tmp/mis_all.ppm            # ALL
BDPT_DBG=... (NEE-only mode) ./build/renderer Scenes/cornell_area.scene /tmp/mis_nee.ppm
```
Expected: ALL is not brighter than the sum of its parts on any patch — no seam/halo where caustics overlap diffuse GI. A halo ⇒ emission double-count (Task 5) or bad `s=0` weight. Report the patch means + a hypothesis; user calls the verdict.

- [ ] **Step 6: Commit**

```bash
git add SourceCode/Pathtracer.cpp SourceCode/Pathtracer.h
git commit -m "MIS balance weights for area emitters including s=0 strategy"
```

---

## Task 8: Cornell-ceiling verification scene + final renders

**Files:**
- Create: `Scenes/cornell_area.scene`

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Author the scene**

A 3×3×3 Cornell box (reuse `scene2`/`glass_prism` room quads), no point light, plus a small emissive quad in the ceiling (y just below the ceiling plane, normal pointing **down** — winding so `u×v` = (0,−1,0)). Emissive material `{"type":"emissive","emission":[1,1,1],"intensity":<tuned>}`. Keep the glass prism from `glass_prism.scene` so caustics are exercised. 400×400.

Winding check: for a downward-facing ceiling light with corner `A` and edges `u`, `v`, ensure `cross(u,v).y < 0`. If it comes out `+y`, swap `u` and `v`.

- [ ] **Step 2: Build (no-op if already built) and render**

Run:
```bash
cmake --build build 2>&1 | tail -2
./build/renderer Scenes/cornell_area.scene /tmp/cornell_area.ppm
```
Expected: completes; ceiling quad visible and lit; walls softly lit with soft shadows.

- [ ] **Step 3: Convert to PNG and inspect stats**

Use the inline Python PPM→PNG converter (struct+zlib) to write `/tmp/cornell_area.png`; report pixmax and %nonzero. Show the image inline with its `/tmp` path.

- [ ] **Step 4: Caustic-quality comparison (the payoff — user judges)**

Render the same prism scene with (a) the point light (`glass_prism.scene`) and (b) the ceiling quad (`cornell_area.scene`) at equal rpp. Expected: the firefly speckle collapses into a smoother caustic with the area light. Present both inline + `/tmp` paths + a one-line hypothesis; user is the final judge.

- [ ] **Step 5: Commit**

```bash
git add Scenes/cornell_area.scene
git commit -m "Add Cornell-ceiling area-light verification scene"
```

---

## Notes for the executor

- **Order matters:** Tasks 6–7 are tightly coupled (`s=0` estimator + its MIS weight). Task 6 lands `s=0` with a stub weight of `1.0`; Task 7 makes it correct. Don't judge exposure until Task 7 is in.
- **`Light` field additions surfaced mid-plan:** Task 6 Step 2 adds `emissiveMaterialIndex` + getter to `Light` and threads it through the Task 3 registration call. Fold this into the Task 2/3 edits when implementing so `Light` is edited once.
- **Verify the real directional sampler:** Task 4 Step 1 — confirm whether `randomHemisphereSample` is cosine-weighted or uniform; Task 7's directional pdf (`cosθ/π` vs `1/2π`) must match the sampler actually used. This is the single most likely source of a wrong-but-plausible exposure.
- **No test runner:** every "verify" step is a build + numeric probe or render. The user judges render/benchmark output — present numbers + hypothesis, don't self-declare correctness.
