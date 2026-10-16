// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcFloatAttributeStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "ViewModels/ArcFloatViewModel.h"
#include "Events/ArcAttributeEventPayloads.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "MassEntitySubsystem.h"
#include "MassAgentComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

void FArcFloatAttributeStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcFloatViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Component.CreateAndRegisterViewModels({ Spec });
}

void FArcFloatAttributeStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
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

	// Resolve the VM handle once; all reads below (Label seed, ApplyToViewModel) use FloatVM directly.
	FloatVM = Cast<UArcFloatViewModel>(Component.FindViewModelByContext(ContextName, UArcFloatViewModel::StaticClass()));

	// Seed the static label once up front; live value/min/max arrive via per-attribute bindings below.
	if (!Label.IsEmpty())
	{
		if (UArcFloatViewModel* ValueVM = FloatVM.Get())
		{
			ValueVM->SetLabel(Label);
		}
	}

	UMassEntitySubsystem* MES = Component.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UArcGameEventSubsystem* Events = Component.GetWorld()->GetSubsystem<UArcGameEventSubsystem>();
	if (!MES || !Events)
	{
		return;
	}

	BoundEventSubsystem = Events;

	// Seed the current attribute values synchronously. GetValue takes a non-const manager.
	FMassEntityManager& EM = MES->GetMutableEntityManager();
	if (ValueAttribute.IsValid())
	{
		ApplyToViewModel(Component, EArcFloatField::Value, ArcAttributes::GetValue(EM, PlayerEntity, ValueAttribute));
	}
	if (MinAttribute.IsValid())
	{
		ApplyToViewModel(Component, EArcFloatField::Min, ArcAttributes::GetValue(EM, PlayerEntity, MinAttribute));
	}
	if (MaxAttribute.IsValid())
	{
		ApplyToViewModel(Component, EArcFloatField::Max, ArcAttributes::GetValue(EM, PlayerEntity, MaxAttribute));
	}
	if (RegenPipsAttribute.IsValid())
	{
		ApplyToViewModel(Component, EArcFloatField::RegenPips, ArcAttributes::GetValue(EM, PlayerEntity, RegenPipsAttribute));
	}
	if (DegenPipsAttribute.IsValid())
	{
		ApplyToViewModel(Component, EArcFloatField::DegenPips, ArcAttributes::GetValue(EM, PlayerEntity, DegenPipsAttribute));
	}

	// Bind one keyed subscription per valid attribute ref. Capturing `this` is safe only because
	// Deinitialize unbinds before this struct dies (the FArcEquipmentStructModel contract).
	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComp = &Component;
	TFunction<FArcGameEventDelegate(EArcFloatField)> MakeDelegate = [this, WeakComp](EArcFloatField Field) -> FArcGameEventDelegate
	{
		return FArcGameEventDelegate::CreateLambda(
			[this, WeakComp, Field](const FArcGameEvent& Event)
			{
				const FArcAttributeChangedPayload* Payload = Event.Payload.GetPtr<FArcAttributeChangedPayload>();
				UArcFrontendMVVMModelComponent* C = WeakComp.Get();
				if (Payload && C)
				{
					ApplyToViewModel(*C, Field, Payload->NewValue);
				}
			});
	};
	if (ValueAttribute.IsValid())
	{
		ValueHandle = Events->Subscribe(PlayerEntity, ValueAttribute, MakeDelegate(EArcFloatField::Value));
	}
	if (MinAttribute.IsValid())
	{
		MinHandle = Events->Subscribe(PlayerEntity, MinAttribute, MakeDelegate(EArcFloatField::Min));
	}
	if (MaxAttribute.IsValid())
	{
		MaxHandle = Events->Subscribe(PlayerEntity, MaxAttribute, MakeDelegate(EArcFloatField::Max));
	}
	if (RegenPipsAttribute.IsValid())
	{
		RegenPipsHandle = Events->Subscribe(PlayerEntity, RegenPipsAttribute, MakeDelegate(EArcFloatField::RegenPips));
	}
	if (DegenPipsAttribute.IsValid())
	{
		DegenPipsHandle = Events->Subscribe(PlayerEntity, DegenPipsAttribute, MakeDelegate(EArcFloatField::DegenPips));
	}

}

void FArcFloatAttributeStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	if (UArcGameEventSubsystem* Events = BoundEventSubsystem.Get())
	{
		Events->Unsubscribe(ValueHandle);
		Events->Unsubscribe(MinHandle);
		Events->Unsubscribe(MaxHandle);
		Events->Unsubscribe(RegenPipsHandle);
		Events->Unsubscribe(DegenPipsHandle);
	}
	ValueHandle.Reset();
	MinHandle.Reset();
	MaxHandle.Reset();
	RegenPipsHandle.Reset();
	DegenPipsHandle.Reset();
	RegenPips = 0.f;
	DegenPips = 0.f;
	FloatVM.Reset();
	BoundEventSubsystem.Reset();
}

void FArcFloatAttributeStructModel::ApplyToViewModel(
	UArcFrontendMVVMModelComponent& Component, EArcFloatField Field, float NewValue)
{
	UArcFloatViewModel* ValueVM = FloatVM.Get();
	if (!ValueVM)
	{
		return;
	}
	switch (Field)
	{
		case EArcFloatField::Value: ValueVM->SetValue(NewValue);    break;
		case EArcFloatField::Min:   ValueVM->SetMinValue(NewValue); break;
		case EArcFloatField::Max:   ValueVM->SetMaxValue(NewValue); break;
		case EArcFloatField::RegenPips:
			RegenPips = NewValue;
			ValueVM->SetPips(ArcResourcePips::NetPips(RegenPips, DegenPips));
			break;
		case EArcFloatField::DegenPips:
			DegenPips = NewValue;
			ValueVM->SetPips(ArcResourcePips::NetPips(RegenPips, DegenPips));
			break;
	}
}
