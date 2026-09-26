# Area Lights (Quad Emitters) — Design

## Goal

Add a diffuse **quad area light** to the BDPT so caustics through specular
surfaces (e.g. the glass prism) become low-variance. A point light is a delta
emitter: it can only be *connected to*, never *hit*, so caustics arrive as
sparse fireflies (the SDS-path variance limit). A finite emitter is
connectable at both endpoints and hittable by camera rays, which removes that
variance.

Point lights remain fully supported and unchanged. Area lights are opt-in per
light in the scene file.

## Decisions (locked with user)

1. **Shape:** quad — a corner `position` plus two edge vectors `u`, `v`.
   `area = |u×v|`, `normal = normalize(u×v)`, sample point
   `p = position + ξ₁·u + ξ₂·v` (uniform, area-measure pdf `1/area`).
2. **Point light kept:** `Light` gains a `type` (POINT | AREA). POINT keeps
   every existing delta-terminal code path.
3. **Emission:** one-sided Lambertian. Emitted radiance
   `L_e = intensity / (area · π)`, emitted only into the hemisphere around the
   light normal. `intensity` keeps its current meaning (radiant power), so an
   existing intensity value maps to a comparable brightness.
4. **The quad is real scene geometry** (Option A). It is declared once in the
   scene file as a normal object with an emissive material; the loader derives
   the sampling `Light` from that object. One declaration serves both roles
   (sampling + hitting), so there is nothing to keep in sync by hand.

## Why the quad is geometry (not an analytic light)

The renderer already carries everything this needs:

- `Material` is a plain struct — add one enum value + one field.
- Objects are `Mesh`es loaded from the scene file with a `material_index`. A
  ceiling light is just a quad object (like the existing back-face-culled front
  wall) pointing at an emissive material. `parseObjects` already handles it.
- `Intersection` already carries `materialIndex`, so a camera ray that lands on
  the emitter already knows it hit a light. The new `s=0` strategy reads data
  that is already there.
- The BVH already handles occlusion, so the light shadows and blocks rays with
  no extra intersection code.

An analytic light would need a separate ray-quad test and a parallel occlusion
path — more code, not less.

## Emitter identity: one declaration, two roles

The emitter is written **once** in the scene file as an object with an emissive
material. The loader then auto-registers an area `Light` derived from that
object's geometry:

- Take the object's first triangle `(A, B, C)`. Two co-planar triangles form
  the quad `A, B, C, D`. Derive `position = A`, `u = B − A`, `v = C − A` (for a
  standard two-triangle quad this reconstructs the parallelogram). `normal`,
  `area` follow.
- `intensity` and `emission` colour come from the emissive material / a scene
  field on the object.

Result: the user places one quad in the ceiling; the loader wires up sampling
and hitting. No duplicated coordinates.

## The four emission-convention sites (the substantive work)

An area light turns four hardcoded point-light assumptions into sampled
quantities. Current locations (post-cleanup):

1. **`getLigthPath` (`Pathtracer.cpp:320`)** — light-path seeding.
   POINT (today): ray origin = fixed point, direction = `randomSphereSample()`,
   `pdf_fwd = 1/(4π)`.
   AREA: sample a surface point `p` (pdf `1/area`); sample a **cosine-weighted
   hemisphere** direction around the light normal (pdf `cosθ/π`). The stored
   first light vertex is the emitter surface point itself, flagged non-delta.
   `beta` seeding stays `(1,1,1)`; the emitted radiance is applied at the
   emission edge (site 3), consistent with how the point light keeps intensity
   out of `beta`.

2. **`directIllumination` (`Pathtracer.cpp:470`, s=1 NEE)** — per shading vertex.
   POINT (today): connect to the fixed point, `intensity/(4π·r²)`.
   AREA: sample a point on the light, weight by `1/(area · … )` in area measure;
   contribution `= β · f_r · G · L_e · cosθ_light`, where `G` carries both
   cosines and `L_e = intensity/(area·π)`. Shadow ray to the sampled point.
   Cull the back face (emit only from the normal side).

3. **`connectVertices` (`:504`) and `castToImagePlane` (`:552`)** — emission term.
   POINT (today): `cosEmit · intensity / (4π · lightDist²)`.
   AREA: replace with the area emitter's emitted radiance
   `L_e · cosEmit` at the emitter vertex (the `1/area` positional pdf and the
   directional pdf already live in the light-path throughput / MIS, so they are
   not re-applied here — this is the one spot to get right to avoid a
   double-count or exposure seam).

4. **`misWeight` (`:441`)** — emission pdf in the balance heuristic.
   POINT (today): emitter is a delta terminal with 0 stored light vertices; the
   `1/(4π)` sphere pdf is baked into `pathAreaPdf`.
   AREA: the emitter vertex is a **real, stored, non-delta vertex**. Its
   marginal pdf is the positional area pdf `1/area` combined with the
   cosine-hemisphere directional pdf converted to area measure. Every strategy
   `sp` in the denominator uses this instead of the `1/(4π)` constant. Point
   lights continue to use the existing delta-terminal path.

## New strategy: `s=0` (camera hits the emitter)

With a finite light, a camera ray can land on the emitter directly. This is
genuinely new (point lights skipped it as provably zero).

