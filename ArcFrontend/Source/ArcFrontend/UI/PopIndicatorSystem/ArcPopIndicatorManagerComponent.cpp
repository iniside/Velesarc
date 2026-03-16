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

#include "UI/PopIndicatorSystem/ArcPopIndicatorManagerComponent.h"
#include "UI/PopIndicatorSystem/PopIndicatorDescriptor.h"

UArcPopIndicatorManagerComponent::UArcPopIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bAutoActivate = true;
	for(int32 Idx = 0; Idx < 300; Idx++)
	{
		CachedSlots.Add(MakeShareable(new FPopIndicatorDescriptor()));
	}
}

UArcPopIndicatorManagerComponent* UArcPopIndicatorManagerComponent::GetComponent(AController* Controller)
{
	if (Controller)
	{
		return Controller->FindComponentByClass<UArcPopIndicatorManagerComponent>();
	}

	return nullptr;
}

void UArcPopIndicatorManagerComponent::BP_AddIndicator(const FArcPopIndicatorData& PopData, TSoftClassPtr<UUserWidget> InWidgetClass, FArcIndicatorEvent OnWidgetAdded)
{
	FPopIndicatorData NewPopData;
	NewPopData.HitLocation = PopData.HitLocation;
	NewPopData.TargetActor = PopData.TargetActor;
	
	FArcPopIndicatorData PopDataCopy = PopData;
	auto WidgetInit = [this, PopDataCopy, OnWidgetAdded](UUserWidget* Widget)
	{
		OnWidgetAdded.ExecuteIfBound(Widget, PopDataCopy);
	};

	AddIndicator(NewPopData, WidgetInit, InWidgetClass);
}

void UArcPopIndicatorManagerComponent::AddIndicator(const FPopIndicatorData& CanvasEntry
													, TFunctionRef<void(UUserWidget*)> WidgetInit
													, TSoftClassPtr<UUserWidget> InWidgetClass)
{
	TSharedPtr<FPopIndicatorDescriptor> Ptr = CachedSlots.Pop();
	Ptr->HitLocation = CanvasEntry.HitLocation;
	Ptr->TargetActor = CanvasEntry.TargetActor;
	Ptr->Tags = CanvasEntry.Tags;
	
	Ptr->SetClampToScreen(true);
	Ptr->SetProjectionMode(EActorCanvasProjectionMode::Point);
	Ptr->SetIndicatorClass(InWidgetClass);
	Ptr->CurrentLifeTime = 0;
	Ptr->CurrentVerticalOffset = 0;
	Ptr->CurrentHorizontalOffset = 0;
	OnIndicatorAdded.Broadcast(Ptr.ToSharedRef(), WidgetInit);
}

void UArcPopIndicatorManagerComponent::ReturnIndicator(const TSharedPtr<FPopIndicatorDescriptor>& CanvasEntry)
{
	CanvasEntry->IndicatorWidget.Reset();
	CachedSlots.Add(CanvasEntry);
}

void UArcPopIndicatorManagerComponent::PopIndicator(const FPopIndicatorDescriptor& PopInfo)
{
//	OnIndicatorAdded.Broadcast(PopInfo);
}
