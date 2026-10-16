// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Config/ArcMassAssortedSparseFragmentsTrait.h"

#include "Mass/EntityElementTypes.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassAssortedSparseFragmentsTrait)

void UArcMassAssortedSparseFragmentsTrait::BuildSparseFragments(
	FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	for (const FInstancedStruct& F : SparseFragments)
	{
		if (!F.IsValid())
		{
			continue;
		}
		const UScriptStruct* Type = F.GetScriptStruct();
		if (UE::Mass::IsA<FMassSparseFragment>(Type))
		{
			Out.AddSparseFragment(FConstStructView(F));
		}
		else
		{
			UE_LOGF(LogMass, Error, "Struct type %ls is not a child of FMassSparseFragment", *GetPathNameSafe(Type));
		}
	}

	for (const FInstancedStruct& T : SparseTags)
	{
		const UScriptStruct* Type = T.GetScriptStruct();
		if (UE::Mass::IsA<FMassSparseTag>(Type))
		{
			Out.AddSparseTag(Type);
		}
		else
		{
			UE_LOGF(LogMass, Error, "Struct type %ls is not a child of FMassSparseTag", *GetPathNameSafe(Type));
		}
	}
}
