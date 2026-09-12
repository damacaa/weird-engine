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

`SdfMusicEngine` implements a multi-channel synthesizer rendered at 44.1 kHz, 16-bit stereo:

```
Channel 0: Lead Melody            ──► [2-Pole TPT SVF + Dynamic Pluck Q] ──┐
Channel 1: Sub-Bassline           ──► [2-Pole TPT SVF:  80 -  220 Hz]     ──┤
Channel 2: Direct Wave / FX       ──► [2-Pole TPT SVF]                    ──┤
Channel 3: Noise Generator        ──► [2-Pole TPT SVF]                    ──┤
Channel 4: Chord Pad              ──► [2-Pole TPT SVF: 700 - 2200 Hz]     ──┼──► [Master Bus, Ducking & Tape Saturation] ──► DAC
Channel 5: Kick Drum (Multi-Kit)  ──► [Drive Saturation]                  ──┤
Channel 6: Snare Drum (Multi-Kit) ──► [Transient Envelope]                ──┤
Channel 7: Hi-Hat (Multi-Kit)     ──► [Metallic Noise Transient]          ──┤
Channel 8: UI Positive Click      ──► [Dual Mechanical Snap + Woody Pop]  ──┤ (Bypasses ducking & fill attenuation)
Channel 9: UI Negative Error      ──► [Dissonant Buzzer + Granular Grit]  ──┘ (Bypasses ducking & fill attenuation)
```

### Voice Breakdown

1. **Channel 0 — Lead Melody**
   - **Range**: Expressive vocal/solo singing register (MIDI 57–81 / A3–A5, ~220–880 Hz) protected by strict octave-folding guards to prevent harsh shrieks.
   - **Waveform**: Derived from AST topology (`SoftSine`, `BandlimitedSaw`, `PulseSquare`, `FMPluck`, or `Wavefolder`).
   - **Saturation**: Driven hard into `tanh` overdrive with even-order 2nd harmonic tube warmth (`driven = s * 2.2 + 0.16 * s^2; raw = tanh(driven) * 0.85`) producing singing analog sustain.
   - **Filtering**: 2-pole resonant TPT SVF with dynamic attack pluck envelope ($Q = Q_{\text{base}} + 0.75 \cdot e^{-t / \tau_{\text{env}}}$).
   - **Phrasing & Breathing Rests**: Motifs play on beats 1–2; beats 3–4 (steps 8–15) rest, letting the bass and chord rhythm section breathe. Cadences resolve and hold with singing delayed vibrato (onset after 100ms at 5.4 Hz).

2. **Channel 1 — Sub-Bass**
   - **Range**: Sub octave `-2` (MIDI 31–43, ~40–85 Hz).
   - **Waveform**: Dual-oscillator sub-bass combining a fundamental sine with an octave-above soft-saturated drive. Dynamic overdrive triggers when ducking occurs to cut through heavy mixes.
   - **Filtering**: Steep 2-pole TPT SVF low-pass filter (80–220 Hz) to keep the sub-bass punchy, clean, and deep.

3. **Channel 4 — Chord Pads & Harmony**
   - **Range**: Warm mid register octaves `-1/0` (MIDI 48–60, ~130–260 Hz).
   - **Waveform**: Multi-sine harmonic blend or AST-derived wavefolder/saw.
   - **Filtering**: Warm, mellow 2-pole TPT SVF (700–2,200 Hz) providing acoustic depth without masking dialogue or gameplay sounds.

4. **Channels 5, 6, 7 — Dynamic Percussion Kits**
   - Distributed dynamically across three kits derived from shape AST topology:
     - **Kit 0 (808 Electronic)**: 145 Hz $\to$ 48 Hz pitch sweep kick, punchy sine/noise snare, 35 ms sizzle hat.
     - **Kit 1 (Acoustic Punch)**: 200 Hz sweep kick with fast transient click, 185 Hz resonant wooden snare, crisp tight acoustic hat.
     - **Kit 2 (Industrial 909)**: Overdriven kick with parabolic drive, dual metallic ring snare, sizzle noise hat.

