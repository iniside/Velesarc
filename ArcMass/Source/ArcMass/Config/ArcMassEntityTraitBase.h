// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "MassCoreConcepts.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;
class UWorld;

/** Build context passed to UArcMassEntityTraitBase::BuildSparseFragments. */
struct FArcSparseFragmentBuildContext
{
	TArray<FInstancedStruct> SparseFragments;
	TArray<const UScriptStruct*> SparseTagTypes;

	template<UE::Mass::CSparse T>
	void AddSparseFragment(const T& Value = T{})
	{
		SparseFragments.Add(FInstancedStruct::Make(Value));
	}

	void AddSparseFragment(FConstStructView View)
	{
		SparseFragments.Emplace(View);
	}

	void AddSparseTag(TNotNull<const UScriptStruct*> Type)
	{
		SparseTagTypes.Add(Type);
	}
};

#include "ArcMassEntityTraitBase.generated.h"

/**
 * Base class for ArcMass entity traits that need to seed sparse fragments
 * or perform post-spawn entity initialization.
 */
UCLASS(Abstract)
class ARCMASS_API UArcMassEntityTraitBase : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	// Concrete no-op override so sparse-only subclasses don't inherit the engine pure-virtual.
	// Subclasses that need regular fragments/tags still override this.
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override {}

	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const {}
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const {}
};

struct FMassEntityConfig;

namespace ArcMass
{
	// The engine template is built from the whole GetParent() chain, so GetTraits() alone — this
	// config's own array — silently drops every inherited trait; GetCombinedTraits is protected.
	// World gates on ValidTargetConfig as FMassEntityTemplateBuildContext::BuildFromTraits does; null skips the gate.
	ARCMASS_API void CollectCombinedTraits(const FMassEntityConfig& InConfig, const UWorld* World,
		TArray<UMassEntityTraitBase*>& OutTraits);
}
