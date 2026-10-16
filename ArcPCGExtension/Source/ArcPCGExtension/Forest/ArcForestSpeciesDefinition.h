// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSimTypes.h"
#include "ArcForestSpeciesDefinition.generated.h"

/**
 * ONE authored habitat preference: an axis, a response curve over that axis, and a combine weight.
 *
 * This is the whole of B's authoring surface — a species places where its ecology is satisfied, and
 * "its ecology" is this list. No map is painted: every axis is derived by the node itself (three from
 * the baked surface height grid, one from analytic noise), so a species asset is portable to any volume
 * over any surface.
 *
 * FRuntimeFloatCurve is an ENGINE type (Curves/CurveFloat.h, ENGINE_API). That costs NO new module
 * dependency, verified rather than assumed: this header already includes Engine/DataAsset.h and
 * UArcForestSpeciesDefinition below derives from UDataAsset, so ArcPCGExtension already hard-depends on
 * Engine (ArcPCGExtension.Build.cs lists it in PrivateDependencyModuleNames), and so does the test
 * module that includes this header. (Rejected: FStateTreeConsiderationResponseCurve — it would drag in
 * StateTreeModule for a four-line Evaluate.)
 *
 * NEVER READ FROM THE WORKER. The solver reads the FLATTENED LUT form
 * (FArcForestFieldResponseLut, Forest/ArcForestField.h), produced on the game thread by
 * ArcForestSpeciesHabitat::FlattenFieldResponses before the solve launches. The reason is the editor
 * mutation race spelled out on that struct — a designer dragging curve keys mid-solve mutates
 * FRichCurve::Keys underneath a worker that would be reading it.
 */
USTRUCT(BlueprintType)
struct FArcForestFieldResponse
{
	GENERATED_BODY()

	/** Which self-derived habitat axis this response reads (see EArcForestHabitatField). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Habitat",
		meta = (Tooltip = "Which self-derived habitat axis this response reads: Slope, Sun Exposure, Elevation or Moisture."))
	EArcForestHabitatField Field = EArcForestHabitatField::Slope;

	/**
	 * Preference as a function of the axis value: input is the axis in [0,1], output is a suitability
	 * multiplier in [0,1] (values outside are clamped). Peak the curve where the species thrives and
	 * take it to 0 where it cannot establish.
	 *
	 * A curve with NO KEYS is treated as UNAUTHORED and the whole response is DROPPED (not evaluated as
	 * 0). That is deliberate: FRichCurve::Eval returns its default 0 for an empty curve, so a designer
	 * who added a row and had not yet placed a key would otherwise wipe the species off the map with no
	 * indication why. Author at least one key for the response to take effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Habitat",
		meta = (Tooltip = "Preference curve: X is the habitat axis in 0..1, Y is a suitability multiplier in 0..1. A curve with no keys is ignored entirely."))
	FRuntimeFloatCurve ResponseCurve;

	/**
	 * Compensatory-multiplicative exponent: the curve value is raised to this power before being
	 * multiplied in (the house convention shared with FArcTQSStep / FArcUtilityConsideration).
	 * 0 disables the response entirely, 1 uses the curve as authored, >1 makes it more decisive.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Habitat",
		meta = (ClampMin = "0.0", Tooltip = "How strongly this response counts: 0 ignores it, 1 uses the curve as authored, above 1 makes it more decisive."))
	float Weight = 1.0f;
};

/**
 * One authored species row. All fields are table values fed to the same solver mechanism —
 * an oak and a fern differ only here, never in code path.
 */
USTRUCT(BlueprintType)
struct FArcForestSpeciesEntry
{
	GENERATED_BODY()

