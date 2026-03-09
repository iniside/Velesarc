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
