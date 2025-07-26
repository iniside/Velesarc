// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcAttributesDebugger.h"
#include "ArcEquipmentDebugger.h"
#include "ArcGameplayAbilitiesDebugger.h"
#include "ArcGameplayEffectsDebugger.h"
#include "ArcGlobalAbilityTargetingDebugger.h"
#include "ArcItemAttachmentDebugger.h"
#include "ArcItemDebuggerItems.h"
#include "ArcProxyAttributeSetDebugger.h"
#include "ArcProxyGameplayEffectDebugger.h"
#include "ArcQuickBarDebugger.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcGameplayDebuggerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class ARCGAMEPLAYDEBUGGER_API UArcGameplayDebuggerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

private:
	bool bDrawDebug = false;

	FArcDebuggerItems DebuggerItems;
	FArcEquipmentDebugger EquipmentDebugger;
	FArcQuickBarDebugger QuickBarDebugger;
	FArcItemAttachmentDebugger ItemAttachmentDebugger;
	FArcGameplayAbilitiesDebugger GameplayAbilitiesDebugger;
	FArcGameplayEffectsDebugger GameplayEffectsDebugger;
	FArcAttributesDebugger AttributesDebugger; 
	FArcGlobalAbilityTargetingDebugger GlobalTargetingDebugger;

	FArcProxyGameplayEffectDebugger ProxyGameplayEffectDebugger; 
	FArcProxyAttributeSetDebugger ProxyAttributeSetDebugger;
	void Toggle();
	
public:
	//TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>> ItemInstancesDraw;
	
	UArcGameplayDebuggerSubsystem();
	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Implement this for deinitialization of instances of the system */
	virtual void Deinitialize() override;

	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}
	virtual bool IsTickable() const override final;
	
	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

	UFUNCTION(exec)
	void ToggleDebug();
};