5. **Channel 8 — UI Positive Click (Mechanical Snap & Resonant Acoustic Pop)**
   - **Design**: Crisp tactile mechanical click paired with a warm, resonant wooden/marimba acoustic pop (~75 ms duration).
   - **Dual Micro-Transients**:
     - *Contact Strike ($t = 0$)*: Sharp pitch snap ($1800\text{ Hz} \to \text{base}$ in 2.2 ms) + shaped noise and impulse for instant tactile tick.
     - *Leaf Latch Snap ($t \approx 2.2\text{ ms}$)*: Secondary micro-impulse mimicking mechanical switch latch release.
   - **Acoustic Body**: Melodic wooden/marimba resonance in the sweet UI register ($440–880\text{ Hz}$, MIDI 67–79) ringing out naturally with the voice decay envelope.
   - **UI Bypass**: Bypasses music ducking and fill attenuation so UI clicks remain crisp and audible at all times.

6. **Channel 9 — UI Negative Error (Invalid Input Rejection)**
   - **Design**: Responsive descending minor-second dissonant interval (150 ms duration) anchored in the mid-register (MIDI 58–70) for laptop and mobile speaker clarity.
   - **Synthesis**: Asymmetric saw harmonics, clipped pulse buzzer edge, and wave-synced granular rasp (`noise * |sin(p)|`) with 22% downward pitch sag.
   - **UI Bypass**: Bypasses music ducking and fill attenuation for consistent feedback.

---

## 4. Physics & Interactive Audio (`PhysicsAudioEngine`)

The physics audio engine converts collisions and body interactions into positional sound events.

### 4.1 Collision Impacts (`SimpleAudioRequest`)
When rigid bodies collide with ground surfaces or other bodies:
- Normal relative impact velocity determines impulse intensity.
- Material IDs select specific timbre palettes (metal, rock, rubber, wood).
- Impact events are pushed to the lock-free ring buffer:
  ```cpp
  WeirdAudio::SimpleAudioRequest req;
  req.type = WeirdAudio::SimpleAudioRequest::Type::SineTone;
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

`AudioService` and `SdfMusicEngine` provide high-level reactive hooks to connect gameplay systems and UI interactions to the music engine:

| Method | Effect on Procedural Audio |
|---|---|
| `triggerPositiveFeedback(float intensity)` | Triggers a crisp, responsive mechanical click / thock tuned to the song scale for valid UI actions. |
| `triggerNegativeFeedback(float intensity)` | Triggers a short, clear dissonant rejection buzzer tuned to the song scale for invalid UI actions. |
| `triggerDeath()` | Sequentially shuts down tracks over 6–8 beats with natural note ring-out and zero abrupt voice cutting, fading into organic silence. |
| `resetDynamicEffects()` | Instantly clears death, surge, and ducking state, restoring normal playback. |
| `surge(float amount)` | Temporarily boosts beat energy and drum prominence for dramatic moments. |
| `duck(float amount)` | Ducks music volume during important dialogue or high-impact sound effects. |

### Sequential Death Sequence (Organic Decay & Natural Silence)
When `triggerDeath()` is called:
- **No Artificial Plunges or Glitches**: Rather than playing harsh synthetic pitch drops or dissonant drone layers, the engine disassembles the active arrangement track by track.
- **Natural Ring-Out (Zero Abrupt Choking)**: When a track is killed, it ceases scheduling new notes. All existing voices ring out according to their natural physical decay envelopes without being prematurely truncated.
- **Sequential Timeline**:
  1. *Immediate (Stage 1)*: Lead melody stops queueing notes; singing leads fade naturally.
  2. *2 Beats Later (Stage 2)*: Drums stop triggering; cymbal and snare tails ring out and decay.
  3. *2 Beats Later (Stage 3)*: Chord pads stop triggering; warm harmonic sustain fades.
  4. *2 Beats Later (Stage 4)*: Sub-bass stops; deep low frequencies resonate out.
  5. *Natural Silence*: With all tracks stopped, a smooth master fade ($\exp(-t / 1.5\text{ s})$) silences any residual tails.
- **Visual State Sync**: Track states (`TrackPlayState::Dead`) update dynamically in the editor inspector badges.

---

## 6. Practical Developer Guide

### 6.1 Creating a Scene Song with Automated UI Placement

To create a song for a scene, define the shape using `point()`. No knowledge of window dimensions or center coordinates is required:

```cpp
#include "weird-audio/SdfSong.h"

