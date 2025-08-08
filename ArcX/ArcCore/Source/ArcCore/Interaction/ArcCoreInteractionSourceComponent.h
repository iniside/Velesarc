// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScalableFloat.h"
#include "Components/ActorComponent.h"
#include "Player/ArcCorePlayerState.h"
#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "Tasks/TargetingSortTask_Base.h"
#include "Tasks/TargetingTask.h"
#include "Types/TargetingSystemTypes.h"

#include "InteractableTargetInterface.h"
#include "InteractionTypes.h"
#include "Components/PawnComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ArcCoreInteractionSourceComponent.generated.h"

class UMVVMViewModelBase;
class UUserWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcInteractionTargetsDynamicDelegate, TArray<TScriptInterface<IInteractionTarget>>, InteractionTargets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcInteractionTargetDynamicDelegate, TScriptInterface<IInteractionTarget>, InteractionTarget);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCORE_API UArcCoreInteractionSourceComponent : public UArcCorePlayerStateComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UTargetingPreset> InteractionTargetingPreset;

	FTargetingRequestHandle TargetingRequestHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Interactions")
	TArray<TScriptInterface<IInteractionTarget>> CurrentAvailableTargets;

	UPROPERTY(BlueprintAssignable, Category = "Interactions")
	FArcInteractionTargetsDynamicDelegate OnInteractionTargetsChanged;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Interactions")
	FArcInteractionTargetDynamicDelegate OnInteractionTargetChanged;
	
public:
	// Sets default values for this component's properties
	UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnPawnDataReady(APawn* Pawn) override;
	
	void HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle);

	UFUNCTION(BlueprintImplementableEvent, Category="Interactions")
	void OnAvailableInteractionsUpdated();
	virtual void NativeOnAvailableInteractionsUpdated() {}
	
	/**
	 * Triggers the interaction with one or more of the currently available targets.
	 * Override this in blueprints or native C++ to decide which of the currently available targets
	 * you would like to interact with and how.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Interactions")
	void OnTriggerInteraction();
	virtual void NativeOnTriggerInteraction() {}
};

UCLASS()
class ARCCORE_API UArcTargetingTask_SphereSweep : public UTargetingTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Radius = 700.f;
	
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};

UCLASS()
class UArcTargetingFilterTask_Interactable : public UTargetingFilterTask_BasicFilterTemplate
{
	GENERATED_BODY()

public:
	/** Evaluation function called by derived classes to process the targeting request */
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};

UCLASS(Blueprintable)
class UArcTargetingFilterTask_SortByDistance : public UTargetingSortTask_Base
{
	GENERATED_BODY()

protected:
	virtual float GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};

USTRUCT(BlueprintType)
struct FArcInteractionTarget_WidgetConfiguration : public  FInteractionTargetConfiguration
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftClassPtr<UUserWidget> DisplayWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftClassPtr<UMVVMViewModelBase> ViewModelClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	FName ViewModelName;
};

UCLASS()
class ARCCORE_API UArcInteractableInterfaceLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core", meta = (CustomStructureParam = "InteractionTargetConfiguration", ExpandBoolAsExecs = "ReturnValue"))
	static bool GetInteractionTargetConfiguration(const FInteractionQueryResults& Item
														  , UPARAM(meta = (MetaStruct = "/Script/InteractableInterface.InteractionTargetConfiguration")) UScriptStruct* InConfigType
														  , int32& InteractionTargetConfiguration);
	
	DECLARE_FUNCTION(execGetInteractionTargetConfiguration);
};