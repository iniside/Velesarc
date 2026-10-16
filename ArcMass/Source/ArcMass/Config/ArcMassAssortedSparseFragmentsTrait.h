// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassAssortedSparseFragmentsTrait.generated.h"

/**
 * Sparse-side counterpart of UMassAssortedFragmentsTrait.
 *
 * Lets authors pick any number of FMassSparseFragment-derived structs
 * (with editable default values) and FMassSparseTag-derived types on a
 * single trait, instead of writing one trait subclass per combination.
 */
UCLASS(MinimalAPI, meta=(DisplayName="Assorted Sparse Fragments"))
class UArcMassAssortedSparseFragmentsTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;

	UPROPERTY(Category="Mass", EditAnywhere,
		meta=(BaseStruct="/Script/MassCore.MassSparseFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> SparseFragments;

	UPROPERTY(Category="Mass", EditAnywhere,
		meta=(BaseStruct="/Script/MassCore.MassSparseTag", ExcludeBaseStruct))
	TArray<FInstancedStruct> SparseTags;
};
