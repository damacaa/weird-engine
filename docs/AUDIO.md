# Procedural Audio & Music Architecture

Weird Engine features a fully synthesized procedural audio and music architecture built on top of [miniaudio](https://miniaud.io/). 

Rather than relying on static, pre-recorded audio files (such as WAV or MP3 clips), audio in Weird Engine is procedurally generated in real time from **physical simulation dynamics** (collisions, sliding friction, velocity) and **mathematical Signed Distance Field (SDF) geometry**.

Additionally, procedural music is tied to the engine's visual identity: every song's governing SDF shape is rendered in real time as a UI shape anchored in the **top-left corner of the screen**.

---

## 1. System Architecture

The audio pipeline is decoupled across four coordinated layers:

```mermaid
flowchart TD
    subgraph GameThread["Game & ECS Thread"]
        Scene[Scene / Systems]
        AudioService[AudioService (ServiceProvider)]
        Registry[ECS Registry (UIShape Entity)]
    end

    subgraph AudioEngineLayer["Audio Engine Core"]
        AudioEngine[AudioEngine Singleton]
        MusicEngine[SdfMusicEngine]
        PhysicsAudio[PhysicsAudioEngine]
        RingBuffer[Lock-Free AudioRingBuffer]
        SpatialProcessor[SpatialAudioProcessor]
    end

    subgraph MiniaudioThread["Miniaudio High-Priority Callback"]
        AudioDevice[DAC Output / Speakers]
        Mixer[Stereo Interleaved Mixer]
    end

    Scene -->|setSong / setSongParameter / playSound| AudioService
    AudioService -->|Push requests| RingBuffer
    AudioService -->|Spawn visualization| Registry
    AudioService -->|Update musical params| MusicEngine
    RingBuffer -->|Drain requests| PhysicsAudio
    MusicEngine -->|8 Synth Channels| Mixer
    PhysicsAudio -->|Impacts & Friction| SpatialProcessor
    SpatialProcessor -->|Positioned Audio| Mixer
    Mixer -->|PCM Samples 44.1kHz| AudioDevice
```

### Thread Safety & Zero-Allocation Streaming
The real-time audio thread must never block or allocate heap memory (`malloc`/`new`). 
- **`AudioRingBuffer<SimpleAudioRequest, 64>`**: A lock-free single-producer single-consumer ring buffer transfers sound trigger requests from the game and physics threads to the audio thread.
- **Atomic Friction Levels**: Atomic floats (`std::atomic<float> frictionSoundLevel`) communicate dynamic contact velocities for sliding and rolling sounds without locks.
- **Mutex-Protected Song State**: Fast `std::mutex` guards protect song reference swaps and parameter updates between frames.

---

## 2. Shape-Driven Procedural Music (`SdfSong` & `SdfMusicEngine`)

The procedural music engine treats mathematical shapes defined by `IMathExpression` trees as musical controllers. The geometry, motion, and spatial distribution of the shape dictate every musical property of the resulting composition.

### 2.1 How Geometry Drives Music

| Geometric Property | Extracted Metric | Musical Consequence |
|---|---|---|
| **Temporal Motion** | $\frac{\Delta d}{\Delta t}$ across domain points | **Tempo (BPM)**: Static shapes produce slow ambient tempos (40–50 BPM); rapidly moving or spinning shapes drive high energy (90–150+ BPM). |
| **Domain Fill Ratio** | Ratio of sample points where $d < 0$ | **Volume / Presence**: Thin/small shapes sound delicate and soft; voluminous shapes produce fuller, louder presence. |
| **Radial Symmetry** | Asymmetry between compass axes ($E \leftrightarrow W$, $N \leftrightarrow S$) | **Musical Scale**: High symmetry selects Major or Pentatonic scales; asymmetric shapes select Minor or Dorian modes. |
| **Radial Alternation** | Alternation between cardinal and diagonal axes | **Harmonic Palette**: High spoke alternation (e.g. stars) selects Minor or Blues tonalities. |
| **Center Field Depth** | Signed distance $d(0, 0)$ at local origin | **Root Key (Base Note)**: Deep negative centers lower the tonic key; shallow boundaries map to higher roots (MIDI 55 [G3] to 67 [G4]). |
| **Compass Probes** | $d$ sampled at $E, W, N, S, NE, NW, SE, SW$ | **Real-Time Synth Modulations**: Evaluated through sigmoid curves to control melody note density, pad richness, syncopation, and chord intervals. |

### 2.2 Universal Bounding Domain
To prevent arbitrarily large shapes from distorting procedural analysis or filling the entire screen:
- Every `SdfSong` automatically bounds the user's expression by intersecting it with a circle of radius **`DOMAIN_RADIUS = 50.0f`**:
  $$\text{shape}_{\text{bounded}}(p) = \max(\text{shape}_{\text{user}}(p),\, \|p\| - 50.0)$$
- Compass probing and symmetry analysis occur at **`SAMPLE_RADIUS = 20.0f`**.

### 2.3 Universal Top-Left UI Alignment & `point()`
Every song's governing shape is displayed as a UI element in the top-left corner of the screen:
- **`point()`**: A canonical parameterless helper returning local coordinates centered at $(0, 0)$.
- In procedural songs, `SdfSong` automatically applies domain bounding and maps screen/UI sample points so $(0, 0)$ is centered at the song's screen anchor.
- Calling `services.audio().setSong(song)` automatically registers the shape in the UI pipeline as a `UIShape` entity.

---

## 3. Synthesizer Voice Channels & Sound Design

`SdfMusicEngine` implements an 8-channel synthesizer rendered at 44.1 kHz, 16-bit stereo:

```
Channel 0: Lead Melody (Voice A)  ──► [1-pole Low-Pass: 1200 - 2800 Hz] ──┐
Channel 1: Lead Melody (Voice B)  ──► [1-pole Low-Pass: 1200 - 2800 Hz] ──┤
Channel 2: Chord Pad (Voice A)    ──► [1-pole Low-Pass:  700 - 2200 Hz] ──┤
Channel 3: Chord Pad (Voice B)    ──► [1-pole Low-Pass:  700 - 2200 Hz] ──┼──► [Master Bus & Ducking] ──► DAC
Channel 4: Sub-Bassline           ──► [1-pole Low-Pass:   80 -  220 Hz] ──┤
Channel 5: Kick Drum (Sub Pitch)  ──► [Drive Saturation]                ──┤
Channel 6: Snare Drum (Body+Noise)──► [Transient Envelope]              ──┤
Channel 7: Hi-Hat (Metallic Noise)──► [High-Pass Transient]             ──┘
```

### Voice Breakdown

1. **Channels 0 & 1 — Lead Melody**
   - **Range**: Vocal register octaves `0/+1` (MIDI 60–74, ~260–587 Hz).
   - **Waveform**: Warm blended pulse and triangle wave with mild octave-above shimmer.
   - **Filtering**: Smooth single-pole low-pass filter modulated between 1,200 Hz and 2,800 Hz to prevent harshness.
   - **Phrasing**: Procedurally generated scale walk driven by the shape's East and North compass probes.

2. **Channels 2 & 3 — Chord Pads & Harmony**
   - **Range**: Warm mid register octaves `-1/0` (MIDI 48–60, ~130–260 Hz).
   - **Waveform**: Multi-sine harmonic blend ($0.65\sin(\theta) + 0.25\sin(2\theta) + 0.10\sin(3\theta)$).
   - **Filtering**: Warm, mellow low-pass filter (700–2,200 Hz) providing acoustic depth without masking dialogue or gameplay sounds.

3. **Channel 4 — Sub-Bass**
   - **Range**: Sub octave `-2` (MIDI 31–43, ~40–85 Hz).
   - **Waveform**: Dual-oscillator sub-bass combining a fundamental sine with an octave-above soft-saturated drive ($1.25s - 0.25s^3$).
   - **Filtering**: Steep low-pass filter (80–220 Hz) to keep the sub-bass punchy, clean, and deep.

4. **Channel 5 — Kick Drum**
   - **Synthesis**: Fast exponential frequency sweep from **145 Hz down to 48 Hz** across a 120 ms envelope.
   - **Transient**: Instant attack ($0.0005\text{ s}$ / 0.5 ms) with soft overdrive saturation to produce a round, tactile punch.

5. **Channel 6 — Snare Drum**
   - **Synthesis**: Dual-component blend:
     - 180 Hz resonant tone body ($45\%$).
     - High-frequency shaped white noise ($55\%$).
   - **Transient**: Instant snap with an 85 ms exponential decay envelope.

6. **Channel 7 — Hi-Hat**
   - **Synthesis**: Metallic phase modulation noise ($70\%$ noise + $30\%$ metallic inharmonic frequencies).
   - **Transient**: Crisp 35 ms transient sizzle.

---

## 4. Physics & Interactive Audio (`PhysicsAudioEngine`)

The physics audio engine converts collisions and body interactions into positional sound events.

### 4.1 Collision Impacts (`SimpleAudioRequest`)
When rigid bodies collide with ground surfaces or other bodies:
- Normal relative impact velocity determines impulse intensity.
- Material IDs select specific timbre palettes (metal, rock, rubber, wood).
- Impact events are pushed to the lock-free ring buffer:
  ```cpp
  WeirdRenderer::SimpleAudioRequest req;
  req.type = WeirdRenderer::SimpleAudioRequest::Type::SineTone;
  req.frequency = 80.0f + intensity * 240.0f;
  req.volume = std::clamp(intensity, 0.05f, 1.0f);
  req.durationSeconds = 0.12f;
  req.pan = calculatedPan; // -1.0 (left) to +1.0 (right)
  services.audio().playSound(req);
  ```

### 4.2 Dynamic Friction Loop
- Sliding and rolling friction calculate tangential velocities between contacting bodies.
- A smoothed atomic float (`services.audio().getFrictionSound()`) feeds a continuous, pitch-modulated noise-resonance synthesizer in the audio thread.

### 4.3 Spatial Audio Processor (`SpatialAudioProcessor`)
- Computes relative distance attenuation (inverse-square law with a minimum distance clamp).
- Constant-power stereo panning based on listener position and orientation:
  $$\text{Gain}_{\text{left}} = \cos\left(\frac{\pi}{4}(1 + \text{pan})\right), \quad \text{Gain}_{\text{right}} = \sin\left(\frac{\pi}{4}(1 + \text{pan})\right)$$
- Doppler pitch shifting based on relative source-listener velocity vectors.

---

## 5. Dynamic Gameplay Audio Hooks

`AudioService` provides high-level reactive hooks to connect game logic to the music engine:

| Method | Effect on Procedural Audio |
|---|---|
| `setTension(float level)` | Increases synth filter cutoffs, adds syncopation, and sharpens melodic intervals (level: 0.0 to 1.0). |
| `setEnergy(float level)` | Modulates tempo multiplier, increases drum velocity, and introduces chord inversions. |
| `setHealth(float cur, float max)` | At low health ($<25\%$), muffles chord filters and introduces an audible sub-bass heartbeat ducking effect. |
| `triggerPositiveFeedback(float intensity)` | Triggers an immediate beat-synced major chord flourish. |
| `triggerNegativeFeedback(float intensity)` | Triggers a low dissonance swell and ducks melodic channels temporarily. |
| `triggerDeath()` | Halts rhythmic percussion and triggers an exponential downward resonant filter sweep. |
| `surge(float amount)` | Temporarily boosts beat energy and drum prominence for dramatic moments. |
| `duck(float amount)` | Ducks music volume during important dialogue or high-impact sound effects. |

---

## 6. Practical Developer Guide

### 6.1 Creating a Scene Song with Automated UI Placement

To create a song for a scene, define the shape using `point()`. No knowledge of window dimensions or center coordinates is required:

```cpp
#include "weird-renderer/audio/SdfSong.h"

class MyScene : public Scene2D
{
public:
	static std::shared_ptr<WeirdRenderer::SdfSong> createSceneSong()
	{
		using namespace SDF;
		// point() provides local coordinates centered at (0, 0)
		Vec2Expr p = point();

		// Construct any 2D SDF shape centered at (0, 0) in p:
		Expr star = sdStar(p, 25.0f, 12.0f, 6.0f, 0.0f);
		Expr core = sdCircle(p, 14.0f);
		Expr shape = sdfSmoothUnion(star, core, 4.0f);

		// SdfSong analyzes the shape to derive tempo, musical scale, and root note
		return WeirdRenderer::SdfSong::create("star_song", shape);
	}

private:
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		// 1. Start playback and spawn top-left UI visualization
		services.audio().setSong(createSceneSong());
	}
};
```

### 6.2 Customizing the Song UI Material
You can customize the color or blend mode of the UI song shape using `SongVisualizationOptions`:

```cpp
auto& songMat = services.materials2D().createMaterial("song_material");
songMat.color = glm::vec4(0.2f, 0.8f, 1.0f, 0.95f); // Glowing cyan

// Pass options to setSong
services.audio().setSong(createSceneSong(), {
	.material = songMat.id,
	.combination = CombinationType::Addition,
	.group = 1
});
```

### 6.3 Binding Real-Time Shape Parameters to Variables
You can bind parameters `var(0)` through `var(7)` to shape expressions and tweak them in real time:

```cpp
static std::shared_ptr<WeirdRenderer::SdfSong> createAnimatedSong()
{
	using namespace SDF;
	Vec2Expr p = point();

	// var(0) = radius, var(1) = displacement, var(2) = points count, var(3) = rotation speed
	Expr star = sdStar(p, Expr(var(0)), Expr(var(1)), Expr(var(2)), Expr(var(3)));

	auto song = WeirdRenderer::SdfSong::create("animated_song", star);
	song->setParameter(0, 30.0f); // Outer radius
	song->setParameter(1, 5.0f);  // Spike amplitude
	song->setParameter(2, 8.0f);  // Star points
	song->setParameter(3, 1.5f);  // Spin speed
	return song;
}

// In onUpdate:
void onUpdate(Registry& registry, ServiceProvider& services) override
{
	// Real-time parameter modulation: updates audio synthesis and UI shader simultaneously
	float spike = 5.0f + 2.5f * std::sin(services.time().time() * 2.0f);
	services.audio().setSongParameter(1, spike);
}
```

---

## 7. API Reference

### `WeirdEngine::AudioService`

| Method | Description |
|---|---|
| `setSong(std::shared_ptr<SdfSong> song, bool beatSynced = true)` | Starts playing a song and creates its top-left UI visualization. |
| `setSong(std::shared_ptr<SdfSong> song, const SongVisualizationOptions& options, bool beatSynced = true)` | Starts playing with custom UI material and combination group. |
| `setSongParameter(size_t index, float value)` | Updates a song parameter (0–7), syncing audio sampling and shader uniform buffers. |
| `getVisualizationEntity()` | Returns the `Entity` ID of the active UI shape. |
| `setSpatialAudioEnabled(bool enabled)` | Enables/disables listener-relative 3D spatial panning and distance attenuation. |
| `playSound(const SimpleAudioRequest& audio)` | Pushes a one-shot sound request to the lock-free audio ring buffer. |
| `setTension(float level)` | Sets gameplay tension (0.0 to 1.0) affecting arrangement density and filter cutoffs. |
| `setEnergy(float level)` | Sets gameplay energy (0.0 to 1.0) affecting tempo and percussion intensity. |
| `setHealth(float current, float max)` | Modulates low-pass filters and heartbeat effects based on player health. |
| `surge(float amount = 0.5f)` | Temporarily surges music energy and volume. |
| `duck(float amount = 0.5f)` | Ducks music volume during dialogue or important sound effects. |
| `getMotionLevel() const` | Returns current geometric motion level measured from the SDF shape. |
| `getMotionNorm() const` | Returns normalized geometric motion level in [0.0, 1.0]. |
| `getFillRatio() const` | Returns domain fill ratio (percentage of volume inside the shape). |

### `WeirdRenderer::SdfSong`

| Method | Description |
|---|---|
| `static create(string name, const Expr& shape, optional<vec2> center = nullopt)` | Creates a procedural song. Center defaults to top-left screen anchor `(70, H - 70)`. |
| `static point()` | Alias for `WeirdEngine::point()`. Returns local `Vec2Expr`. |
| `getTempo() const / setTempo(float bpm)` | Gets or sets playback tempo (automatically calculated from shape if unspecified). |
| `getScale() const / setScale(MusicalScale scale)` | Gets or sets musical scale (`PentatonicMajor`, `PentatonicMinor`, `Major`, `NaturalMinor`, `Dorian`, `Lydian`). |
| `getRootMidi() const / setRootMidi(int root)` | Gets or sets root MIDI note (55–67). |
| `getParameter(size_t i) / setParameter(size_t i, float v)` | Gets or sets dynamic shape variable (indices 0–7). |
| `getCenter() const / setCenter(vec2 center)` | Gets or sets screen anchor coordinate. |
| `getShapeExpression() const` | Returns the bounded AST shape expression ($50\text{ px}$ domain circle applied). |