class MyScene : public Scene2D
{
public:
	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		// point() provides local coordinates centered at (0, 0)
		Vec2Expr p = point();

		// Construct any 2D SDF shape centered at (0, 0) in p:
		Expr star = sdStar(p, 25.0f, 12.0f, 6.0f, 0.0f);
		Expr core = sdCircle(p, 14.0f);
		Expr shape = sdfSmoothUnion(star, core, 4.0f);

		// SdfSong analyzes the shape to derive tempo, musical scale, and root note
		return WeirdAudio::SdfSong::create("star_song", shape);
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
static std::shared_ptr<WeirdAudio::SdfSong> createAnimatedSong()
{
	using namespace SDF;
	Vec2Expr p = point();

	// var(0) = radius, var(1) = displacement, var(2) = points count, var(3) = rotation speed
	Expr star = sdStar(p, Expr(var(0)), Expr(var(1)), Expr(var(2)), Expr(var(3)));

	auto song = WeirdAudio::SdfSong::create("animated_song", star);
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
| `surge(float amount = 0.5f)` | Temporarily surges music energy and volume. |
| `duck(float amount = 0.5f)` | Ducks music volume during dialogue or important sound effects. |
| `triggerPositiveFeedback(float intensity)` | Triggers a crisp, responsive mechanical click / thock for valid UI actions. |
| `triggerNegativeFeedback(float intensity)` | Triggers a dissonant rejection buzzer for invalid UI actions. |
| `triggerDeath()` | Triggers sequential track shutdown (Lead $\to$ Drums $\to$ Pad $\to$ Bass) fading into natural silence. |
| `resetDynamicEffects()` | Clears death, surge, and ducking state, restoring normal playback. |
| `getMotionLevel() const` | Returns current geometric motion level measured from the SDF shape. |
| `getMotionNorm() const` | Returns normalized geometric motion level in [0.0, 1.0]. |
| `getFillRatio() const` | Returns domain fill ratio (percentage of volume inside the shape). |
| `getTempo() const` | Returns current dynamic playback tempo in BPM. |
| `getTimeBetweenBeats() const` | Returns duration of one quarter-note beat in seconds ($60.0 / \text{BPM}$). |

### `WeirdAudio::SdfSong`

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
| `getFingerprint() const` | Returns `ASTFingerprint` with topological metrics (`structuralHash`, `nodeCount`, `maxDepth`, `branchCount`). |

---

## 8. AST Topology & Procedural Instrument Selection

To provide timbral variety across different songs while keeping sound identity stable under real-time parameter changes, the procedural engine decouples **instrument selection** from **real-time performance**:

1. **AST Topology (Instrument Rack — Who Plays)**:
   - Evaluated **once** when an `SdfSong` is instantiated by traversing `m_rawShapeExpression`.
   - Trivial leaves (`FloatConstant` and `FloatVariable` `var0`..`var7`) are skipped, guaranteeing that slider tweaks or runtime animations **never change the instrument selection**.
   - Traverses tree hierarchy (node count, depth, branching, parent-child links) to compute a deterministic topological seed (`structuralHash`).
   - Pure structural topology: no complex node-type balancing or fragile ratios. Changing the tree structure (adding a node, changing a connection, deeper nesting) rolls a completely different instrument rack.

2. **Instrument Rack Assignment**:
   - `SdfMusicEngine::selectInstrumentRack()` uses `structuralHash` as a deterministic PRNG seed (salting for lead, bass, pad, and drum kit). Any change in node composition or tree graph topology selects a completely different instrument ensemble:
     - **Lead Waveform**: Selects uniformly across `SoftSine`, `BandlimitedSaw`, `PulseSquare`, `FMPluck`, and `Wavefolder`.
     - **Bass Waveform**: Selects across `SoftSine`, `BandlimitedSaw`, `PulseSquare`, and `FMPluck`.
     - **Pad Waveform**: Selects across `SoftSine`, `Wavefolder`, `BandlimitedSaw`, and `PulseSquare`.
     - **Percussion Kit**: Distributes uniformly across all 3 kits:
       - **Kit 0**: Deep 808 Electronic (sub kick, snappy snare, FM hat)
       - **Kit 1**: Acoustic Punch (200 Hz sweep kick, resonant wood snare, crisp hat)
       - **Kit 2**: Industrial / 909 (saturated overdriven kick, metallic ring snare, sizzle hat)
     - **Synthesis Parameters**: Pulse width, FM mod index, and wavefolder drive are derived from salted hash entropy, ensuring distinct sonic flavor for each shape.

3. **Geometry Probes (Conductor — What They Play)**:
   - Compass probes (`melodyDensity`, `harmonyRichness`, `brightness`, `syncopation`, `tempoFactor`) and domain motion/fill continuously modulate filter cutoff frequencies, note density, tempo, and octave registers in real time without altering the instrument rack.

---

## 9. Real-Time DSP Hardening & Filter Architecture

To satisfy strict real-time audio thread constraints (glitch-free 44.1 kHz execution without priority inversions or buffer underruns), `SdfMusicEngine` implements hardened DSP primitives:

### 9.1 Fast Lock-Free XorShift32 RNG (`fastNoise`)
- Standard C library `std::rand()` acquires a global mutex (`__libc_lock_lock`) in POSIX `glibc`, introducing thread contention, cache thrashing, and potential audio dropouts when called inside per-sample inner loops.
- Replaced with an inline, branchless XorShift32 PRNG with per-voice state (`rngState`):
  ```cpp
  static inline float fastNoise(uint32_t& state)
  {
      if (state == 0) state = 123456789u;
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      return (static_cast<float>(state & 0x00FFFFFF) / 8388607.0f) - 1.0f;
  }
  ```
- Guaranteed lock-free, deterministic, and executes in ~3 clock cycles.

### 9.2 Normalized Phase Accumulator $[0.0, 1.0)$
- Switched oscillator phase accumulators from radians $[0, 2\pi)$ to normalized cycles $[0.0, 1.0)$:
  ```cpp
  const float phaseInc = currentFreq / sampleRateF;
  voice.phase += phaseInc;
  if (voice.phase >= 1.0f) voice.phase -= 1.0f;
  ```
- Eliminates floating-point phase creep and removes repeated $2\pi$ multiplications inside inner sample loops. Radian conversion ($p = \text{phase} \times 2\pi$) is performed only where trigonometric evaluation is explicitly required.

### 9.3 2-Pole Resonant Topology-Preserving Transform (TPT) SVF
- Upgraded synth voice filtering from a 1-pole 6 dB/octave RC filter to a 2-pole resonant Topology-Preserving Transform State Variable Filter (12 dB/octave attenuation):
  ```cpp
  // Precalculated outside sample loop:
  float g = std::tan(M_PI * cutoff / sampleRateF);
  float k = 1.0f / Q;
  float a1 = 1.0f / (1.0f + g * (g + k));
  float a2 = g * a1;

  // Per sample:
  float v0 = rawSample;
  float v1 = a1 * s1 + a2 * (v0 - s2);
  float v2 = s2 + g * v1; // 12 dB/oct resonant low-pass output
  s1 = 2.0f * v1 - s1;
  s2 = 2.0f * v2 - s2;
  ```
- **Unconditional Stability**: Stable across all cutoff frequencies up to Nyquist without distortion, phase delay warping, or self-oscillation blowup.
- **Dynamic Pluck Envelope**: Lead voices dynamically increase resonance ($Q$) during the initial attack transient ($Q = Q_{\text{base}} + 0.75 \cdot e^{-t / \tau}$) for punchy acoustic bite.
- **Denormal Flushing**: Internal filter states (`s1`, `s2`) are flushed to zero when under $10^{-15}$ to prevent CPU denormal floating-point penalties.
