# Disyn Manual

Disyn is a monophonic distortion synthesizer with 19 algorithms. The algorithm selector chooses a synthesis core, while three parameter knobs are mapped per-algorithm.

## Controls

- Algorithm: selects the synthesis algorithm (0-18).
- Param 1: algorithm-specific control.
- Param 2: algorithm-specific control.
- Param 3: algorithm-specific control.
- Attack: envelope attack.
- Release: envelope release.
- Reverb Size: Schroeder reverb size.
- Reverb Level: wet/dry mix.
- Master Gain: output level.

Note: the UI labels for Param 1/2/3 currently read Sides/Launch/Jitter (Trajectory defaults). For all other algorithms, treat them as Param 1/2/3 per the table below.

## Algorithms and Parameter Mapping

0. Dirichlet (Dirichlet Pulse)
   - Param 1: Harmonics (1-64)
   - Param 2: Tilt (-3 to +15 dB/oct)
   - Param 3: Drive/soft-clip blend

1. DSF 1 (Single-Sided DSF)
   - Param 1: Decay (0-0.98)
   - Param 2: Ratio (0.5-4)
   - Param 3: Blend DSF -> sine

2. DSF 2 (Double-Sided DSF)
   - Param 1: Decay (0-0.96)
   - Param 2: Ratio (0.5-4.5)
   - Param 3: Balance positive vs negative DSF components

3. Tanh Sq (Tanh Square)
   - Param 1: Drive (0.05-5)
   - Param 2: Trim (0.2-1.2)
   - Param 3: DC bias (pulse-width style asymmetry)

4. Tanh Saw (Tanh Saw)
   - Param 1: Drive (0.05-4.5)
   - Param 2: Blend (square -> saw)
   - Param 3: Edge emphasis

5. PAF (Phase-Aligned Formant)
   - Param 1: Formant ratio (0.5-6 x f0)
   - Param 2: Bandwidth (50-3000 Hz)
   - Param 3: Modulation depth

6. Mod FM (Modified FM)
   - Param 1: Index (0.01-8)
   - Param 2: Ratio (0.25-6)
   - Param 3: Modulator feedback

7. Hybrid (Combination 1: Hybrid Formant)
   - Param 1: ModFM index
   - Param 2: Reserved (unused)
   - Param 3: Formant spacing

8. Cascade (Combination 2: Cascaded Spectral)
   - Param 1: DSF decay
   - Param 2: AsymFM ratio
   - Param 3: Tanh drive

9. Parallel (Combination 3: Parallel Bank)
   - Param 1: ModFM index
   - Param 2: Reserved (unused)
   - Param 3: Mix (ModFM <-> PAF)

10. Feedback (Combination 4: Feedback Network)
    - Param 1: ModFM index
    - Param 2: Feedback amount
    - Param 3: Post-shape drive

11. Morph (Combination 5: Morphing Engine)
    - Param 1: Morph position (DSF <-> ModFM <-> PAF)
    - Param 2: Character
    - Param 3: Morph curve (skew)

12. Inharm (Combination 6: Inharmonic Resonator)
    - Param 1: DSF decay
    - Param 2: PAF shift
    - Param 3: DSF <-> PAF mix

13. Adaptive (Combination 7: Adaptive Filter)
    - Param 1: Cutoff
    - Param 2: Resonance
    - Param 3: DSF <-> ModFM mix

14. Multi (Novel 1: Multistage)
    - Param 1: Tanh drive
    - Param 2: Exp depth
    - Param 3: Ring mod ratio

15. Asym (Novel 2: Freq Asymmetry)
    - Param 1: Low-band asymmetry
    - Param 2: High-band asymmetry
    - Param 3: AsymFM index

16. Cross (Novel 3: Cross Mod)
    - Param 1: DSF -> ModFM depth
    - Param 2: ModFM -> DSF depth
    - Param 3: DSF <-> ModFM mix

17. Taylor (Novel 4: Taylor Series)
    - Param 1: Fundamental terms
    - Param 2: 2nd harmonic terms
    - Param 3: Blend

18. Trajectory (Polygonal Trajectory)
    - Param 1: Sides (3-12)
    - Param 2: Launch Angle (0-360 deg)
    - Param 3: Jitter (0-10 deg)
    - Output: stereo x/y position

## Output

Most algorithms output a single mono signal (duplicated to both channels). The Trajectory algorithm outputs x (left) and y (right).