- In `computeColor`, after building `cameraPath`, if the last camera vertex's
  material is emissive, add its emitted radiance toward the camera:
  `β_cam · L_e · cosθ` (MIS-weighted against the NEE/connection strategies that
  also sample that emitter). Back face contributes zero.
- MIS: `s=0` joins the denominator sum for area lights (it is a real strategy),
  so no seam/double-count where direct-hit and NEE overlap.

## Data model changes

### `Material.h`
```
enum class MaterialType { DIFFUSE, REFLECTIVE, REFRACTIVE, EMISSIVE };
struct Material {
    ...
    Vector emission;   // radiance colour, used when type == EMISSIVE
};
```
Emissive surfaces need a distinct flag, not reuse of `is_delta`. Today
`is_delta = (type != DIFFUSE)` marks specular pass-through vertices (mirror,
glass) that are skipped as connection endpoints. An EMISSIVE surface is the
opposite: it never scatters, terminates the path, and IS a valid connection
endpoint. So the vertex gains `is_light` (already present, currently always
false) set true for emissive hits, and `is_delta` stays false for emissive.
`is_delta` continues to mean "specular, skip as endpoint" only.

### `Light.h` / `Light.cpp`
```
enum class LightType { POINT, AREA };
class Light {
    LightType type;
    Vector position;      // POINT: the point; AREA: quad corner
    float   intensity;
    Vector  u, v;         // AREA only
    Vector  normal;       // AREA only (derived)
    float   area;         // AREA only (derived)
public:
    // POINT ctor unchanged; new AREA ctor
    LightType getType() const;
    const Vector& getPosition() const;   // AREA: returns quad centre (keeps base Raytracer working)
    float getIntensity() const;
    // AREA sampling:
    Vector samplePoint(float& pdfArea) const;   // uniform on quad
    const Vector& getNormal() const;
    float getArea() const;
    float emittedRadiance() const;              // intensity / (area * PI)
};
```
`getPosition()` for an AREA light returns the quad centre so the **base
`Raytracer.cpp` keeps compiling and rendering** (it treats the area light as a
point at the centre — acceptable; the base tracer is not the target of this
work).

### `SceneFactory.cpp`
- Parse `emission` on materials; parse the emissive object as a normal mesh.
- After objects+materials load, scan for emissive objects and auto-append an
  AREA `Light` derived from each (`position/u/v` from the first triangle,
  `intensity`/colour from the material or an object field).

## Non-breaking guarantees

- Point-light scenes (`scene2`, `glass_dragon`) parse and render identically:
  no `type` field ⇒ POINT; all POINT code paths untouched.
- Base `Raytracer.cpp` compiles unchanged; area lights degrade gracefully to a
  centre point there.
- The BDPT diagnostic scaffolding (`BDPT_CAUSTIC`, `BDPT_SPLIT`,
  `BDPT_EXPOSURE`, `BDPT_DBG`, debug-mode enum) is left in place; it is
  orthogonal to this change and its cleanup is tracked separately.

## Blast radius

| File | Change |
|---|---|
| `SourceCode/Scene/Material.h` | +`EMISSIVE` enum, +`emission` field |
| `SourceCode/Scene/Light.h` / `.cpp` | `type`, quad fields, `samplePoint`, `emittedRadiance`, derived normal/area |
| `SourceCode/Scene/SceneFactory.cpp` | parse emission; auto-register AREA light from emissive object |
| `SourceCode/Pathtracer.cpp` | 4 emission-convention sites + new `s=0` branch in `computeColor` |
| `SourceCode/Pathtracer.h` | signatures if emission helpers need new params |
| `SourceCode/Raytracer.cpp` | emissive materials return emission on hit (keep base tracer sane) |
| A new Cornell-ceiling scene file | quad emitter in the ceiling for verification |

No new `.cpp` files ⇒ no CMake change (CMake globs `Scene/`+`Utils/` and lists
`Pathtracer.cpp`).

## Verification plan

No automated tests exist; verification is visual + numeric, and the user is the
final judge of results.

1. **Regression:** render `scene2` (point light) — must match the current
   committed result (same exposure, no code-path drift).
2. **New Cornell-ceiling scene:** quad light in the ceiling.
   - Staged via debug modes: NEE-only (clean soft direct light + soft
     shadows), light-trace-only (caustics present and *smoother* than the
     point-light prism render), ALL (combined, no seam/halo where caustics
     overlap diffuse GI — a halo signals an emission-term double-count at site
     3 or a bad `s=0` MIS weight).
   - `s=0` check: the light quad is visible in the image at the right
     brightness; turning off `s=0` only adds noise, never changes mean exposure
     (MIS consistency).
3. **Caustic quality:** re-render the glass-prism scene with a small ceiling
   quad instead of the point light; expect the firefly speckle to collapse into
   a smoother caustic at equal rpp — the whole point of the change.

## Open risks (flag during implementation)

- **Emission double-count (site 3 vs MIS):** the `1/area` and directional pdfs
  must live in exactly one place (throughput/MIS), not also in the emission
  term. A brightness mismatch between NEE, connection, and `s=0` of the *same*
  emitter is the tell.
- **`s=0` MIS weight:** must be added to the denominator for area lights only;
  leaving it out biases nothing but wastes the strategy, double-adding it
  darkens direct hits.
- **Back-face emission:** one-sided — a light whose normal points away from the
  scene emits nothing. The quad winding in the scene file determines the normal
  (`u×v`); document the expected winding so ceiling lights face down.
