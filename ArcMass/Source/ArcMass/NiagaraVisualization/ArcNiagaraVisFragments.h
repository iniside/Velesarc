// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcNiagaraVisFragments.generated.h"

class UNiagaraSystem;
struct FArcNiagaraRenderStateHelper;

// ---------------------------------------------------------------------------
// Tag — archetype marker for Niagara visualization entities
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcNiagaraVisTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Config — const shared fragment (per-archetype)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcNiagaraVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Niagara system to spawn per entity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** Whether the Niagara system casts shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	bool bCastShadow = false;

	/** Use fixed bounds instead of dynamic bounds from simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	bool bFixedBounds = false;

	/** Fixed bounds to use when bFixedBounds is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization", meta = (EditCondition = "bFixedBounds"))
	FBox FixedBounds = FBox(ForceInit);

	/** Local-space offset applied to the Niagara system relative to the entity origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	FTransform ComponentTransform = FTransform::Identity;
};

template<>
struct TMassFragmentTraits<FArcNiagaraVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Per-entity mutable state — owns the render state helper
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcNiagaraVisFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcNiagaraVisFragment() = default;
	~FArcNiagaraVisFragment();
	FArcNiagaraVisFragment(FArcNiagaraVisFragment&& Other);
	FArcNiagaraVisFragment(const FArcNiagaraVisFragment&) = delete;
	FArcNiagaraVisFragment& operator=(const FArcNiagaraVisFragment&) = delete;
	FArcNiagaraVisFragment& operator=(FArcNiagaraVisFragment&&) = delete;

	FArcNiagaraRenderStateHelper* RenderStateHelper = nullptr;
};

template<>
struct TStructOpsTypeTraits<FArcNiagaraVisFragment> : TStructOpsTypeTraitsBase2<FArcNiagaraVisFragment>
{
	enum
	{
		WithCopy = false,
	};
};

template<>
struct TMassFragmentTraits<FArcNiagaraVisFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Per-entity Niagara parameter overrides (optional)
//
// Gameplay code writes parameter values here; the visualization processor
// applies them to the Niagara system instance when bDirty is set, then
// clears the flag. Values persist in the Niagara system between applies —
// only set bDirty when a value actually changes.
//
// Usage:
//   auto& Params = EntityManager.GetFragmentDataChecked<FArcNiagaraVisParamsFragment>(Entity);
//   Params.SetFloat(FName("FireIntensity"), 0.8f);
//   Params.SetColor(FName("FlameColor"), FLinearColor::Red);
//
// Parameter names must match User Parameter names in the Niagara system asset.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcNiagaraVisParamsFragment : public FMassFragment
{
	GENERATED_BODY()

	/** True when any parameter value has changed and needs to be applied.
	 *  Set automatically by the setter methods. Cleared by the processor. */
	bool bDirty = false;

	// Scalar types
	TMap<FName, float> FloatParams;
	TMap<FName, int32> IntParams;
	TMap<FName, bool> BoolParams;

	// Vector types (double precision input, converted to single precision for Niagara)
	TMap<FName, FVector2D> Vector2DParams;
	TMap<FName, FVector> VectorParams;
	TMap<FName, FVector4> Vector4Params;

	// Color & rotation
	TMap<FName, FLinearColor> ColorParams;
	TMap<FName, FQuat> QuatParams;

	void SetFloat(FName Name, float Value) { FloatParams.Add(Name, Value); bDirty = true; }
	void SetInt(FName Name, int32 Value) { IntParams.Add(Name, Value); bDirty = true; }
	void SetBool(FName Name, bool Value) { BoolParams.Add(Name, Value); bDirty = true; }
	void SetVector2D(FName Name, const FVector2D& Value) { Vector2DParams.Add(Name, Value); bDirty = true; }
	void SetVector(FName Name, const FVector& Value) { VectorParams.Add(Name, Value); bDirty = true; }
	void SetVector4(FName Name, const FVector4& Value) { Vector4Params.Add(Name, Value); bDirty = true; }
	void SetColor(FName Name, const FLinearColor& Value) { ColorParams.Add(Name, Value); bDirty = true; }
	void SetQuat(FName Name, const FQuat& Value) { QuatParams.Add(Name, Value); bDirty = true; }
};

template<>
struct TMassFragmentTraits<FArcNiagaraVisParamsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
