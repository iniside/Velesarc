// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Effects/ArcAttributeCapture.h"

struct FArcEffectContext;
struct FMassEntityManager;
struct FArcEvaluateParameters;

#include "ArcExtraCapture.generated.h"

/** Abstract polymorphic resolver for an "extra" capture. Subclasses know how to obtain a float
 *  from any source (source entity, target entity, held item, …) given the effect context.
 *  Subclasses that need an attribute identifier carry it as their own UPROPERTY. */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcExtraCaptureSource
{
	GENERATED_BODY()

	virtual ~FArcExtraCaptureSource() = default;

	/** Produce a float value from Context/EntityManager.
	 *  Return false to signal resolve failure (snapshot entry will have bHasValue=false;
	 *  live resolve returns an unset TOptional). */
	virtual bool ResolveValue(
		const FArcEffectContext& Context,
		FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams,
		float& OutValue) const PURE_VIRTUAL(FArcExtraCaptureSource::ResolveValue, return false;);

	virtual EArcCaptureSource GetCaptureSide() const { return EArcCaptureSource::Source; }
};

/** One authored extra-capture entry on FArcEffectExecution. Bundles the polymorphic source
 *  resolver (who supplies the value) with the snapshot flag. Resolvers that need an attribute
 *  identifier carry it as their own UPROPERTY inside the Source payload. */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcExtraCaptureDef
{
	GENERATED_BODY()

	/** Polymorphic resolver that knows how to extract a float from the effect context. */
	UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/ArcMassAbilities.ArcExtraCaptureSource", ExcludeBaseStruct))
	FInstancedStruct Source;

	/** True: resolve once at FArcEffectSpec construction and cache the float.
	 *  False: do not resolve at MakeSpec time; re-resolve on each AttemptResolveExtraCapture call. */
	UPROPERTY(EditAnywhere)
	bool bSnapshot = true;

	bool operator==(const FArcExtraCaptureDef& Other) const
	{
		// Null-guard: a blank Source (default-constructed FInstancedStruct, no UScriptStruct set)
		// would crash FInstancedStruct::operator== inside CompareScriptStruct. Treat both-null as
		// equal-source, one-null-one-not as unequal.
		const UScriptStruct* SelfType = Source.GetScriptStruct();
		const UScriptStruct* OtherType = Other.Source.GetScriptStruct();
		if (SelfType != OtherType) return false;
		if (SelfType != nullptr && !(Source == Other.Source)) return false;  // deep payload compare via CompareScriptStruct
		return bSnapshot == Other.bSnapshot;
	}

	friend uint32 GetTypeHash(const FArcExtraCaptureDef& Def)
	{
		// Hash on (Source script-struct pointer, bSnapshot). Payload differences
		// collide here and fall through to operator==. Matches FArcAttributeCaptureDef's
		// compromise on hash-vs-equality coverage.
		return HashCombine(
			GetTypeHash(Def.Source.GetScriptStruct()),
			GetTypeHash(Def.bSnapshot));
	}
};
