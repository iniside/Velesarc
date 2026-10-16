// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFeatures/ArcMassGameFeatureAction_WorldActionBase.h"
#include "Input/ArcMassInputMappingContextAndPriority.h"

#include "ArcMassGameFeatureAction_AddInputContextMapping.generated.h"

class AActor;
class APlayerController;
class UInputMappingContext;
class UPlayer;
struct FComponentRequestHandle;

/**
 * Adds InputMappingContext entries to local players' EnhancedInput system.
 *
 * Mirror of ArcCore's UGameFeatureAction_AddInputContextMapping but decoupled from ArcCore;
 * routes through ArcMassGameplayFramework's UArcMassPawnExtensionComponent::NAME_BindInputsNow extension event.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Input Mapping (Mass)"))
class UArcMassGameFeatureAction_AddInputContextMapping final : public UArcMassGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	virtual void OnGameFeatureUnregistering() override;
	//~End of UGameFeatureAction interface

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FArcMassInputMappingContextAndPriority> InputMappings;

private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APlayerController>> ControllersAddedTo;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	/** Delegate for when the game instance is changed to register IMC's */
	FDelegateHandle RegisterInputContextMappingsForGameInstanceHandle;

	/** Registers owned Input Mapping Contexts to the Input Registry Subsystem. Also binds onto the start of GameInstances and the adding/removal of Local Players. */
	void RegisterInputMappingContexts();

	/** Registers owned Input Mapping Contexts to the Input Registry Subsystem for a specified GameInstance. This also gets called by a GameInstance Start. */
	void RegisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);

	/** Registers owned Input Mapping Contexts to the Input Registry Subsystem for a specified Local Player. This also gets called when a Local Player is added. */
	void RegisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);

	/** Unregisters owned Input Mapping Contexts from the Input Registry Subsystem. Also unbinds from the start of GameInstances and the adding/removal of Local Players. */
	void UnregisterInputMappingContexts();

	/** Unregisters owned Input Mapping Contexts from the Input Registry Subsystem for a specified GameInstance. */
	void UnregisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);

	/** Unregisters owned Input Mapping Contexts from the Input Registry Subsystem for a specified Local Player. This also gets called when a Local Player is removed. */
	void UnregisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);

	//~UArcMassGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End of UArcMassGameFeatureAction_WorldActionBase interface

	void Reset(FPerContextData& ActiveData);
	void HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddInputMappingForPlayer(UPlayer* Player, FPerContextData& ActiveData);
	void RemoveInputMapping(APlayerController* PlayerController, FPerContextData& ActiveData);
};
