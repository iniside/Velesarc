// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcTargetStructModel.h"

#include "ArcTargetingSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "MassAgentComponent.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "MassAgentSubsystem.h"
#include "MassEntitySubsystem.h"
#include "ViewModels/ArcTargetViewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTargetStructModel, Log, All);

namespace ArcUIModelsTargetStructModel
{
	FMassEntityHandle ResolveLocalPlayerEntity(UArcFrontendMVVMModelComponent& Component)
	{
		APlayerState* PS = Cast<APlayerState>(Component.GetOwner());
		if (PS == nullptr)
		{
			return FMassEntityHandle();
		}
		APawn* Pawn = PS->GetPawn();
		if (Pawn == nullptr)
		{
			return FMassEntityHandle();
		}
		UMassAgentComponent* Agent = Pawn->FindComponentByClass<UMassAgentComponent>();
		if (Agent == nullptr)
		{
			return FMassEntityHandle();
		}
		return Agent->GetEntityHandle();
	}

	bool HasTarget(const TArray<FArcTargetCandidate>* Result)
	{
		return Result != nullptr && Result->Num() > 0;
	}
}

void FArcTargetStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcTargetViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Component.CreateAndRegisterViewModels({ Spec });
}

void FArcTargetStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	if (!TargetingTag.IsValid())
	{
		UE_LOG(LogArcTargetStructModel, Error, TEXT("FArcTargetStructModel '%s' has no valid TargetingTag; it subscribes to nothing."), *ContextName.ToString());
		return;
	}

	TargetVM = Cast<UArcTargetViewModel>(Component.FindViewModelByContext(ContextName, UArcTargetViewModel::StaticClass()));

	UWorld* World = Component.GetWorld();
	if (World != nullptr)
	{
		UMassAgentSubsystem* AgentSS = World->GetSubsystem<UMassAgentSubsystem>();
		if (AgentSS != nullptr)
		{
			TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
			EntityAssociatedHandle = AgentSS->GetOnMassAgentComponentEntityAssociated().AddLambda(
				[this, WeakComponent](const UMassAgentComponent& AgentComponent)
				{
					if (BoundEntity.IsSet())
					{
						return;
					}

					UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get();
					if (Comp == nullptr)
					{
						return;
					}
					APlayerState* PS = Cast<APlayerState>(Comp->GetOwner());
					if (PS == nullptr)
					{
						return;
					}
					APawn* Pawn = PS->GetPawn();
					if (Pawn == nullptr || AgentComponent.GetOwner() != Pawn)
					{
						return;
					}

					TryBindToEntity(*Comp);
				});
		}
	}

	TryBindToEntity(Component);
}

bool FArcTargetStructModel::TryBindToEntity(UArcFrontendMVVMModelComponent& Component)
{
	if (BoundEntity.IsSet())
	{
		return true;
	}

	const FMassEntityHandle Entity = ArcUIModelsTargetStructModel::ResolveLocalPlayerEntity(Component);
	if (!Entity.IsSet())
	{
		return false;
	}

	UWorld* World = Component.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	UArcTargetingSubsystem* Targeting = World->GetSubsystem<UArcTargetingSubsystem>();
	if (Targeting == nullptr)
	{
		return false;
	}

	BoundEntity = Entity;
	BoundTargeting = Targeting;

	ResultHandle = Targeting->SubscribeResult(
		Entity,
		TargetingTag,
		FArcTargetingResultChanged::FDelegate::CreateLambda(
			[this](const TArray<FArcTargetCandidate>* Result)
			{
				ApplyResult(Result);
			}));

	ApplyResult(Targeting->GetLatestPersistentResult(Entity, TargetingTag));
	return true;
}

void FArcTargetStructModel::ApplyResult(const TArray<FArcTargetCandidate>* Result)
{
	UArcTargetViewModel* VM = TargetVM.Get();
	UArcTargetingSubsystem* Targeting = BoundTargeting.Get();
	if (VM == nullptr || Targeting == nullptr)
	{
		return;
	}

	if (!ArcUIModelsTargetStructModel::HasTarget(Result))
	{
		TargetEntity = FMassEntityHandle();
		VM->SetHasTarget(false);
		return;
	}

	TargetEntity = (*Result)[0].Entity;
	UWorld* World = Targeting->GetWorld();
	if (World != nullptr)
	{
		WriteMarker(*World);
	}
	VM->SetHasTarget(true);
}

bool FArcTargetStructModel::WriteMarker(UWorld& World)
{
	UArcTargetViewModel* VM = TargetVM.Get();
	UMassEntitySubsystem* MassSS = World.GetSubsystem<UMassEntitySubsystem>();
	if (VM == nullptr || MassSS == nullptr || !TargetEntity.IsSet())
	{
		return false;
	}

	const FMassEntityManager& Manager = MassSS->GetEntityManager();
	const FTransformFragment* Transform = Manager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
	if (Transform == nullptr)
	{
		return false;
	}

	const FVector Location = Transform->GetTransform().GetLocation() + MarkerOffset;
	VM->SetMarkerX(static_cast<float>(Location.X));
	VM->SetMarkerY(static_cast<float>(Location.Y));
	VM->SetMarkerZ(static_cast<float>(Location.Z));
	return true;
}

void FArcTargetStructModel::Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component)
{
	if (!TargetEntity.IsSet())
	{
		return;
	}

	UWorld* World = Component.GetWorld();
	if (World == nullptr)
	{
		return;
	}
	WriteMarker(*World);
}

void FArcTargetStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	UWorld* World = Component.GetWorld();
	if (World != nullptr && EntityAssociatedHandle.IsValid())
	{
		UMassAgentSubsystem* AgentSS = World->GetSubsystem<UMassAgentSubsystem>();
		if (AgentSS != nullptr)
		{
			AgentSS->GetOnMassAgentComponentEntityAssociated().Remove(EntityAssociatedHandle);
		}
	}
	EntityAssociatedHandle.Reset();

	if (UArcTargetingSubsystem* Targeting = BoundTargeting.Get())
	{
		Targeting->UnsubscribeResult(BoundEntity, TargetingTag, ResultHandle);
	}
	ResultHandle.Reset();
	BoundTargeting.Reset();
	BoundEntity = FMassEntityHandle();
	TargetEntity = FMassEntityHandle();

	if (UArcTargetViewModel* VM = TargetVM.Get())
	{
		VM->SetHasTarget(false);
	}
	TargetVM.Reset();
}
