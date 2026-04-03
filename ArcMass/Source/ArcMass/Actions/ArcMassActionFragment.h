// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "MassExternalSubsystemTraits.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassActionFragment.generated.h"

class UArcMassStatelessActionAsset;

USTRUCT()
struct ARCMASS_API FArcMassStatefulActionList
{
	GENERATED_BODY()

	UPROPERTY(meta = (BaseStruct = "/Script/ArcMass.ArcMassStatefulAction"))
	TArray<FInstancedStruct> Actions;
};

USTRUCT()
struct ARCMASS_API FArcMassActionFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	/** Per-signal stateless action assets — shared, read-only at runtime. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>> StatelessActions;

	/** Per-signal stateful actions — copied from asset at spawn, mutable per-entity. */
	UPROPERTY()
	TMap<FName, FArcMassStatefulActionList> StatefulActions;
};

template<>
struct TMassFragmentTraits<FArcMassActionFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
