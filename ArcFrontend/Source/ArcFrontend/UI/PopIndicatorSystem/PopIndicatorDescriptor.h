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


#include "GameplayTagContainer.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"
#include "SceneView.h"
#include "UObject/WeakInterfacePtr.h"

#include "UI/IndicatorSystem/IndicatorDescriptor.h"

class FPopIndicatorDescriptor;
class USceneComponent;

struct FPopActorCanvasProjection
{
	bool Project(FPopIndicatorDescriptor& CanvasEntry, const FSceneViewProjectionData& InProjectionData, const FVector2f& ScreenSize, FVector& ScreenPositionWithDepth);
};

class FPopIndicatorData
{
public:
	FText DisplayText;
	FGameplayTagContainer Tags;
	FHitResult HitLocation;
	TWeakObjectPtr<AActor> TargetActor;
};

class FPopIndicatorDescriptor
{
public:
	FPopIndicatorDescriptor()
		: Component(nullptr)
	{}

	FPopIndicatorDescriptor(AActor* InActor)
		: Component(InActor->GetRootComponent())
	{
	}

	FPopIndicatorDescriptor(const FHitResult& InHitLocation)
		: HitLocation(InHitLocation)
	{
	}

	FPopIndicatorDescriptor(USceneComponent* InComponent, FName InComponentSocketName = NAME_None)
		: Component(InComponent)
		, ComponentSocketName(InComponentSocketName)
	{
	}

	virtual ~FPopIndicatorDescriptor() = default;

public:
	// Core Properties
	//=======================
	FHitResult HitLocation;
	TWeakObjectPtr<USceneComponent> Component;
	TWeakObjectPtr<AActor> TargetActor;
	FGameplayTagContainer Tags;
	
	FName ComponentSocketName = NAME_None;

	TSoftClassPtr<UUserWidget> GetIndicatorClass() const { return IndicatorWidgetClass; }
	void SetIndicatorClass(TSoftClassPtr<UUserWidget> InIndicatorWidgetClass)
	{
		IndicatorWidgetClass = InIndicatorWidgetClass;
	}

public:
	// TODO Organize this better.
	TWeakObjectPtr<UUserWidget> IndicatorWidget;
	FPopActorCanvasProjection Projector;

public:
	void SetAutoRemoveWhenIndicatorActorIsNull(bool InCanRemove)
	{
		bAutoRemoveWhenIndicatorActorIsNull = InCanRemove;
	}
	bool GetAutoRemoveWhenIndicatorActorIsNull() const { return bAutoRemoveWhenIndicatorActorIsNull; }

	bool ShouldRemove() const
	{
		return bAutoRemoveWhenIndicatorActorIsNull && Component.Get() == nullptr;
	}

public:
	// Layout Properties
	//=======================

	bool GetIsVisible() const { return bVisible; }
	void SetIsVisible(bool InVisible)
	{
		bVisible = InVisible;
	}

	EActorCanvasProjectionMode GetProjectionMode() const { return ProjectionMode; }
	void SetProjectionMode(EActorCanvasProjectionMode InProjectionMode)
	{
		ProjectionMode = InProjectionMode;
	}

	// Horizontal alignment to the point in space to place the indicator at.
	EHorizontalAlignment GetHAlign() const { return HAlignment; }
	void SetHAlign(EHorizontalAlignment InHAlignment)
	{
		HAlignment = InHAlignment;
	}

	// Vertical alignment to the point in space to place the indicator at.
	EVerticalAlignment GetVAlign() const { return VAlignment; }
	void SetVAlign(EVerticalAlignment InVAlignment)
	{
		VAlignment = InVAlignment;
	}

	// Clamp the indicator to the edge of the screen?
	bool GetClampToScreen() const { return bClampToScreen; }
	void SetClampToScreen(bool bValue)
	{
		bClampToScreen = bValue;
	}

	// Show the arrow if clamping to the edge of the screen?
	bool GetShowClampToScreenArrow() const { return bShowClampToScreenArrow; }
	void SetShowClampToScreenArrow(bool bValue)
	{
		bShowClampToScreenArrow = bValue;
	}

	// The position offset for the indicator in world space.
	FVector GetWorldPositionOffset() const { return WorldPositionOffset; }
	void SetWorldPositionOffset(FVector Offset)
	{
		WorldPositionOffset = Offset;
	}

	// The position offset for the indicator in screen space.
	FVector2D GetScreenSpaceOffset() const { return ScreenSpaceOffset; }
	void SetScreenSpaceOffset(FVector2D Offset)
	{
		ScreenSpaceOffset = Offset;
	}

	// 
	FVector2D GetBoundingBoxAnchor() const { return BoundingBoxAnchor; }
	void SetBoundingBoxAnchor(FVector2D InBoundingBoxAnchor)
	{
		BoundingBoxAnchor = InBoundingBoxAnchor;
	}

public:
	// Sorting Properties
	//=======================

	// Allows sorting the indicators (after they are sorted by depth), to allow some group of indicators
	// to always be in front of others.
	int32 GetPriority() const { return Priority; }
	void SetPriority(int32 InPriority)
	{
		Priority = InPriority;
	}
	float CurrentLifeTime = 0;
	float CurrentVerticalOffset = 0;
	float CurrentHorizontalOffset = 0;
	
	bool bJustPoped = false;
	
private:
	bool bVisible = true;
	bool bClampToScreen = false;
	bool bShowClampToScreenArrow = false;
	bool bOverrideScreenPosition = false;
	bool bAutoRemoveWhenIndicatorActorIsNull = false;
	float RandomizedHorizontalDirection = 1.0f;


	EActorCanvasProjectionMode ProjectionMode = EActorCanvasProjectionMode::Point;
	EHorizontalAlignment HAlignment = HAlign_Center;
	EVerticalAlignment VAlignment = VAlign_Center;

	int32 Priority = 0;

	FVector2D BoundingBoxAnchor = FVector2D(0.5, 0.5);
	FVector2D ScreenSpaceOffset = FVector2D(0, 0);
	FVector WorldPositionOffset = FVector(0, 0, 0);

private:
	friend class SPopActorCanvas;

	TSoftClassPtr<UUserWidget> IndicatorWidgetClass;

	TWeakPtr<SWidget> Content;
	TWeakPtr<SWidget> CanvasHost;
};