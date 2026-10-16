// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSpeciesDefinition.h"

/**
 * Shared builders for the ArcPCGExtension forest-sim tests. Header-scoped (shared namespace is fine
 * in a .h per the C++ style rule) so the pure-core and PCG-smoke test .cpp files construct identical
 * in-memory species tables without duplicating the field-by-field setup.
 *
 * The species definition is kept alive via TStrongObjectPtr so a mid-test GC cannot collect the
 * transient UArcForestSpeciesDefinition while a Simulate/Build call is reading it.
 */
namespace ArcForestTestHelpers
{
	/** Build one species row from its scalar knobs (the same fields the editor authors). */
	inline FArcForestSpeciesEntry MakeSpecies(
		FName Name,
		EArcForestLayer MatureLayer,
		float MaxHeight,
		float MaxCrownRadius,
		float GrowthRate,
		float TargetDensity,
		float PatchRadiusSigma,
		float PatchDensity)
	{
		FArcForestSpeciesEntry Entry;
		Entry.Name = Name;
		Entry.MatureLayer = MatureLayer;
		Entry.MaxHeight = MaxHeight;
		Entry.MaxCrownRadius = MaxCrownRadius;
		Entry.GrowthRate = GrowthRate;
		Entry.TargetDensity = TargetDensity;
		Entry.PatchRadiusSigma = PatchRadiusSigma;
		Entry.PatchDensity = PatchDensity;
		return Entry;
	}

	/** Wrap a species table in a GC-rooted transient data asset (index in the array = Species id). */
	inline TStrongObjectPtr<UArcForestSpeciesDefinition> MakeDefinition(TArray<FArcForestSpeciesEntry> Entries)
	{
		UArcForestSpeciesDefinition* Def = NewObject<UArcForestSpeciesDefinition>();
		Def->Species = MoveTemp(Entries);
		return TStrongObjectPtr<UArcForestSpeciesDefinition>(Def);
	}

	/** A square XY footprint centred on the origin, HalfSizeCm from centre to edge on each axis. */
	inline FBox2f CentredBounds(float HalfSizeCm)
	{
		return FBox2f(FVector2f(-HalfSizeCm, -HalfSizeCm), FVector2f(HalfSizeCm, HalfSizeCm));
	}
}
