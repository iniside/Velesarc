// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectStateTreeContext.h"

#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassEntityTypes.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectStateTreeContext)

namespace Arcx::MassAbilities::EffectStateTree
{

static const FName OwnerEntityHandleName = TEXT("OwnerEntityHandle");
static const FName SourceEntityHandleName = TEXT("SourceEntityHandle");
static const FName EffectTreeContextName = TEXT("EffectTreeContext");

} // namespace Arcx::MassAbilities::EffectStateTree

bool FArcEffectStateTreeContext::Activate(const UStateTree* StateTree, UObject* InOwner)
{
	if (StateTree == nullptr || InOwner == nullptr)
	{
		return false;
	}

	CachedStateTree = StateTree;
	Owner = InOwner;

	FStateTreeExecutionContext Context(*InOwner, *StateTree, InstanceData);
	if (!Context.IsValid())
	{
		return false;
	}

	if (!SetContextRequirements(Context))
	{
		return false;
	}

	LastRunStatus = Context.Start();
	return LastRunStatus != EStateTreeRunStatus::Failed;
}

bool FArcEffectStateTreeContext::Tick(float DeltaTime)
{
	if (CachedStateTree == nullptr || Owner == nullptr)
	{
		return false;
	}

	FStateTreeExecutionContext Context(*Owner, *CachedStateTree, InstanceData);
	if (!Context.IsValid())
	{
		return false;
	}

	if (SetContextRequirements(Context))
	{
		LastRunStatus = Context.Tick(DeltaTime);
	}

	return LastRunStatus == EStateTreeRunStatus::Running;
}

void FArcEffectStateTreeContext::Deactivate()
{
	if (CachedStateTree == nullptr || Owner == nullptr)
	{
		LastRunStatus = EStateTreeRunStatus::Unset;
		CachedStateTree = nullptr;
		return;
	}

	FStateTreeExecutionContext Context(*Owner, *CachedStateTree, InstanceData);
	if (Context.IsValid())
	{
		if (SetContextRequirements(Context))
		{
			Context.Stop();
		}
	}

	LastRunStatus = EStateTreeRunStatus::Unset;
	CachedStateTree = nullptr;
}

void FArcEffectStateTreeContext::RequestTick(const FGameplayTag& Tag, FConstStructView Payload)
{
	if (CachedStateTree == nullptr || Owner == nullptr)
	{
		return;
	}

	FStateTreeMinimalExecutionContext MinimalContext(*Owner, *CachedStateTree, InstanceData);
	MinimalContext.SendEvent(Tag, Payload);

	Tick(0.f);
}

bool FArcEffectStateTreeContext::SetContextRequirements(FStateTreeExecutionContext& Context)
{
	if (!Context.IsValid())
	{
		return false;
	}

	Context.SetContextDataByName(Arcx::MassAbilities::EffectStateTree::OwnerEntityHandleName,
		FStateTreeDataView(FStructView::Make(OwnerEntity)));
	Context.SetContextDataByName(Arcx::MassAbilities::EffectStateTree::SourceEntityHandleName,
		FStateTreeDataView(FStructView::Make(SourceEntity)));
	Context.SetContextDataByName(Arcx::MassAbilities::EffectStateTree::EffectTreeContextName,
		FStateTreeDataView(FStructView::Make(*this)));

	UWorld* ResolvedWorld = World.Get();

	UMassEntitySubsystem* MES = ResolvedWorld ? ResolvedWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManagerPtr = MES ? &MES->GetMutableEntityManager() : nullptr;
	const FMassEntityHandle CapturedOwnerEntity = OwnerEntity;

	Context.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
		[ResolvedWorld, EntityManagerPtr, CapturedOwnerEntity]
		(const FStateTreeExecutionContext& Ctx, const UStateTree* ST,
		 TArrayView<const FStateTreeExternalDataDesc> ExternalDescs,
		 TArrayView<FStateTreeDataView> OutDataViews)
		{
			const bool bHasEntityView = EntityManagerPtr != nullptr && CapturedOwnerEntity.IsValid();

			for (int32 Index = 0; Index < ExternalDescs.Num(); Index++)
			{
				const FStateTreeExternalDataDesc& Desc = ExternalDescs[Index];
				if (Desc.Struct == nullptr)
				{
					continue;
				}

				if (bHasEntityView && UE::Mass::IsA<FMassFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedOwnerEntity);
					FStructView Fragment = EntityView.GetFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment);
					}
				}
				else if (bHasEntityView && UE::Mass::IsA<FMassSharedFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedOwnerEntity);
					FStructView Fragment = EntityView.GetSharedFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment.GetScriptStruct(), Fragment.GetMemory());
					}
				}
				else if (bHasEntityView && UE::Mass::IsA<FMassConstSharedFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedOwnerEntity);
					FConstStructView Fragment = EntityView.GetConstSharedFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment.GetScriptStruct(), const_cast<uint8*>(Fragment.GetMemory()));
					}
				}
				else if (ResolvedWorld != nullptr && Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
				{
					UWorldSubsystem* Subsystem = ResolvedWorld->GetSubsystemBase(
						Cast<UClass>(const_cast<UStruct*>(ToRawPtr(Desc.Struct))));
					if (Subsystem)
					{
						OutDataViews[Index] = FStateTreeDataView(Subsystem);
					}
				}
			}

			return true;
		})
	);

	return Context.AreContextDataViewsValid();
}