	/** Species identifier; also emitted as an output attribute for downstream mesh selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (Tooltip = "Species identifier, emitted for downstream mesh filtering."))
	FName Name;

	/**
	 * The band this species reaches at maturity — AUTHORING / MESH-FILTERING METADATA ONLY.
	 * Per MC1 the solver derives a plant's live competition/light band from its CURRENT height,
	 * NEVER from this field. Do not wire competition, shading, or seeding off MatureLayer — it
	 * exists so the mesh stage can pick a canopy vs ground-cover mesh set for a mature plant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (Tooltip = "Mature form band — authoring/mesh metadata only. Solver uses live height (MC1)."))
	EArcForestLayer MatureLayer = EArcForestLayer::Canopy;

	/** Final height (cm) the plant asymptotes toward as it grows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "Mature height in cm; growth asymptotes here."))
	float MaxHeight = 2000.0f;

	/** Mature horizontal influence radius (cm) in the XY plane (R_max). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "Mature crown radius in cm (XY-plane influence radius)."))
	float MaxCrownRadius = 400.0f;

	/** Height/radius growth increment per iteration under full light (g). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "Growth per iteration under full light."))
	float GrowthRate = 0.15f;

	/**
	 * Incident light fraction below which growth stalls (pioneer high, shade-tolerant low).
	 * Drives growth gating (JABOWA/SORTIE mechanism).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (Tooltip = "Incident light fraction below which growth stalls (pioneer high, shade-tolerant low)."))
	float LightNeeded = 0.5f;

	/**
	 * How well the plant survives sustained low light before mortality.
	 * Drives shade-out mortality (JABOWA/SORTIE mechanism).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (Tooltip = "How well the plant survives sustained low light before mortality."))
	float ShadeTolerance = 0.5f;

	/**
	 * How WIDE each clump of this species is, in cm. This species doesn't scatter evenly — it grows in
	 * clumps, and this is how far its plants spread out from a clump's centre: most land within this
	 * many cm of the centre, a few stray out to ~3x that, and essentially none beyond. Small value =
	 * tight, well-separated clumps with clear gaps; large value = broad, diffuse clumps that overlap
	 * and blend toward an even mix.
	 * (Technically: the sigma of the Gaussian scatter of offspring around each clump centre — the
	 * offspring kernel of the Thomas cluster process the seeder draws clumped darts from.)
	 * ONLY has an effect when the node's WPatch > 0 — that dial sets how many plants are drawn as
	 * clump offspring at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "How far this species' plants spread out from a clump centre, in cm. Small = tight separate clumps; large = broad overlapping clumps. Only matters when the node's WPatch > 0."))
	float PatchRadiusSigma = 300.0f;

	/**
	 * How MANY separate clumps of this species get scattered, per square metre. 0.05 means roughly one
	 * clump centre per 20 m^2. Higher = lots of clumps packed across the map; lower = a few isolated
	 * clumps with empty ground between them. These are just the clump CENTRES (randomly scattered, then
	 * thinned by habitat suitability), and every clumped plant of this species is scattered around one
	 * of them; how far it spreads out from its centre is PatchRadiusSigma.
	 * (Technically: the Poisson intensity of patch parent centres per m^2 in the Thomas cluster
	 * process — the parents the seeder draws clumped darts around.)
	 * ONLY has an effect when the node's WPatch > 0. With no clump centres the species scatters evenly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "How many clump centres of this species per m^2 (0.05 ~= one clump per 20 m^2), each one a place its plants gather around. Higher = more, denser clumps; lower = sparse isolated clumps. Only matters when the node's WPatch > 0."))
	float PatchDensity = 0.05f;

	/**
	 * How MANY plants of THIS species you want per square metre once the stand is mature. 0.1 means
	 * roughly one mature plant of this species every 10 m^2. This is a per-species target and is
	 * INDEPENDENT of the others — raising one species' TargetDensity asks for more of that species; it
	 * does NOT steal ground from any other species (each species is seeded against its own target, then
	 * all of them thin together through competition). Do NOT confuse this with PatchDensity: PatchDensity
	 * counts CLUMP CENTRES per m^2 (how the species is grouped), whereas TargetDensity counts PLANTS per
	 * m^2 (how many individuals of the species there are). A species can have few clumps but many plants,
	 * or many clumps each with few plants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Species",
		meta = (ClampMin = "0.0", Tooltip = "How many plants of THIS species per m^2 at maturity (0.1 ~= one plant per 10 m^2). Independent per species — raising it does not steal ground from others. NOTE: this is plants-per-m^2, unlike PatchDensity which is clumps-per-m^2."))
	float TargetDensity = 0.1f;

	/**
	 * WHERE this species grows — its habitat preferences, one entry per axis it cares about.
	 *
	 * EMPTY (the default) means "grows anywhere the surface allows", which is EXACTLY the pre-habitat
	 * behaviour: no arithmetic is performed at all, so seeding is bit-identical to a build without this
	 * field. Adding entries is what stops the species being scattered uniformly.
	 *
	 * Entries COMBINE MULTIPLICATIVELY (compensatory): the acceptance probability is the surface
	 * suitability times the product of Pow(Clamp(curve(axis), 0, 1), Weight) over every entry. Any one
	 * response reaching 0 excludes the species there regardless of the others — so two responses is a
	 * conjunction ("steep AND dry"), not an average.
	 *
	 * It bites in TWO places, and the first matters more: it thins this species' CLUMP CENTRES (the
	 * parents of the Thomas process — so whole clumps move to the habitat), and it thins individual
	 * darts within a clump.
	 *
	 * A response whose axis was not supplied by the node is SKIPPED, not evaluated at a stand-in value —
	 * see the response-skip rule on ArcForestField::EvaluateHabitatResponse.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Habitat",
		meta = (Tooltip = "Where this species grows: one entry per habitat axis it cares about. Empty means it grows anywhere the surface allows. Entries multiply, so any one reaching zero excludes the species there."))
	TArray<FArcForestFieldResponse> FieldResponses;

	/**
	 * This species' relative share of the per-iteration RECRUITMENT budget, among the species that are
	 * currently MATURE (see MaturityHeightCm). Shares are proportional: two mature species at 1.0 and
	 * 3.0 split the budget 25/75. 0 means this species never recruits.
	 *
	 * Only has an effect when the node's Enable Recruitment is on. It is deliberately SEPARATE from
	 * TargetDensity: that one is the seed-time candidate budget (how much of this species the initial
	 * stand attempts), this one is how much of the ongoing regeneration is this species.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Recruitment",
		meta = (ClampMin = "0.0", Tooltip = "Relative share of the per-iteration recruitment budget among mature species. 0 = never recruits. Only matters when the node's Enable Recruitment is on."))
	float RecruitmentWeight = 1.0f;

	/**
	 * Height (cm) this species must REACH before it starts producing recruits — it contributes to the
	 * recruitment budget only once at least one LIVE plant of the species stands at or above this
	 * height. 0 (the default) means it recruits from the very first iteration.
	 *
	 * This is the reproductive-maturity gate: without it a stand of seedlings would immediately seed a
	 * second generation. Set it near MaxHeight for a slow canopy tree, low or 0 for a ground layer that
	 * spreads as soon as it establishes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Recruitment",
		meta = (ClampMin = "0.0", Tooltip = "Height in cm this species must reach before it produces recruits. 0 = recruits immediately."))
	float MaturityHeightCm = 0.0f;

	/**
	 * WHERE this species' seedlings establish, as a function of the light reaching seedling height:
	 * input is transmittance in [0,1] (0 = deep shade, 1 = full open light), output is a recruitment
	 * preference in [0,1].
	 *
	 * A curve with NO KEYS is treated as UNAUTHORED and the whole shade term is DROPPED (a factor of
	 * exactly 1.0) — same rule, and same reason, as FArcForestFieldResponse::ResponseCurve.
	 *
	 * This is the shade-tolerance authoring surface for REGENERATION, and it is a curve rather than a
	 * reuse of ShadeTolerance precisely because it must express a PREFERENCE: peak it near 0 for a
	 * species whose seedlings only establish under a closed canopy, near 1 for a pioneer that needs a
	 * gap, or in the middle for a species that wants dappled light. ShadeTolerance drives survival
	 * once established and is monotone in light, so it cannot state "prefers dark".
	 *
	 * NEVER READ FROM THE WORKER — flattened to FArcForestShadeResponseLut on the game thread by
	 * ArcForestSpeciesHabitat::FlattenFieldResponses, for the editor mutation race documented there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest|Recruitment",
		meta = (Tooltip = "Recruitment preference curve: X is the light reaching seedling height (0 = deep shade, 1 = full light), Y is a preference multiplier in 0..1. No keys = no shade preference."))
	FRuntimeFloatCurve ShadeResponseCurve;

	// NOTE: this row carries ONLY ecological tuning. It intentionally holds NO mesh/spawn/config
	// reference — the node emits points + descriptive attributes (Species/Layer/…), and the PCG graph
	// selects by attribute and spawns whatever it wants downstream. Do not add a visualization/config
	// soft-path here (that couples the generator to one spawn path — see the node's output contract).
};

/**
 * Data asset holding the full species table for a forest sim run. This is the primary
 * tuning surface — light-response curves, growth rates, and patch parameters are found
 * iteratively by editing rows here.
 */
UCLASS(BlueprintType)
class ARCPCGEXTENSION_API UArcForestSpeciesDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** All species eligible to seed in this forest. Index in this array is the emitted Species id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest",
		meta = (TitleProperty = "Name"))
	TArray<FArcForestSpeciesEntry> Species;
};

