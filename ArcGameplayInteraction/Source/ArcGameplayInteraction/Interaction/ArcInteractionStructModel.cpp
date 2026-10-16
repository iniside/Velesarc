// Copyright Lukasz Baran. All Rights Reserved.

#include "Interaction/ArcInteractionStructModel.h"

#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/ArcCoreInteractionSourceComponent.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ViewModels/ArcInteractionViewModel.h"

namespace ArcGameplayInteractionStructModel
{
	AActor* ResolveTargetActor(const TScriptInterface<IInteractionTarget>& Target)
	{
		UObject* Object = Target.GetObject();
		if (Object == nullptr)
		{
			return nullptr;
		}
		if (AActor* Actor = Cast<AActor>(Object))
		{
			return Actor;
		}
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
		{
			return ActorComponent->GetOwner();
		}
		return nullptr;
	}
}

void FArcInteractionStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcInteractionViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Component.CreateAndRegisterViewModels({ Spec });
}

void FArcInteractionStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	InteractionVM = Cast<UArcInteractionViewModel>(Component.FindViewModelByContext(ContextName, UArcInteractionViewModel::StaticClass()));
}

bool FArcInteractionStructModel::ResolveAnchor(UArcFrontendMVVMModelComponent& Component, FVector& OutAnchor) const
{
	APlayerState* PS = Cast<APlayerState>(Component.GetOwner());
	if (PS == nullptr)
	{
		return false;
	}

	const UArcCoreInteractionSourceComponent* Source = PS->FindComponentByClass<UArcCoreInteractionSourceComponent>();
	if (Source == nullptr)
	{
		return false;
	}

	const FMassEntityHandle EntityTarget = Source->GetCurrentEntityTarget();
	if (EntityTarget.IsSet())
	{
		UWorld* World = Component.GetWorld();
		if (World == nullptr)
		{
			return false;
		}
		const UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSS == nullptr)
		{
			return false;
		}
		const FMassEntityManager& EntityManager = MassSS->GetEntityManager();
		if (!EntityManager.IsEntityValid(EntityTarget))
		{
			return false;
		}
		const FArcMassPhysicsBodyFragment* BodyFragment = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(EntityTarget);
		if (BodyFragment == nullptr || BodyFragment->Body == nullptr)
		{
			return false;
		}
		const FBox Bounds = BodyFragment->Body->GetBodyBounds();
		if (!Bounds.IsValid)
		{
			return false;
		}
		const FVector Center = Bounds.GetCenter();
		OutAnchor = FVector(Center.X, Center.Y, Bounds.Max.Z) + AnchorOffset;
		return true;
	}

	AActor* TargetActor = ArcGameplayInteractionStructModel::ResolveTargetActor(Source->GetCurrentTarget());
	if (!IsValid(TargetActor))
	{
		return false;
	}
	const FBox Box = TargetActor->GetComponentsBoundingBox(true);
	if (!Box.IsValid)
	{
		return false;
	}
	const FVector BoxCenter = Box.GetCenter();
	OutAnchor = FVector(BoxCenter.X, BoxCenter.Y, Box.Max.Z) + AnchorOffset;
	return true;
}

void FArcInteractionStructModel::Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component)
{
	UArcInteractionViewModel* VM = InteractionVM.Get();
	if (VM == nullptr)
	{
		return;
	}

	FVector Anchor;
	if (!ResolveAnchor(Component, Anchor))
	{
		VM->SetHasTarget(false);
		return;
	}

	VM->SetAnchorX(static_cast<float>(Anchor.X));
	VM->SetAnchorY(static_cast<float>(Anchor.Y));
	VM->SetAnchorZ(static_cast<float>(Anchor.Z));
	VM->SetHasTarget(true);
}

void FArcInteractionStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	if (UArcInteractionViewModel* VM = InteractionVM.Get())
	{
		VM->SetHasTarget(false);
	}
	InteractionVM.Reset();
}
