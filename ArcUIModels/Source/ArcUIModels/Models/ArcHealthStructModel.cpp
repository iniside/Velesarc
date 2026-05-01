// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcHealthStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "ViewModels/ArcFloatViewModel.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"
#include "MassAgentComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

void FArcHealthStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	AActor* Owner = Component.GetOwner();
	if (!Owner)
	{
		return;
	}

	APlayerState* PS = Cast<APlayerState>(Owner);
	if (!PS)
	{
		return;
	}

	APawn* Pawn = PS->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UMassAgentComponent* MassAgent = Pawn->FindComponentByClass<UMassAgentComponent>();
	if (!MassAgent)
	{
		return;
	}

	FMassEntityHandle PlayerEntity = MassAgent->GetEntityHandle();
	if (!PlayerEntity.IsSet())
	{
		return;
	}

	UArcFrontendMVVMModelComponent* CompPtr = &Component;
	AttributeListener.BindEntity(
		Component.GetWorld(),
		PlayerEntity,
		FArcAttributeChangedCallback::CreateLambda(
			[this, CompPtr](FMassEntityHandle Entity, const FArcAttributeChangedFragment& Fragment)
			{
				OnAttributeChanged(Entity, Fragment, *CompPtr);
			})
	);
}

void FArcHealthStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	AttributeListener.Unbind();
}

void FArcHealthStructModel::OnAttributeChanged(
	FMassEntityHandle Entity,
	const FArcAttributeChangedFragment& Fragment,
	UArcFrontendMVVMModelComponent& Component)
{
	const FArcAttributeRef HealthAttr = FArcMassHealthStatsFragment::GetHealthAttribute();
	const FArcAttributeRef MaxHealthAttr = FArcMassHealthStatsFragment::GetMaxHealthAttribute();

	UArcFloatViewModel* HealthVM = Cast<UArcFloatViewModel>(FindCachedViewModel(HealthViewModelName));
	if (!HealthVM)
	{
		return;
	}

	for (const FArcAttributeChangeEntry& Entry : Fragment.Changes)
	{
		if (Entry.Attribute == HealthAttr)
		{
			HealthVM->SetValue(Entry.NewValue);
		}
		else if (Entry.Attribute == MaxHealthAttr)
		{
			HealthVM->SetMaxValue(Entry.NewValue);
		}
	}
}
