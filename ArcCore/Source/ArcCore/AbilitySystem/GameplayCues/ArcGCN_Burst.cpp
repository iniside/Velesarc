/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#include "ArcGCN_Burst.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Misc/DataValidation.h"
#include "Particles/ParticleSystemComponent.h"

//////////////////////////////////////////////////////////////////////////
// UGameplayCueNotify_Burst
//////////////////////////////////////////////////////////////////////////
UArcGCN_Burst::UArcGCN_Burst()
{
}

bool FArcGameplayCueNotify_PlacementInfo::FindSpawnTransform(const FArcGameplayCueNotify_SpawnContext& SpawnContext
	, FTransform& OutTransform) const
{
	const FGameplayCueParameters& CueParameters = SpawnContext.CueParameters;

	bool bSetTransform = false;

	if (bSpawnOnFloor && SpawnContext.TargetActor)
	{
		FHitResult OutHit;

		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(SpawnContext.TargetActor);
		SpawnContext.World->LineTraceSingleByChannel(OutHit, SpawnContext.TargetActor->GetActorLocation()
			, SpawnContext.TargetActor->GetActorLocation() - FVector(0.0f, 0.0f, 1000.0f), ECollisionChannel::ECC_Visibility, TraceParams);
		{
			OutTransform = FTransform(FQuat::Identity, OutHit.ImpactPoint, FVector(1.0));
			bSetTransform = true;

			return bSetTransform;
		}
	}
	// First attempt to get the transform from the hit result.
	// If that fails, use the gameplay cue parameters.
	// If that fails, fall back to the target actor.
	if (SpawnContext.HitResult && SpawnContext.HitResult->bBlockingHit)
	{
		OutTransform = FTransform(SpawnContext.HitResult->ImpactNormal.Rotation(), SpawnContext.HitResult->ImpactPoint);
		bSetTransform = true;
	}
	else if (!CueParameters.Location.IsZero())
	{
		OutTransform = FTransform(CueParameters.Normal.Rotation(), CueParameters.Location);
		bSetTransform = true;
	}
	else if (SpawnContext.TargetComponent)
	{
		OutTransform = SpawnContext.TargetComponent->GetSocketTransform(SocketName);
		bSetTransform = true;
	}

	if (bSetTransform)
	{
		if (bOverrideRotation)
		{
			OutTransform.SetRotation(RotationOverride.Quaternion());
		}

		if (bOverrideScale)
		{
			OutTransform.SetScale3D(ScaleOverride);
		}
	}
	else
	{
		// This can happen if the target actor if destroyed out from under us.  We could alternatively pass the actor down, from ::GameplayCueFinishedCallback.
		// Right now it seems to make more sense to skip spawning more stuff in this case.  As the OnRemove effects will spawn if the GC OnRemove is actually broadcasted "naturally".
		// The actor dieing and forcing the OnRemove event is more of a "please cleanup everything" event. This could be changed in the future.
		//UE_LOG(LogGameplayCueNotify, Log, TEXT("GameplayCueNotify: Failed to find spawn transform for gameplay cue notify [%s]."), *CueParameters.MatchedTagName.ToString());
	}

	return bSetTransform;
}

void FArcGameplayCueNotify_PlacementInfo::SetComponentTransform(USceneComponent* Component
	, const FTransform& Transform) const
{
	check(Component);

	Component->SetAbsolute(Component->IsUsingAbsoluteLocation(), bOverrideRotation, bOverrideScale);
	Component->SetWorldTransform(Transform);
}

void FArcGameplayCueNotify_PlacementInfo::TryComponentAttachment(USceneComponent* Component
	, USceneComponent* AttachComponent) const
{
	check(Component);

	if (AttachComponent && (AttachPolicy == EGameplayCueNotify_AttachPolicy::AttachToTarget))
	{
		FAttachmentTransformRules AttachmentRules(AttachmentRule, true);

		Component->AttachToComponent(AttachComponent, AttachmentRules, SocketName);
	}
}

