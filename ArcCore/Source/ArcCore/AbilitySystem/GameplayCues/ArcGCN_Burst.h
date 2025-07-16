/**
 * This file is part of ArcX.
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



#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotifyTypes.h"
#include "GameplayCueNotify_Static.h"
#include "ArcGCN_Burst.generated.h"

struct FArcGameplayCueNotify_SpawnCondition;
struct FArcGameplayCueNotify_SpawnContext;
class USceneComponent;

USTRUCT(BlueprintType)
struct FArcGameplayCueNotify_PlacementInfo
{
	GENERATED_BODY()

public:

	FArcGameplayCueNotify_PlacementInfo() = default;

	bool FindSpawnTransform(const FArcGameplayCueNotify_SpawnContext& SpawnContext, FTransform& OutTransform) const;

	void SetComponentTransform(USceneComponent* Component, const FTransform& Transform) const;
	void TryComponentAttachment(USceneComponent* Component, USceneComponent* AttachComponent) const;

public:

	// Target's socket (or bone) used for location and rotation.  If "None", it uses the target's root.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	FName SocketName;

	// Whether to attach to the target actor or not attach.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	EGameplayCueNotify_AttachPolicy AttachPolicy;

	// How the transform is handled when attached.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	EAttachmentRule AttachmentRule;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	uint32 bSpawnOnFloor : 1;
	
	// If enabled, will always spawn using rotation override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotation : 1;

	// If enabled, will always spawn using the scale override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (InlineEditConditionToggle))
	uint32 bOverrideScale : 1;

	// If enabled, will always spawn using rotation override.
	// This will also set the rotation to be absolute, so any changes to the parent's rotation will be ignored after attachment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (EditCondition = "bOverrideRotation"))
	FRotator RotationOverride;

	// If enabled, will always spawn using scale override.
	// This will also set the scale to be absolute, so any changes to the parent's scale will be ignored after attachment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (EditCondition = "bOverrideScale"))
	FVector ScaleOverride;
};

/**
 * FGameplayCueNotify_SpawnContext
 *
 *	Temporary spawn information collected from input parameters.
 */
struct FArcGameplayCueNotify_SpawnContext
{
public:

	FArcGameplayCueNotify_SpawnContext(UWorld* InWorld, AActor* InTargetActor, const FGameplayCueParameters& InCueParameters)
		: World(InWorld)
		, TargetActor(InTargetActor)
		, CueParameters(InCueParameters)
	{}
	
	void SetDefaultSpawnCondition(const FArcGameplayCueNotify_SpawnCondition* InSpawnCondition) {DefaultSpawnCondition = InSpawnCondition;}
	void SetDefaultPlacementInfo(const FArcGameplayCueNotify_PlacementInfo* InPlacementInfo) {DefaultPlacementInfo = InPlacementInfo;}

	const FArcGameplayCueNotify_PlacementInfo& GetPlacementInfo(bool bUseOverride, const FArcGameplayCueNotify_PlacementInfo& PlacementInfoOverride) const
	{
		return (!bUseOverride && DefaultPlacementInfo) ? *DefaultPlacementInfo : PlacementInfoOverride;
	}

	const FArcGameplayCueNotify_SpawnCondition& GetSpawnCondition(bool bUseOverride, const FArcGameplayCueNotify_SpawnCondition& SpawnConditionOverride) const
	{
		return (!bUseOverride && DefaultSpawnCondition) ? *DefaultSpawnCondition : SpawnConditionOverride;
	}

	APlayerController* FindLocalPlayerController(EGameplayCueNotify_LocallyControlledSource Source) const;

protected:

	void InitializeContext();

public:

	UWorld* World;
	AActor* TargetActor;
	const FGameplayCueParameters& CueParameters;
	const FHitResult* HitResult;
	USceneComponent* TargetComponent;
	EPhysicalSurface SurfaceType;

private:

	const FArcGameplayCueNotify_SpawnCondition* DefaultSpawnCondition;
	const FArcGameplayCueNotify_PlacementInfo* DefaultPlacementInfo;
};

USTRUCT(BlueprintType)
struct FArcGameplayCueNotify_SpawnCondition
{
	GENERATED_BODY()

public:

	FArcGameplayCueNotify_SpawnCondition() = default;

	bool ShouldSpawn(const FArcGameplayCueNotify_SpawnContext& SpawnContext) const;

public:

	// Source actor to use when determining if it is locally controlled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	EGameplayCueNotify_LocallyControlledSource LocallyControlledSource;

	// Locally controlled policy used to determine if the gameplay cue effects should spawn.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	EGameplayCueNotify_LocallyControlledPolicy LocallyControlledPolicy;