namespace ArcForestSpeciesHabitat
{
	/**
	 * GAME THREAD ONLY. Flatten every species' authored curves into the LUT form the worker reads,
	 * writing BOTH FArcForestHabitatInputs::SpeciesResponses (the habitat FieldResponses) and
	 * ::SpeciesShadeResponses (the recruitment ShadeResponseCurve) — both fully overwritten, both sized
	 * to Species.Species.Num(). One pass, one entry point: the two arrays are flattened from the same
	 * species table on the same thread at the same moment, so there is no window in which one is stale.
	 *
	 * This is the ONLY place FRuntimeFloatCurve is touched, and it must stay that way: it runs on the
	 * game thread BEFORE UE::Tasks::Launch, which is what removes the editor mutation race (a designer
	 * dragging curve keys while the solve polls across frames — see FArcForestFieldResponseLut).
	 *
	 * The output array is always sized to Species.Species.Num(), so every species index is valid even if
	 * its response set is empty. WITHIN a species, a response is DROPPED (no LUT emitted) when its curve
	 * carries no keys — an unauthored curve evaluates to FRichCurve's default 0 and would silently
	 * exclude the species everywhere. The per-response LUT samples and Weight are clamped here, so the
	 * worker never has to defend against an out-of-range authored value.
	 *
	 * A species with no usable responses ends up with an EMPTY response set, which
	 * ArcForestField::EvaluateHabitatResponse answers with an exact 1.0 — the bit-identity path.
	 */
	ARCPCGEXTENSION_API void FlattenFieldResponses(
		const UArcForestSpeciesDefinition& Species,
		FArcForestHabitatInputs& OutHabitat);
}
