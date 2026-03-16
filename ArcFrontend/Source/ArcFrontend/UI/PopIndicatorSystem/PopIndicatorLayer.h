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


#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "GameplayTagContainer.h"

#include "PopIndicatorLayer.generated.h"

class SPopActorCanvas;

UCLASS()
class UPopIndicatorLayer : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Default arrow brush to use if UI is clamped to the screen and needs to show an arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush ArrowBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	float Speed = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
    float MaxLifeTime = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HorizontalDirectionAndSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRandomizeHorizontalDirection = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalDirectionAndSpeed = -150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;
	
protected:
	// UWidget interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End UWidget

protected:
	TSharedPtr<SPopActorCanvas> MyActorCanvas;
};