	// Random chance to play the effects.  (1.0 = always play,  0.0 = never play)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (UIMin = "0.0", ClampMin = "0.0", UIMax = "1.0", ClampMax = "1.0"))
	float ChanceToPlay = 1.0f;

	// The gameplay cue effects will only spawn if the surface type is in this list.
	// An empty list means everything is allowed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	TArray<TEnumAsByte<EPhysicalSurface>> AllowedSurfaceTypes;
	mutable FGameplayCueNotify_SurfaceMask AllowedSurfaceMask;

	// The gameplay cue effects will only spawn if the surface type is NOT in this list.
	// An empty list means nothing will be rejected.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	TArray<TEnumAsByte<EPhysicalSurface>> RejectedSurfaceTypes;
	mutable FGameplayCueNotify_SurfaceMask RejectedSurfaceMask;
};

/**
 * FGameplayCueNotify_ParticleInfo
 *
 *	Properties that specify how to play a particle effect.
 */
USTRUCT(BlueprintType)
struct FArcGameplayCueNotify_ParticleInfo
{
	GENERATED_BODY()

public:

	FArcGameplayCueNotify_ParticleInfo() = default;

	bool PlayParticleEffect(const FArcGameplayCueNotify_SpawnContext& SpawnContext, FGameplayCueNotify_SpawnResult& OutSpawnResult) const;

	void ValidateBurstAssets(const UObject* ContainingAsset, const FString& Context, class FDataValidationContext& ValidationContext) const;

public:

	// Condition to check before spawning the particle system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (EditCondition = "bOverrideSpawnCondition"))
	FArcGameplayCueNotify_SpawnCondition SpawnConditionOverride;

	// Defines how the particle system will be placed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (EditCondition = "bOverridePlacementInfo"))
	FArcGameplayCueNotify_PlacementInfo PlacementInfoOverride;

	// Niagara FX system to spawn.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	// If enabled, use the spawn condition override and not the default one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (InlineEditConditionToggle))
	uint32 bOverrideSpawnCondition : 1;

	// If enabled, use the placement info override and not the default one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify, Meta = (InlineEditConditionToggle))
	uint32 bOverridePlacementInfo : 1;

	// If enabled, this particle system will cast a shadow.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	uint32 bCastShadow : 1;
};

USTRUCT(BlueprintType)
struct FArcGameplayCueNotify_BurstEffects
{
	GENERATED_BODY()

public:

	FArcGameplayCueNotify_BurstEffects() = default;

	void ExecuteEffects(const FArcGameplayCueNotify_SpawnContext& SpawnContext, FGameplayCueNotify_SpawnResult& OutSpawnResult) const;

	//void ValidateAssociatedAssets(const UObject* ContainingAsset, const FString& Context, class FDataValidationContext& ValidationContext) const;

protected:

	// Particle systems to be spawned on gameplay cue execution.  These should never use looping effects!
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	TArray<FArcGameplayCueNotify_ParticleInfo> BurstParticles;

	//// Sound to be played on gameplay cue execution.  These should never use looping effects!
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	//TArray<FGameplayCueNotify_SoundInfo> BurstSounds;
	//
	//// Camera shake to be played on gameplay cue execution.  This should never use a looping effect!
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	//FGameplayCueNotify_CameraShakeInfo BurstCameraShake;
	//
	//// Camera lens effect to be played on gameplay cue execution.  This should never use a looping effect!
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	//FGameplayCueNotify_CameraLensEffectInfo BurstCameraLensEffect;
	//
	//// Force feedback to be played on gameplay cue execution.  This should never use a looping effect!
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	//FGameplayCueNotify_ForceFeedbackInfo BurstForceFeedback;
	//
	//// Input device properties to be applied on gameplay cue execution
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayCueNotify)
	//FGameplayCueNotify_InputDevicePropertyInfo BurstDevicePropertyEffect;
	//
	//// Decal to be spawned on gameplay cue execution.  Actor should have fade out time or override should be set so it will clean up properly.
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayCueNotify)
	//FGameplayCueNotify_DecalInfo BurstDecal;
};
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcGCN_Burst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UArcGCN_Burst();
	
protected:

	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnBurst(AActor* Target, const FGameplayCueParameters& Parameters, const FGameplayCueNotify_SpawnResult& SpawnResults) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // #if WITH_EDITOR

protected:

	// Default condition to check before spawning anything.  Applies for all spawns unless overridden.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Defaults")
	FArcGameplayCueNotify_SpawnCondition DefaultSpawnCondition;

	// Default placement rules.  Applies for all spawns unless overridden.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Defaults")
	FArcGameplayCueNotify_PlacementInfo DefaultPlacementInfo;

	// List of effects to spawn on burst.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Effects")
	FArcGameplayCueNotify_BurstEffects BurstEffects;
};
