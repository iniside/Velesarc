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

#include "UI/PopIndicatorSystem/PopIndicatorLayer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/LocalPlayer.h"
#include "UI/PopIndicatorSystem/SPopActorCanvas.h"

/////////////////////////////////////////////////////
// UIndicatorLayer

UPopIndicatorLayer::UPopIndicatorLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPopIndicatorLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyActorCanvas.Reset();
}

TSharedRef<SWidget> UPopIndicatorLayer::RebuildWidget()
{
	if (!IsDesignTime())
	{
		ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
		if (ensureMsgf(LocalPlayer, TEXT("Attempting to rebuild a UActorCanvas without a valid LocalPlayer!")))
		{
			MyActorCanvas = SNew(SPopActorCanvas, FLocalPlayerContext(LocalPlayer), &ArrowBrush);
			MyActorCanvas->Speed = Speed;
			MyActorCanvas->MaxLifeTime = MaxLifeTime;
			MyActorCanvas->VerticalDirectionAndSpeed = VerticalDirectionAndSpeed;
			MyActorCanvas->HorizontalDirectionAndSpeed = HorizontalDirectionAndSpeed;
			MyActorCanvas->bRandomizeHorizontalDirection = bRandomizeHorizontalDirection;
			MyActorCanvas->RequiredTags = RequiredTags;
			
			return MyActorCanvas.ToSharedRef();
		}
	}

	// Give it a trivial box, NullWidget isn't safe to use from a UWidget
	return SNew(SBox);
}
