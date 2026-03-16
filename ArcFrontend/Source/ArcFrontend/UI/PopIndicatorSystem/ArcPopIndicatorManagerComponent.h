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

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ControllerComponent.h"
#include "Delegates/DelegateCombinations.h"

#include "UI/PopIndicatorSystem/PopIndicatorDescriptor.h"
#include "ArcPopIndicatorManagerComponent.generated.h"

class FPopIndicatorDescriptor;


USTRUCT(BlueprintType)
struct FArcPopIndicatorData
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHitResult HitLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AActor> TargetActor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInstancedStruct CustomData;
};

USTRUCT(BlueprintType)
struct FArcPopIndicatorCustomData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	float Value = 0.f;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcIndicatorDynamicEvent, UUserWidget*, WidgetInit, FArcPopIndicatorData, IndicatorData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FArcIndicatorEvent, UUserWidget*, WidgetInit, FArcPopIndicatorData, IndicatorData);
/**
 * @class UArcIndicatorManagerComponent
 */
UCLASS(BlueprintType, Blueprintable)
class ARCFRONTEND_API UArcPopIndicatorManagerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Indicator")
	FArcIndicatorDynamicEvent OnAdded;
	
	UArcPopIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator")
	static UArcPopIndicatorManagerComponent* GetComponent(AController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator", meta = (BlueprintThreadSafe))
	void BP_AddIndicator(const FArcPopIndicatorData& PopData, TSoftClassPtr<UUserWidget> InWidgetClass, FArcIndicatorEvent OnWidgetAdded);
	
	void AddIndicator(const FPopIndicatorData& CanvasEntry
		, TFunctionRef<void(UUserWidget*)> WidgetInit
		, TSoftClassPtr<UUserWidget> InWidgetClass);

	void ReturnIndicator(const TSharedPtr<FPopIndicatorDescriptor>& CanvasEntry);

	void PopIndicator(const FPopIndicatorDescriptor& PopInfo);
	
	DECLARE_EVENT_TwoParams(UArcIndicatorManagerComponent, FIndicatorEvent, const TSharedRef<FPopIndicatorDescriptor>&, TFunctionRef<void(UUserWidget*)>)
	
	FIndicatorEvent OnIndicatorAdded;
	FIndicatorEvent OnIndicatorRemoved;
	
private:
	TArray<TSharedPtr<FPopIndicatorDescriptor>> CachedSlots;
	TArray<TSharedPtr<FPopIndicatorDescriptor>> ActiveSlots;

};
