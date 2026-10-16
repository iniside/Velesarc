// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Abilities/Schema/ArcAbilitySchemaNames.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityExecutionContext)

namespace ArcAbilityExecutionExtensionImpl
{
	void SignalEntity(UObject& Owner, const FMassEntityHandle Entity)
	{
		UWorld* World = Owner.GetWorld();
		if (!World || !Entity.IsValid()) { return; }
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
		if (!SignalSubsystem) { return; }
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
	}
}

void FArcAbilityExecutionExtension::ScheduleNextTick(const FContextParameters& Context, const FNextTickArguments& Args)
{
	ArcAbilityExecutionExtensionImpl::SignalEntity(Context.Owner, Entity);
}

FArcAbilityExecutionContext::FArcAbilityExecutionContext(
	UObject& InOwner,
	const UStateTree& InStateTree,
	FStateTreeInstanceData& InInstanceData,
	FMassExecutionContext& InMassContext)
	: FStateTreeExecutionContext(InOwner, InStateTree, InInstanceData)
	, MassContext(&InMassContext)
{
}

FMassEntityManager& FArcAbilityExecutionContext::GetEntityManager() const
{
	return MassContext->GetEntityManagerChecked();
}

void FArcAbilityExecutionContext::SetEntity(const FMassEntityHandle InEntity)
{
	Entity = InEntity;
}

void FArcAbilityExecutionContext::SetContextRequirements(
	FMassEntityHandle InEntity,
	FMassEntityManager& EntityManager,
	FArcEffectStackFragment* EffectStack,
	FArcAbilityContext& AbilityContextData,
	FInstancedStruct& SourceDataRef)
{
	FMassEntityView EntityView(EntityManager, InEntity);

	FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	if (OwnedTagsPtr)
	{
		SetContextDataByName(Arcx::MassAbilities::OwnedTags,
			FStateTreeDataView(FStructView::Make(*OwnedTagsPtr)));
	}
	if (EffectStack)
	{
		SetContextDataByName(Arcx::MassAbilities::EffectStack,
			FStateTreeDataView(FStructView::Make(*EffectStack)));
	}
	FArcAbilityInputFragment* AbilityInputPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
	if (AbilityInputPtr)
	{
		SetContextDataByName(Arcx::MassAbilities::AbilityInput,
			FStateTreeDataView(FStructView::Make(*AbilityInputPtr)));
	}
	SetContextDataByName(Arcx::MassAbilities::AbilityContext,
		FStateTreeDataView(FStructView::Make(AbilityContextData)));
	if (SourceDataRef.IsValid())
	{
		SetContextDataByName(Arcx::MassAbilities::SourceData,
			FStateTreeDataView(SourceDataRef));
	}
}

EStateTreeRunStatus FArcAbilityExecutionContext::Start()
{
	ensureMsgf(Entity.IsValid(), TEXT("The entity is not valid before starting the ability state tree instance."));

	FArcAbilityExecutionExtension Extension;
	Extension.Entity = Entity;
	Extension.AbilityHandle = AbilityHandle;

	return FStateTreeExecutionContext::Start(FStateTreeExecutionContext::FStartParameters
		{
			.ExecutionExtension = TInstancedStruct<FArcAbilityExecutionExtension>::Make(MoveTemp(Extension))
		});
}
