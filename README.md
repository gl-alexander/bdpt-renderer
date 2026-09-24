# BDPT Renderer

A CPU ray tracer / bidirectional path tracer written from scratch.

### Base Functionalities
<img src="https://github.com/gl-alexander/bdpt-renderer/blob/main/Images/Homework11/scene8_depth15.png" alt="Glass Dragon"/> <br>
- Support for Diffuse, Reflective and Refractive materials via shaders
- Acceleration structures: [KD Tree](https://en.wikipedia.org/wiki/K-d_tree) for fast intersection checking
- Multithreading: the final image is rendered in buckets of fixed size. Work is split between available threads.
- Textures: bitmap textures, as well as debug textures
- `.scene` file format parsing

### Added functionalities
<img src="https://github.com/gl-alexander/bdpt-renderer/blob/main/Images/Project/Animations/GI_random_sampling.gif" alt="Global Illumination"/> <br>
- Path Tracing with explicit light sampling: ray path continues on non-specular hits (global illumination). Rays are "guided" towards a light source.
- Anti-aliasing: [FXAA](https://en.wikipedia.org/wiki/Fast_approximate_anti-aliasing) implementation, Rendered AA (option for rays per pixel)
- Animations module: separates mesh-manipulating animations from camera movement (to preserve KD Tree building). Supports orbit animation, as well as [Vertigo effect](https://en.wikipedia.org/wiki/Dolly_zoom) 
- Depth of Field: simulates the physical attributes of a real camera (e.g. aperature and focus distance)
- [Stereoscopic effect](https://en.wikipedia.org/wiki/Stereoscopy): simulates the depth and 3D effects of human eyesight by creating two pictures for each eye. The pictures are then combined into a single one using the [anaglyphic effect](https://en.wikipedia.org/wiki/Anaglyph_3D)
- Bidirectional Path Tracing

### Bidirectional Path Tracing
A fully general light transport algorithm, that can handle a wide variety of cases that a regular Path Tracer misses (especially in tricky lighting conditions). For this "extension" a different approach to materials and ray path construction is used (See [BRDF](https://en.wikipedia.org/wiki/Bidirectional_reflectance_distribution_function))

The algorithm simulates two ray paths for each sample: the camera path (as in the regular path tracer), as well as a light path (that starts at a random light source). These paths are then connected using a variety of techniques, that are then weighed by their importance (Multiple Importance Sampling) and combined, resulting in a higher image quality with fewer ray paths.

This technique allows for [caustics](https://en.wikipedia.org/wiki/Caustic_(optics)) to appear, due to the concentration of light rays in a tight area. These hits are then connected with the camera's lens, which allows us to see them in the final image.

<img src="https://github.com/gl-alexander/bdpt-renderer/blob/main/Images/Project/BDPT/scene2_final.png" alt="Scene with caustics"/>

### [Project Presentation](https://github.com/gl-alexander/bdpt-renderer/blob/main/Images/Project/course-assignment-chaos.pdf): illustrates the added functionalities

### Sources
- The ChaosCamp lectures
- [Physically Based Rendering](https://www.pbr-book.org/3ed-2018/Light_Transport_III_Bidirectional_Methods/Bidirectional_Path_Tracing)
- [Eric Veach's Dissertation](https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf)
- [Scratchapixel](https://www.scratchapixel.com/)
