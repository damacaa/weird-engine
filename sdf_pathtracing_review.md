# SDF Path Tracing Review & Denoising Recommendations

## Implementation Overview

Your path tracer lives in [sdf_raymarching.frag](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag) with the CPU-side pipeline in [SDF3DRenderPipeline.cpp](file:///d:/Projects/weird-engine/src/weird-renderer/core/SDF3DRenderPipeline.cpp). The architecture is:

1. **Raymarching** SDF scene → primary hit
2. **`pathTrace()`** loop with next-event estimation (direct light sampling) + indirect bounce via cosine-weighted hemisphere or specular reflection
3. **Temporal accumulation** via ping-pong `AccumulationData` textures with an EMA blend
4. **Post-process**: optional bilateral "surface blur" + optional Bayer dithering in [screen_output.frag](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/postprocess/screen_output.frag)

---

## Code Review Observations

### ✅ Things that work well

| Aspect | Notes |
|---|---|
| **Next-event estimation** | Direct light sampling at every bounce with shadow rays is correct and dramatically reduces variance vs. pure unidirectional path tracing. |
| **Fresnel-driven Russian roulette** | Stochastic specular/diffuse split weighted by Schlick Fresnel is physically sound and correctly compensates throughput. |
| **Material jitter at smooth-union boundaries** | The `materialSamplePos` jitter ([L791](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L791), [L577](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L577)) is a clever trick for temporal anti-aliasing of smooth-blended SDF materials. |
| **Dithered transparency** | Bayer-matrix transparency with frame-cycling offset ([L193-L198](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L193-L198)) converges cleanly with accumulation. |
| **Camera-movement detection** | [SDF3DRenderPipeline.cpp:L68-L73](file:///d:/Projects/weird-engine/src/weird-renderer/core/SDF3DRenderPipeline.cpp#L68-L73) correctly resets the frame counter on camera or shader changes. |
| **Accumulation cap** | `maxAccumulationFrames` prevents pointless work after convergence. |

---

### ⚠️ Issues & Improvement Opportunities

#### 1. Specular throughput is always `vec3(1.0)` — energy not conserved

```glsl
// Line 553
throughput *= vec3(1.0); // ????
```

This comment (`????`) flags the concern already. Since you split on Fresnel probability and the diffuse branch multiplies by `currentAlbedo`, the specular branch should multiply by the **specular tint**. For dielectrics that's `vec3(1.0)` (which is fine), but for metals (`f0 > 0`) the specular tint should be the albedo color. A more correct version:

```glsl
if (isSpecular)
    throughput *= mix(vec3(1.0), currentAlbedo, currentF0); // metallic tint
else
    throughput *= currentAlbedo;
```

This would give metals correct colored reflections and is a single-line fix.

#### 2. Hash-based PRNG has visible correlation artifacts

[`hash2`](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L329-L333) and [`hash3`](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L335-L339) use `sin()` hashing which:
- Produces visible **structured noise** (banding/correlation), especially on GLES devices where `sin()` precision varies
- Seeding is linear (`seed += 1.0`), which creates inter-pixel correlation

> [!TIP]
> Consider switching to a proper integer hash like **PCG** or **xxhash** adapted for GLSL. Even a simple `uint`-based hash like the one below produces dramatically better blue-noise-like distribution:
> ```glsl
> uint pcg(inout uint state) {
>     state = state * 747796405u + 2891336453u;
>     uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
>     return (word >> 22u) ^ word;
> }
> ```

#### 3. Normal epsilon may be too small for SDF precision

```glsl
const float NORMAL_EPSILON = 0.00001; // Line 99
```

With `float` precision and world-space coordinates that could be tens of units away from origin, `0.00001` is dangerously close to floating-point ULP. This can produce **speckle noise in normals** that looks like denoiser failure but is actually geometric. Consider `0.001` or at least `0.0001`.

#### 4. Shadow ray bias direction

```glsl
float d = rayMarch(p - rd * 0.02, normalize(lightPos)); // Line 322 (standardLighting)
```

Biasing along `-rd` instead of along the surface normal `N` can cause light leaks on grazing surfaces. The path tracer correctly uses `p + N * 0.01` at [L500](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/3d/sdf_raymarching.frag#L500), but the non-path-tracing `getPointLight()` at L322 doesn't.

#### 5. EMA weight floor at 0.02 may cause ghosting

```glsl
float weight = max(1.0 / float(u_frameCounter + 1), 0.02); // Line 865
```

The 2% floor means old frames never fully fade, so **scene changes (object movement, light changes) without camera movement will ghost**. This is fine if the scene is truly static when the camera stops, but if SDF objects are animated, you'd need motion-aware invalidation or a higher floor.

#### 6. Gamma roundtrip in accumulation

```glsl
prevColor.xyz = pow(prevColor.xyz, vec3(2.2)); // Line 860 — linearize
...
FragColor = vec4(pow(col, vec3(0.4545)), 1.0); // Line 870 — back to sRGB
```

Every frame decodes sRGB → linear → encodes back to sRGB. This repeated pow introduces subtle **quantization drift** over hundreds of frames. If your `AccumulationData` texture format supports it, accumulating in linear (using `GL_RGBA16F` or `GL_RGBA32F`) and only converting to sRGB at the very end in the output pass would give cleaner convergence.

---

## Denoising Algorithm Recommendations

Your current denoiser is **temporal accumulation** (EMA) plus an optional **bilateral spatial blur**. Here are algorithms worth investigating, ordered from most practical to most ambitious:

---

### 1. 🥇 À-Trous Wavelet Filter (Edge-Aware)
**Best fit for your engine. Can be implemented as a post-process shader pass.**

The à-trous filter is a multi-scale spatial denoiser used by nearly every real-time path tracer (originally from the [SVGF paper](https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced)). It works by repeatedly applying a small 5×5 filter at exponentially increasing step sizes (1, 2, 4, 8, 16 pixels), which effectively creates a large kernel with only ~25 taps per pass.

**Why it fits your engine:**
- Runs as 3-5 fullscreen post-process passes — trivial to add to your pipeline
- Uses **normals + depth** as edge-stopping functions to preserve sharp silhouettes (you already have both in your GBuffer)
- Operates on a single noisy frame, so it works even while the camera moves
- Very cheap per-pass (5×5 sparse kernel)

**Key papers:** [Dammertz et al. 2010 "Edge-Avoiding À-Trous Wavelet Transform"](https://jo.dreggn.org/home/2010_atrous.pdf)

---

### 2. 🥈 SVGF (Spatiotemporal Variance-Guided Filtering)
**The gold standard for real-time path tracing denoising.**

SVGF combines your existing temporal accumulation with the à-trous spatial filter, but adds a **per-pixel variance estimate** to adaptively control filter strength. Pixels with high variance (noisy) get blurred more; converged pixels stay sharp.

**Components you'd need to add:**
- Per-pixel luminance variance tracking (1st and 2nd moment accumulation buffers)
- Temporal reprojection using motion vectors (you'd need to output these from your SDF pass)
- The à-trous spatial filter from #1, with variance-driven `sigma` parameters

**Why it's powerful:** It can produce clean images from as few as 1 sample per pixel when the camera is moving, which would let you drop `maxAccumulationFrames` dramatically.

**Key paper:** [Schied et al. 2017 "Spatiotemporal Variance-Guided Filtering"](https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced)

> [!IMPORTANT]
> SVGF needs **motion vectors**. For SDF scenes these are non-trivial — you can't just use mesh vertex motion. You'd need to either:
> - Reproject using the previous camera matrix (works for static scenes only)
> - Store per-pixel world positions and reproject through the previous view-projection matrix

---

### 3. 🥉 Non-Local Means (NLM) Denoising
**Higher quality than bilateral, still single-pass friendly.**

NLM compares **patches** (e.g., 5×5 blocks) rather than individual pixels, making it much better at preserving texture detail while removing noise. It's more expensive than bilateral but less complex than SVGF.

**A GPU-friendly variant:**
- Use a search window of ~11×11 and a patch size of 3×3
- Weight by patch similarity in luminance space
- Can be guided by normals/depth like your bilateral filter

This would be a drop-in upgrade over your existing `surfaceBlur()` in [screen_output.frag](file:///d:/Projects/weird-engine/src/weird-renderer/shaders/postprocess/screen_output.frag#L31-L59).

---

### 4. Temporal Anti-Aliasing with Neighborhood Clamping (TAA+)
**Improve your existing temporal denoiser without adding spatial passes.**

Your current temporal blend is a simple EMA. A more robust approach adds **neighborhood clamping**: before blending with the history, clamp the historical color to the min/max of the current pixel's 3×3 neighborhood. This eliminates ghosting from stale data while preserving temporal stability.

```glsl
// Pseudo-code for neighborhood clamping
vec3 minCol = vec3(1e10), maxCol = vec3(-1e10);
for (int y = -1; y <= 1; y++)
    for (int x = -1; x <= 1; x++) {
        vec3 s = texelFetch(currentFrame, ivec2(gl_FragCoord.xy) + ivec2(x,y), 0).rgb;
        minCol = min(minCol, s);
        maxCol = max(maxCol, s);
    }
vec3 clampedHistory = clamp(prevColor, minCol, maxCol);
col = mix(clampedHistory, col, weight);
```

This directly addresses the **ghosting issue** from observation #5 above, and requires minimal changes to your existing shader.

---

### 5. OIDN / OptiX-style Neural Denoiser (Offline / Bake Mode)
**For when you want to bake a final clean render.**

Intel [Open Image Denoise](https://www.openimagedenoise.org/) and NVIDIA OptiX denoisers use trained neural networks. They're too heavy for real-time per-frame use, but if you ever want a "screenshot mode" that produces a publication-quality image from ~64 SPP, you could:
- Accumulate N frames
- Read back the GPU texture
- Run OIDN on CPU (or OptiX on GPU)
- Display the result

This pairs naturally with your existing screenshot functionality in [Renderer.cpp](file:///d:/Projects/weird-engine/src/weird-renderer/core/Renderer.cpp#L358-L369).

---

## Recommended Implementation Path

| Phase | What | Effort |
|-------|------|--------|
| **Quick wins** | Fix specular throughput, upgrade PRNG, bump normal epsilon | ~1 hour |
| **Phase 1** | Add neighborhood clamping to your existing temporal accumulator | ~2 hours |
| **Phase 2** | Implement à-trous wavelet spatial filter as a post-process pass | ~1 day |
| **Phase 3** | Combine into SVGF with variance tracking | ~2-3 days |
| **Optional** | Integrate OIDN for offline/screenshot denoising | ~1 day |

> [!NOTE]
> Phase 1 alone (neighborhood clamping) would give you the biggest quality-per-effort improvement — it eliminates ghosting and lets you lower the EMA floor for faster convergence.
