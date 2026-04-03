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

#include "IActorIndicatorWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"
#include "SceneView.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/WeakInterfacePtr.h"
#include "Mass/EntityHandle.h"

class FIndicatorDescriptor;
struct FMassEntityManager;

struct FActorCanvasProjection
{
	bool Project(FIndicatorDescriptor& CanvasEntry, const FSceneViewProjectionData& InProjectionData, const FVector2f& ScreenSize, FVector& ScreenPositionWithDepth, const FMassEntityManager* EntityManager);
};

enum struct EActorCanvasProjectionMode
{
	Point,
	BoundingBox
};

enum struct EIndicatorLocationMode : uint8
{
	Component,
	MassEntity,
	ManualTransform
};

class FIndicatorDescriptor
{
public:
	FIndicatorDescriptor(AActor* InActor)
		: Component(InActor->GetRootComponent())
	{
	}

	FIndicatorDescriptor(USceneComponent* InComponent, FName InComponentSocketName = NAME_None)
		: Component(InComponent)
		, ComponentSocketName(InComponentSocketName)
	{
	}

	FIndicatorDescriptor(FVector InWorldLocation)
		: LocationMode(EIndicatorLocationMode::ManualTransform)
		, ManualWorldLocation(InWorldLocation)
	{
	}

	FIndicatorDescriptor(FMassEntityHandle InEntity)
		: LocationMode(EIndicatorLocationMode::MassEntity)
		, MassEntity(InEntity)
	{
	}

	virtual ~FIndicatorDescriptor() = default;

public:
	// Core Properties
	//=======================

	TWeakObjectPtr<USceneComponent> Component;
	TWeakObjectPtr<AActor> LocationInterfaceActor;
	
	FName ComponentSocketName = NAME_None;
	EIndicatorLocationMode LocationMode = EIndicatorLocationMode::Component;
	FMassEntityHandle MassEntity;
	FVector ManualWorldLocation = FVector::ZeroVector;

	TSoftClassPtr<UUserWidget> GetIndicatorClass() const { return IndicatorWidgetClass; }
	void SetIndicatorClass(TSoftClassPtr<UUserWidget> InIndicatorWidgetClass)
	{
		IndicatorWidgetClass = InIndicatorWidgetClass;
	}

public:
	// TODO Organize this better.
	TWeakObjectPtr<UUserWidget> IndicatorWidget;
	FActorCanvasProjection Projector;

public:
	void SetAutoRemoveWhenIndicatorActorIsNull(bool InCanRemove)
	{
		bAutoRemoveWhenIndicatorActorIsNull = InCanRemove;
	}
	bool GetAutoRemoveWhenIndicatorActorIsNull() const { return bAutoRemoveWhenIndicatorActorIsNull; }

	bool ShouldRemove() const
	{
		if (LocationMode != EIndicatorLocationMode::Component)
		{
			return false;
		}
		return bAutoRemoveWhenIndicatorActorIsNull && Component.Get() == nullptr;
	}

public:
	// Layout Properties
	//=======================

	bool GetIsVisible() const
	{
		if (!bVisible)
		{
			return false;
		}
		if (LocationMode == EIndicatorLocationMode::Component && Component.Get() == nullptr)
		{
			return false;
		}
		return true;
	}
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

	FInstancedStruct InstanceData;
	
private:
	bool bVisible = true;
	bool bClampToScreen = false;
	bool bShowClampToScreenArrow = false;
	bool bOverrideScreenPosition = false;
	bool bAutoRemoveWhenIndicatorActorIsNull = false;

	EActorCanvasProjectionMode ProjectionMode = EActorCanvasProjectionMode::Point;
	EHorizontalAlignment HAlignment = HAlign_Center;
	EVerticalAlignment VAlignment = VAlign_Center;

	int32 Priority = 0;

	FVector2D BoundingBoxAnchor = FVector2D(0.5, 0.5);
	FVector2D ScreenSpaceOffset = FVector2D(0, 0);
	FVector WorldPositionOffset = FVector(0, 0, 0);

private:
	friend class SActorCanvas;

	TSoftClassPtr<UUserWidget> IndicatorWidgetClass;

	TWeakPtr<SWidget> Content;
	TWeakPtr<SWidget> CanvasHost;
};