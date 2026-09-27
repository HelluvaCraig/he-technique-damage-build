# HE Technique Damage build

Build-only repository for the Elden Rim physical Technique damage replacement DLL.

The plugin uses Precision's official pre-hit API. Physical Technique Stones receive a whole-move damage budget of:
- T1: 4.5x weapon damage
- T2: 7x weapon damage
- T3: 10x weapon damage

The budget is divided by the Technique's expected damaging contacts. Precision keeps the actual attacking hand/weapon for weapon collisions; ambiguous/body contacts use the normal right-hand fallback path.

Build trigger branch for CI validation.
