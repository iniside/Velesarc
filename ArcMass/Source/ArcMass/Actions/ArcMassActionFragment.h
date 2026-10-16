// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassActionFragment.generated.h"

class UArcMassStatelessActionAsset;
class UArcMassEntityConfigAsset;
class UWorld;
class FArcMassActionData;
struct FArcMassActionContext;
struct FArcMassActionPreparationContext;

USTRUCT()
struct ARCMASS_API FArcMassStatefulActionList
{
	GENERATED_BODY()

	UPROPERTY(meta = (BaseStruct = "/Script/ArcMass.ArcMassStatefulAction"))
	TArray<FInstancedStruct> Actions;

	bool Prepare(const FArcMassActionPreparationContext& Context);
};

USTRUCT()
struct FArcMassActionStorage
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>> StatelessActions;

	UPROPERTY()
	TMap<FName, FArcMassStatefulActionList> StatefulActions;
};

USTRUCT()
struct ARCMASS_API FArcMassActionFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	FArcMassActionFragment();
	FArcMassActionFragment(const FArcMassActionFragment& Other);
	FArcMassActionFragment(FArcMassActionFragment&& Other) = default;
	FArcMassActionFragment& operator=(const FArcMassActionFragment& Other);
	FArcMassActionFragment& operator=(FArcMassActionFragment&& Other) = default;
	bool Serialize(FArchive& Ar);
	bool Identical(const FArcMassActionFragment* Other, uint32 PortFlags) const;

	const TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>>& GetStatelessActions() const;
	TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>>& GetMutableStatelessActions();
	const TMap<FName, FArcMassStatefulActionList>& GetStatefulActions() const;
	TMap<FName, FArcMassStatefulActionList>& GetMutableStatefulActions();

	void Execute(FName EventName, FArcMassActionContext& Context);
	void PostExecute(FName EventName, FArcMassActionContext& Context);

private:
	TSharedPtr<FArcMassActionData> ActionData;
};

template<>
struct TMassFragmentTraits<FArcMassActionFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template<>
struct TStructOpsTypeTraits<FArcMassActionFragment> : public TStructOpsTypeTraitsBase2<FArcMassActionFragment>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
	};
};

namespace UE::ArcMass::Actions
{
	ARCMASS_API FArcMassActionFragment BuildInitialActionFragment(
		const UArcMassEntityConfigAsset& Config,
		const UWorld& World);
}
