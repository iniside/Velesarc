// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActionFragment.h"

#include "ArcMassAction.h"
#include "ArcMassActionAsset.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "UObject/GCObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassActionFragment)

class FArcMassActionData final : public FGCObject
{
public:
	FArcMassActionStorage Storage;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
};

bool FArcMassStatefulActionList::Prepare(const FArcMassActionPreparationContext& Context)
{
	for (FInstancedStruct& ActionStruct : Actions)
	{
		FArcMassStatefulAction* Action = ActionStruct.GetMutablePtr<FArcMassStatefulAction>();
		if (Action && !Action->Prepare(Context))
		{
			return false;
		}
	}

	return true;
}

void FArcMassActionData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(FArcMassActionStorage::StaticStruct(), &Storage);
}

FString FArcMassActionData::GetReferencerName() const
{
	return TEXT("FArcMassActionData");
}

FArcMassActionFragment::FArcMassActionFragment()
	: ActionData(MakeShared<FArcMassActionData>())
{
}

FArcMassActionFragment::FArcMassActionFragment(const FArcMassActionFragment& Other)
	: ActionData(MakeShared<FArcMassActionData>(*Other.ActionData))
{
}

FArcMassActionFragment& FArcMassActionFragment::operator=(const FArcMassActionFragment& Other)
{
	if (this != &Other)
	{
		ActionData = MakeShared<FArcMassActionData>(*Other.ActionData);
	}
	return *this;
}

bool FArcMassActionFragment::Serialize(FArchive& Ar)
{
	FArcMassActionStorage::StaticStruct()->SerializeItem(Ar, &ActionData->Storage, nullptr);
	return true;
}

bool FArcMassActionFragment::Identical(const FArcMassActionFragment* Other, uint32 PortFlags) const
{
	return Other && FArcMassActionStorage::StaticStruct()->CompareScriptStruct(
		&ActionData->Storage, &Other->ActionData->Storage, PortFlags);
}

const TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>>& FArcMassActionFragment::GetStatelessActions() const
{
	return ActionData->Storage.StatelessActions;
}

TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>>& FArcMassActionFragment::GetMutableStatelessActions()
{
	return ActionData->Storage.StatelessActions;
}

const TMap<FName, FArcMassStatefulActionList>& FArcMassActionFragment::GetStatefulActions() const
{
	return ActionData->Storage.StatefulActions;
}

TMap<FName, FArcMassStatefulActionList>& FArcMassActionFragment::GetMutableStatefulActions()
{
	return ActionData->Storage.StatefulActions;
}

void FArcMassActionFragment::Execute(FName EventName, FArcMassActionContext& Context)
{
	TObjectPtr<UArcMassStatelessActionAsset>* StatelessAsset = ActionData->Storage.StatelessActions.Find(EventName);
	if (StatelessAsset && *StatelessAsset)
	{
		for (const FInstancedStruct& ActionStruct : (*StatelessAsset)->Actions)
		{
			const FArcMassStatelessAction* Action = ActionStruct.GetPtr<FArcMassStatelessAction>();
			if (Action)
			{
				Action->Execute(Context);
			}
		}
	}

	FArcMassStatefulActionList* StatefulList = ActionData->Storage.StatefulActions.Find(EventName);
	if (StatefulList)
	{
		for (FInstancedStruct& ActionStruct : StatefulList->Actions)
		{
			FArcMassStatefulAction* Action = ActionStruct.GetMutablePtr<FArcMassStatefulAction>();
			if (Action)
			{
				Action->Execute(Context);
			}
		}
	}
}

void FArcMassActionFragment::PostExecute(FName EventName, FArcMassActionContext& Context)
{
	TObjectPtr<UArcMassStatelessActionAsset>* StatelessAsset = ActionData->Storage.StatelessActions.Find(EventName);
	if (StatelessAsset && *StatelessAsset)
	{
		for (const FInstancedStruct& ActionStruct : (*StatelessAsset)->Actions)
		{
			const FArcMassStatelessAction* Action = ActionStruct.GetPtr<FArcMassStatelessAction>();
			if (Action)
			{
				Action->PostExecute(Context);
			}
		}
	}

	FArcMassStatefulActionList* StatefulList = ActionData->Storage.StatefulActions.Find(EventName);
	if (StatefulList)
	{
		for (FInstancedStruct& ActionStruct : StatefulList->Actions)
		{
			FArcMassStatefulAction* Action = ActionStruct.GetMutablePtr<FArcMassStatefulAction>();
			if (Action)
			{
				Action->PostExecute(Context);
			}
		}
	}
}

FArcMassActionFragment UE::ArcMass::Actions::BuildInitialActionFragment(
	const UArcMassEntityConfigAsset& Config,
	const UWorld& World)
{
	FArcMassActionFragment Result;
	FArcSparseFragmentBuildContext SparseContext;
	TArray<UMassEntityTraitBase*> Traits;
	::ArcMass::CollectCombinedTraits(Config.GetConfig(), &World, Traits);
	for (UMassEntityTraitBase* Trait : Traits)
	{
		const UArcMassEntityTraitBase* ArcTrait = Cast<const UArcMassEntityTraitBase>(Trait);
		if (ArcTrait)
		{
			ArcTrait->BuildSparseFragments(SparseContext, World);
		}
	}

	for (const FInstancedStruct& SparseFragment : SparseContext.SparseFragments)
	{
		const FArcMassActionFragment* ActionFragment = SparseFragment.GetPtr<FArcMassActionFragment>();
		if (ActionFragment)
		{
			Result = *ActionFragment;
		}
	}

	for (const FInstancedStruct& SparseFragment : Config.InitialSparseFragments)
	{
		const FArcMassActionFragment* ActionFragment = SparseFragment.GetPtr<FArcMassActionFragment>();
		if (ActionFragment)
		{
			Result = *ActionFragment;
		}
	}

	return Result;
}
