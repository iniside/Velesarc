# ArcNeeds Plugin

Needs/motivation system (hunger, fatigue and similar), driven by `GameDesign/mechanics/settlement/needs.yaml`.

- Need bands (`FArcNeedBand`, `FArcNeedBandTableFragment` in `Fragments/ArcNeedBands.h`) are deliberately a separate shape from ArcMassMechanics `FArcTierBands` / meters: half-open `[Min, Max)` ranges with a different driver. Do not merge them.
