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

#include "Components/ControllerComponent.h"
#include "Delegates/DelegateCombinations.h"

#include "ArcIndicatorManagerComponent.generated.h"

class FIndicatorDescriptor;

USTRUCT(BlueprintType)
struct FArcIndicatorHandle
{
	GENERATED_BODY()

	TWeakPtr<FIndicatorDescriptor> WeakPtr;

	FArcIndicatorHandle()
		: WeakPtr(nullptr)
	{
		
	}

	bool operator==(const FArcIndicatorHandle& Other) const
	{
		return WeakPtr == Other.WeakPtr;
	}

	void operator=(const FArcIndicatorHandle& Other)
	{
		WeakPtr = Other.WeakPtr;
	}

	void Reset()
	{
		WeakPtr.Reset();
	}
};

template <>
struct TStructOpsTypeTraits<FArcIndicatorHandle> : public TStructOpsTypeTraitsBase2<FArcIndicatorHandle>
{
	enum
	{
		WithCopy = true,
		WithIdenticalViaEquality = true
	};
};

/**
 * @class UArcIndicatorManagerComponent
 */
UCLASS(BlueprintType, Blueprintable)
class ARCFRONTEND_API UArcIndicatorManagerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UArcIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator")
	static UArcIndicatorManagerComponent* GetComponent(AController* Controller);

	void AddIndicator(const TSharedRef<FIndicatorDescriptor>& CanvasEntry);
	void RemoveIndicator(const TSharedRef<FIndicatorDescriptor>& CanvasEntry);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator", meta = (BlueprintThreadSafe))
	FArcIndicatorHandle AddIndicatorFromComponent(USceneComponent* TargetComponent
		, FName TargetComponentSocketName
		, TSoftClassPtr<UUserWidget> InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator", meta = (BlueprintThreadSafe))
	FArcIndicatorHandle AddIndicatorFromActor(AActor* TargetActor
		, FName TargetComponentSocketName
		, TSoftClassPtr<UUserWidget> InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Indicator", meta = (BlueprintThreadSafe))
	void RemoveIndicator(const FArcIndicatorHandle& Handle);
	
	DECLARE_EVENT_OneParam(UArcIndicatorManagerComponent, FIndicatorEvent, const TSharedRef<FIndicatorDescriptor>&)
	FIndicatorEvent OnIndicatorAdded;
	FIndicatorEvent OnIndicatorRemoved;

private:
	TArray<TSharedPtr<FIndicatorDescriptor>> Indicators;
};

UINTERFACE(BlueprintType)
class UArcIndicatorWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

class IArcIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mass|Actor Pooling")
	void SetIndicatorDescriptor(const FArcIndicatorHandle& InDescriptor);
};
