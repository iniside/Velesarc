// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Abilities/Schema/ArcAbilitySchemaNames.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityExecutionContext)

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
	FArcOwnedTagsFragment& OwnedTags,
	FArcEffectStackFragment* EffectStack,
	FArcAbilityInputFragment& AbilityInput,
	FArcAbilityContext& AbilityContextData,
	FInstancedStruct& SourceDataRef)
{
	SetContextDataByName(Arcx::MassAbilities::OwnedTags,
		FStateTreeDataView(FStructView::Make(OwnedTags)));
	if (EffectStack)
	{
		SetContextDataByName(Arcx::MassAbilities::EffectStack,
			FStateTreeDataView(FStructView::Make(*EffectStack)));
	}
	SetContextDataByName(Arcx::MassAbilities::AbilityInput,
		FStateTreeDataView(FStructView::Make(AbilityInput)));
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
