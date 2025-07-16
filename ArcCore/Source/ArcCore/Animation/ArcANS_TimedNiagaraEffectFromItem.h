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

#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "NiagaraAnimNotifies/Public/AnimNotifyState_TimedNiagaraEffect.h"
#include "ArcANS_TimedNiagaraEffectFromItem.generated.h"

class UArcItemDefinition;
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcANS_TimedNiagaraEffectFromItem : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/**
	 * If tag is preset Niagara System will be selected from:
	 * @sruct FArcItemFragment_NiagaraSystemMap instead
	 */
	UPROPERTY(EditAnywhere, Category = NiagaraSystem)
	FGameplayTag NiagaraSystemTag;

	// The socket within our mesh component to attach to when we spawn the Niagara component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem, meta = (ToolTip = "The socket or bone to attach the system to", AnimNotifyBoneName = "true"))
	FName SocketName;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem, meta = (ToolTip = "The niagara system to spawn for the notify state"))
	TObjectPtr<UNiagaraSystem> PreviewTemplate;
#endif

	/**
	 * If set, notify will attempt finding item on slot and it's actor/component attachment and attaching to it.
	 */
	UPROPERTY(EditAnywhere, meta = (Categories = "SlotId"), Category = NiagaraSystem)
	FGameplayTag ItemSlotId;

	/** Socket on actor/component attachment  to which to attach system. */
	UPROPERTY(EditAnywhere, Category = NiagaraSystem)
	FName ItemAttachmentSocketName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem)
	TEnumAsByte<EAttachLocation::Type> LocationType = EAttachLocation::KeepRelativeOffset;
	
	// Offset from the socket / bone location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem, meta = (ToolTip = "Offset from the socket or bone to place the Niagara system"))
	FVector LocationOffset;

	// Offset from the socket / bone rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem, meta = (ToolTip = "Rotation offset from the socket or bone for the Niagara system"))
	FRotator RotationOffset;

	// Scale to spawn the Niagara system at
	UPROPERTY(EditAnywhere, Category = NiagaraSystem)
	FVector Scale = FVector(1.0f);

	// Should we set the animation rate scale as time dilation on the spawn effect?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem)
	bool bApplyRateScaleAsTimeDilation = false;

	// Whether or not we destroy the component at the end of the notify or instead just stop
	// the emitters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NiagaraSystem, meta = (DisplayName = "Destroy Immediately", ToolTip = "Whether the Niagara system should be immediately destroyed at the end of the notify state or be allowed to finish"))
	bool bDestroyAtEnd;
	
	UArcANS_TimedNiagaraEffectFromItem(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, const FAnimNotifyEventReference& EventReference) override;

	// Overridden from UAnimNotifyState to provide custom notify name.
	FString GetNotifyName_Implementation() const override;

	// Return FXSystemComponent created from SpawnEffect
	UFUNCTION(BlueprintCallable, Category = "AnimNotify")
	UFXSystemComponent* GetSpawnedEffect(UMeshComponent* MeshComp, UAnimSequenceBase * Animation);

protected:
	// Spawns the NiagaraSystemComponent. Called from Notify.
	virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const;

	bool ValidateParameters(USkeletalMeshComponent* MeshComp) const;

	FName GetSpawnedComponentTag() const;
};
