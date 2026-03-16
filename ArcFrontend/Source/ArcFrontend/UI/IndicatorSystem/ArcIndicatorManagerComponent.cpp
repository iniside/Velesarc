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

#include "UI/IndicatorSystem/ArcIndicatorManagerComponent.h"

#include "IndicatorDescriptor.h"

UArcIndicatorManagerComponent::UArcIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bAutoActivate = true;
}

/*static*/ UArcIndicatorManagerComponent* UArcIndicatorManagerComponent::GetComponent(AController* Controller)
{
	if (Controller)
	{
		return Controller->FindComponentByClass<UArcIndicatorManagerComponent>();
	}

	return nullptr;
}

void UArcIndicatorManagerComponent::AddIndicator(const TSharedRef<FIndicatorDescriptor>& CanvasEntry)
{
	OnIndicatorAdded.Broadcast(CanvasEntry);
	Indicators.Add(CanvasEntry);
}

void UArcIndicatorManagerComponent::RemoveIndicator(const TSharedRef<FIndicatorDescriptor>& CanvasEntry)
{
	OnIndicatorRemoved.Broadcast(CanvasEntry);
	Indicators.Remove(CanvasEntry);
}

FArcIndicatorHandle UArcIndicatorManagerComponent::AddIndicatorFromComponent(USceneComponent* TargetComponent, FName TargetComponentSocketName
	, TSoftClassPtr<UUserWidget> InWidgetClass)
{
	TSharedPtr<FIndicatorDescriptor> NewDescriptor = MakeShareable(new FIndicatorDescriptor(TargetComponent, TargetComponentSocketName));
	NewDescriptor->SetIndicatorClass(InWidgetClass);
	
	AddIndicator(NewDescriptor.ToSharedRef());

	FArcIndicatorHandle NewHandle;
	NewHandle.WeakPtr = NewDescriptor;
	return NewHandle;
}

FArcIndicatorHandle UArcIndicatorManagerComponent::AddIndicatorFromActor(AActor* TargetActor, FName TargetComponentSocketName
	, TSoftClassPtr<UUserWidget> InWidgetClass)
{
	if (!TargetActor || !TargetActor->GetRootComponent())
	{
		return FArcIndicatorHandle();
	}
	
	TSharedPtr<FIndicatorDescriptor> NewDescriptor = MakeShareable(new FIndicatorDescriptor(TargetActor));
	NewDescriptor->ComponentSocketName = TargetComponentSocketName;
	if (TargetActor->Implements<UArcIndicatorLocationInterface>())
	{
		NewDescriptor->LocationInterfaceActor = TargetActor;
	}
	
	NewDescriptor->SetIndicatorClass(InWidgetClass);

	AddIndicator(NewDescriptor.ToSharedRef());

	FArcIndicatorHandle NewHandle;
	NewHandle.WeakPtr = NewDescriptor;
	return NewHandle;
}

void UArcIndicatorManagerComponent::RemoveIndicator(const FArcIndicatorHandle& Handle)
{
	if (Handle.WeakPtr.IsValid())
	{
		RemoveIndicator(Handle.WeakPtr.Pin().ToSharedRef());
	}
}