APlayerController* FArcGameplayCueNotify_SpawnContext::FindLocalPlayerController(EGameplayCueNotify_LocallyControlledSource Source) const
{
	AActor* ActorToSearch = nullptr;

	switch (Source)
	{
		case EGameplayCueNotify_LocallyControlledSource::InstigatorActor:
			ActorToSearch = CueParameters.Instigator.Get();
		break;

		case EGameplayCueNotify_LocallyControlledSource::TargetActor:
			ActorToSearch = TargetActor;
		break;

		default:
			break;
	}

	auto FindPlayerControllerFromActor = [](AActor* Actor) -> APlayerController*
	{
		if (!Actor)
		{
			return nullptr;
		}

		// Check the actual actor first.
		APlayerController* PC = Cast<APlayerController>(Actor);
		if (PC)
		{
			return PC;
		}

		// Check using pawn next.
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			Pawn = Actor->GetInstigator<APawn>();
		}

		PC = (Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr);
		if (PC)
		{
			return PC;
		}

		// Check using player state.
		APlayerState* PS = Cast<APlayerState>(Actor);

		PC = (PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr);
		if (PC)
		{
			return PC;
		}

		return nullptr;
	};
	
	APlayerController* PC = FindPlayerControllerFromActor(ActorToSearch);

	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

bool FArcGameplayCueNotify_SpawnCondition::ShouldSpawn(const FArcGameplayCueNotify_SpawnContext& SpawnContext) const
{
	const APlayerController* PC = SpawnContext.FindLocalPlayerController(LocallyControlledSource);

	const bool bIsLocallyControlledSource = (PC != nullptr);

	if ((LocallyControlledPolicy == EGameplayCueNotify_LocallyControlledPolicy::LocalOnly) && !bIsLocallyControlledSource)
	{
		return false;
	}
	else if ((LocallyControlledPolicy == EGameplayCueNotify_LocallyControlledPolicy::NotLocal) && bIsLocallyControlledSource)
	{
		return false;
	}

	if ((ChanceToPlay < 1.0f) && (ChanceToPlay < FMath::FRand()))
	{
		return false;
	}

	const FGameplayCueNotify_SurfaceMask SpawnContextSurfaceMask = (1ULL << SpawnContext.SurfaceType);

	if (AllowedSurfaceTypes.Num() > 0)
	{
		// Build the surface type mask if needed.
		if (AllowedSurfaceMask == 0x0)
		{
			for (EPhysicalSurface SurfaceType : AllowedSurfaceTypes)
			{
				AllowedSurfaceMask |= (1ULL << SurfaceType);
			}
		}

		if ((AllowedSurfaceMask & SpawnContextSurfaceMask) == 0x0)
		{
			return false;
		}
	}

	if (RejectedSurfaceTypes.Num() > 0)
	{
		// Build the surface type mask if needed.
		if (RejectedSurfaceMask == 0x0)
		{
			for (EPhysicalSurface SurfaceType : RejectedSurfaceTypes)
			{
				RejectedSurfaceMask |= (1ULL << SurfaceType);
			}
		}

		if ((RejectedSurfaceMask & SpawnContextSurfaceMask) != 0x0)
		{
			return false;
		}
	}

	return true;
}

bool FArcGameplayCueNotify_ParticleInfo::PlayParticleEffect(const FArcGameplayCueNotify_SpawnContext& SpawnContext, FGameplayCueNotify_SpawnResult& OutSpawnResult) const
{
	UNiagaraComponent* SpawnedFXSC = nullptr;

	if (NiagaraSystem != nullptr)
	{
		const FArcGameplayCueNotify_SpawnCondition& SpawnCondition = SpawnContext.GetSpawnCondition(bOverrideSpawnCondition, SpawnConditionOverride);
		const FArcGameplayCueNotify_PlacementInfo& PlacementInfo = SpawnContext.GetPlacementInfo(bOverridePlacementInfo, PlacementInfoOverride);

		if (SpawnCondition.ShouldSpawn(SpawnContext))
		{
			FTransform SpawnTransform;

			if (PlacementInfo.FindSpawnTransform(SpawnContext, SpawnTransform))
			{
				check(SpawnContext.World);

				const FVector SpawnLocation = SpawnTransform.GetLocation();
				const FRotator SpawnRotation = SpawnTransform.Rotator();
				const FVector SpawnScale = SpawnTransform.GetScale3D();
				const bool bAutoDestroy = false;
				const bool bAutoActivate = true;

				if (SpawnContext.TargetComponent && (PlacementInfo.AttachPolicy == EGameplayCueNotify_AttachPolicy::AttachToTarget))
				{
					const EAttachLocation::Type AttachLocationType = EAttachLocation::Type::SnapToTarget;// GetAttachLocationTypeFromRule(PlacementInfo.AttachmentRule);

					SpawnedFXSC = UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, SpawnContext.TargetComponent, PlacementInfo.SocketName,
						SpawnLocation, SpawnRotation, SpawnScale, AttachLocationType, bAutoDestroy, ENCPoolMethod::AutoRelease, bAutoActivate);
				}
				else
				{
					SpawnedFXSC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(SpawnContext.World, NiagaraSystem,
						SpawnLocation, SpawnRotation, SpawnScale, bAutoDestroy, bAutoActivate, ENCPoolMethod::AutoRelease);
				}

				if (SpawnedFXSC)
				{
					SpawnedFXSC->SetCastShadow(bCastShadow);
				}
			}
		}
	}

	// Always add to the list, even if null, so that the list is stable and in order for blueprint users.
	OutSpawnResult.FxSystemComponents.Add(SpawnedFXSC);

	return (SpawnedFXSC != nullptr);
}

void FArcGameplayCueNotify_ParticleInfo::ValidateBurstAssets(const UObject* ContainingAsset, const FString& Context, FDataValidationContext& ValidationContext) const
{
#if WITH_EDITORONLY_DATA
	if (NiagaraSystem != nullptr)
	{
		if (NiagaraSystem->IsLooping())
		{
			//ValidationContext.AddError(FText::Format(
			//	LOCTEXT("NiagaraSystem_ShouldNotLoop", "Niagara system [{0}] used in slot [{1}] for asset [{2}] is set to looping, but the slot is a one-shot (the instance will leak)."),
			//	FText::AsCultureInvariant(NiagaraSystem->GetPathName()),
			//	FText::AsCultureInvariant(Context),
			//	FText::AsCultureInvariant(ContainingAsset->GetPathName())));
		}
	}
#endif // #if WITH_EDITORONLY_DATA
}

void FArcGameplayCueNotify_BurstEffects::ExecuteEffects(const FArcGameplayCueNotify_SpawnContext& SpawnContext
	, FGameplayCueNotify_SpawnResult& OutSpawnResult) const
{
	if (!SpawnContext.World)
	{
		//UE_LOG(LogGameplayCueNotify, Error, TEXT("GameplayCueNotify: Trying to execute Burst effects with a NULL world."));
		return;
	}

	for (const FArcGameplayCueNotify_ParticleInfo& ParticleInfo : BurstParticles)
	{
		ParticleInfo.PlayParticleEffect(SpawnContext, OutSpawnResult);
	}

	//for (const FGameplayCueNotify_SoundInfo& SoundInfo : BurstSounds)
	//{
	//	SoundInfo.PlaySound(SpawnContext, OutSpawnResult);
	//}
	//
	//BurstCameraShake.PlayCameraShake(SpawnContext, OutSpawnResult);
	//BurstCameraLensEffect.PlayCameraLensEffect(SpawnContext, OutSpawnResult);
	//BurstForceFeedback.PlayForceFeedback(SpawnContext, OutSpawnResult);
	//BurstDevicePropertyEffect.SetDeviceProperties(SpawnContext, OutSpawnResult);
	//BurstDecal.SpawnDecal(SpawnContext, OutSpawnResult);
}

bool UArcGCN_Burst::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = (Target ? Target->GetWorld() : GetWorld());

	FArcGameplayCueNotify_SpawnContext SpawnContext(World, Target, Parameters);
	SpawnContext.SetDefaultSpawnCondition(&DefaultSpawnCondition);
	SpawnContext.SetDefaultPlacementInfo(&DefaultPlacementInfo);

	if (DefaultSpawnCondition.ShouldSpawn(SpawnContext))
	{
		FGameplayCueNotify_SpawnResult SpawnResult;

		BurstEffects.ExecuteEffects(SpawnContext, SpawnResult);

		OnBurst(Target, Parameters, SpawnResult);

		return true;
	}

	return false;
}

#if WITH_EDITOR
EDataValidationResult UArcGCN_Burst::IsDataValid(FDataValidationContext& Context) const
{
	//BurstEffects.ValidateAssociatedAssets(this, TEXT("BurstEffects"), Context);

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // #if WITH_EDITOR
