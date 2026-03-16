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

#include "SPopActorCanvas.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "UI/PopIndicatorSystem/ArcPopIndicatorManagerComponent.h"
#include "Widgets/Layout/SBox.h"

void SPopActorCanvas::Construct(const FArguments& InArgs, const FLocalPlayerContext& InLocalPlayerContext, const FSlateBrush* InActorCanvasArrowBrush)
{
	LocalPlayerContext = InLocalPlayerContext;
	ActorCanvasArrowBrush = InActorCanvasArrowBrush;

	IndicatorPool.SetWorld(LocalPlayerContext.GetWorld());

	SetCanTick(false);
	SetVisibility(EVisibility::SelfHitTestInvisible);


	bDrawElementsInOrder = false;
	bShowAnyIndicators = false;

	UpdateActiveTimer();
}

EActiveTimerReturnType SPopActorCanvas::UpdateCanvas(double InCurrentTime, float InDeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SPopActorCanvas_UpdateCanvas);

	if (!OptionalPaintGeometry.IsSet())
	{
		return EActiveTimerReturnType::Continue;
	}

	// Grab the local player
	ULocalPlayer* LocalPlayer = LocalPlayerContext.GetLocalPlayer();
	UArcPopIndicatorManagerComponent* IndicatorComponent = IndicatorComponentPtr.Get();
	if (IndicatorComponent == nullptr)
	{
		IndicatorComponent = UArcPopIndicatorManagerComponent::GetComponent(LocalPlayerContext.GetPlayerController());
		if (IndicatorComponent && !IndicatorComponent->OnIndicatorAdded.IsBound())
		{
			IndicatorComponentPtr = IndicatorComponent;
			IndicatorComponent->OnIndicatorAdded.AddSP(this, &SPopActorCanvas::OnIndicatorAdded);
		}
		else
		{
			//TODO HIDE EVERYTHING
			return EActiveTimerReturnType::Continue;
		}
	}

	//Make sure we have a player. If we don't, we can't project anything
	if (LocalPlayer)
	{
		FGeometry PaintGeometry = OptionalPaintGeometry.GetValue();

		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, /*out*/ ProjectionData))
		{
			SetShowAnyIndicators(true);

			bool IndicatorsChanged = false;

			for (int32 ChildIndex = 0; ChildIndex < CanvasChildren.Num(); ++ChildIndex)
			{
				SPopActorCanvas::FSlot& CurChild = CanvasChildren[ChildIndex];
				const TSharedPtr<FPopIndicatorDescriptor>& ActorEntry = CurChild.ActorEntry;
				ActorEntry->CurrentLifeTime += InDeltaTime;

				if (ActorEntry->bJustPoped)
				{
					ActorEntry->bJustPoped = false;
					FArcPopIndicatorData Data;
					Data.HitLocation = ActorEntry->HitLocation;
					Data.TargetActor = ActorEntry->TargetActor;
					Data.Tags.Reset();
					
					Data.Tags = ActorEntry->Tags;
					if (bRandomizeHorizontalDirection)
					{
						ActorEntry->RandomizedHorizontalDirection = FMath::RandBool() ? -1.0f : 1.0f;
					}
					IndicatorComponent->OnAdded.Broadcast(ActorEntry->IndicatorWidget.Get(), Data);
				}
				
				CurChild.ActorEntry->CurrentHorizontalOffset += (HorizontalDirectionAndSpeed * ActorEntry->RandomizedHorizontalDirection * CurChild.HorizontalDirection * InDeltaTime) * Speed;
				CurChild.ActorEntry->CurrentVerticalOffset += (VerticalDirectionAndSpeed * InDeltaTime) * Speed;

				
				
				if(ActorEntry->CurrentLifeTime > MaxLifeTime)
				{
					CanvasChildren.RemoveAt(ChildIndex);
					IndicatorComponentPtr->ReturnIndicator(ActorEntry);
					RemoveIndicatorForEntry(ActorEntry);
					// Decrement the current index to account for the removal 
					--ChildIndex;
					continue;
				}
				// If the slot content is invalid and we have permission to remove it
				if (ActorEntry->ShouldRemove())
				{
					IndicatorsChanged = true;
					IndicatorComponentPtr->ReturnIndicator(ActorEntry);
					CanvasChildren.RemoveAt(ChildIndex);
					// Decrement the current index to account for the removal 
					--ChildIndex;
					continue;
				}

				CurChild.SetIsIndicatorVisible(ActorEntry->GetIsVisible());

				if (!CurChild.GetIsIndicatorVisible())
				{
					IndicatorsChanged |= CurChild.bIsDirty();
					CurChild.ClearDirtyFlag();
					continue;
				}

				// If the indicator changed clamp status between updates, alert the indicator and mark the indicators as changed
				if (CurChild.WasIndicatorClampedStatusChanged())
				{
					//Indicator->OnIndicatorClampedStatusChanged(CurChild.WasIndicatorClamped());
					CurChild.ClearIndicatorClampedStatusChangedFlag();
					IndicatorsChanged = true;
				}

				FVector ScreenPositionWithDepth;
				bool Success = ActorEntry->Projector.Project(*ActorEntry.Get(), ProjectionData, PaintGeometry.Size, OUT ScreenPositionWithDepth);

				if (!Success)
				{
					CurChild.SetHasValidScreenPosition(false);
					CurChild.SetInFrontOfCamera(false);

					IndicatorsChanged |= CurChild.bIsDirty();
					CurChild.ClearDirtyFlag();
					continue;
				}

				CurChild.SetInFrontOfCamera(Success);
				CurChild.SetHasValidScreenPosition(CurChild.GetInFrontOfCamera() || ActorEntry->GetClampToScreen());

				if (CurChild.HasValidScreenPosition())
				{
					// Only dirty the screen position if we can actually show this indicator.
					CurChild.SetScreenPosition(FVector2D(ScreenPositionWithDepth));
					CurChild.SetDepth(ScreenPositionWithDepth.X);
				}

				CurChild.SetPriority(ActorEntry->GetPriority());

				IndicatorsChanged |= CurChild.bIsDirty();
				CurChild.ClearDirtyFlag();
			}

			if (IndicatorsChanged)
			{
				Invalidate(EInvalidateWidget::Paint);
			}
		}
		else
		{
			SetShowAnyIndicators(false);
		}
	}
	else
	{
		SetShowAnyIndicators(false);
	}

	if (CanvasChildren.Num() == 0)
	{
		TickHandle.Reset();
		return EActiveTimerReturnType::Stop;
	}
	else
	{
		return EActiveTimerReturnType::Continue;
	}
}

void SPopActorCanvas::SetShowAnyIndicators(bool bIndicators)
{
	if (bShowAnyIndicators != bIndicators)
	{
		bShowAnyIndicators = bIndicators;

		if (!bShowAnyIndicators)
		{
			for (int32 ChildIndex = 0; ChildIndex < AllChildren.Num(); ChildIndex++)
			{
				AllChildren.GetChildAt(ChildIndex)->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
}

void SPopActorCanvas::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SPopActorCanvas_OnArrangeChildren);

	NextArrowIndex = 0;

	//Make sure we have a player. If we don't, we can't project anything
	if (bShowAnyIndicators)
	{
		const FVector2D ArrowWidgetSize = ActorCanvasArrowBrush->GetImageSize();
		const FIntPoint FixedPadding = FIntPoint(10.0f, 10.0f) + FIntPoint(ArrowWidgetSize.X, ArrowWidgetSize.Y);
		const FVector Center = FVector(AllottedGeometry.Size * 0.5f, 0.0f);

		// Sort the children
		TArray<const SPopActorCanvas::FSlot*> SortedSlots;
		for (int32 ChildIndex = 0; ChildIndex < CanvasChildren.Num(); ++ChildIndex)
		{
			SortedSlots.Add(&CanvasChildren[ChildIndex]);
		}

		SortedSlots.StableSort([](const SPopActorCanvas::FSlot& A, const SPopActorCanvas::FSlot& B)
		{
			return A.GetPriority() == B.GetPriority() ? A.GetDepth() > B.GetDepth() : A.GetPriority() < B.GetPriority();
		});

		// Go through all the sorted children
		for (int32 ChildIndex = 0; ChildIndex < SortedSlots.Num(); ++ChildIndex)
		{
			//grab a child
			const SPopActorCanvas::FSlot& CurChild = *SortedSlots[ChildIndex];
			const TSharedRef<FPopIndicatorDescriptor>& ActorEntry = CurChild.ActorEntry;

			// Skip this indicator if it's invalid or has an invalid world position
			if (!ArrangedChildren.Accepts(CurChild.GetWidget()->GetVisibility()))
			{
				CurChild.SetWasIndicatorClamped(false);
				continue;
			}

			FVector2D ScreenPosition = CurChild.GetScreenPosition();
			const bool bInFrontOfCamera = CurChild.GetInFrontOfCamera();

			// Don't bother if we can't project the position and the indicator doesn't want to be clamped
			const bool bShouldClamp = ActorEntry->GetClampToScreen();

			//get the offset and final size of the slot
			FVector2D SlotSize, SlotOffset, SlotPaddingMin, SlotPaddingMax;
			GetOffsetAndSize(ActorEntry, SlotSize, SlotOffset, SlotPaddingMin, SlotPaddingMax);
			SlotOffset.X += CurChild.ActorEntry->CurrentHorizontalOffset;
			SlotOffset.Y += CurChild.ActorEntry->CurrentVerticalOffset;
			bool bWasIndicatorClamped = false;

			// If we don't have to clamp this thing, we can skip a lot of work
			if (bShouldClamp)
			{
				// Determine the size of inner screen rect to clamp within
				const FIntPoint RectMin = FIntPoint(SlotPaddingMin.X, SlotPaddingMin.Y) + FixedPadding;
				const FIntPoint RectMax = FIntPoint(AllottedGeometry.Size.X - SlotPaddingMax.X, AllottedGeometry.Size.Y - SlotPaddingMax.Y) - FixedPadding;
				const FIntRect ClampRect(RectMin, RectMax);

				// Make sure the screen position is within the clamp rect
				if (!ClampRect.Contains(FIntPoint(ScreenPosition.X, ScreenPosition.Y)))
				{
					const FPlane Planes[] =
					{
						FPlane(FVector(1.0f, 0.0f, 0.0f), ClampRect.Min.X),		// Left
						FPlane(FVector(0.0f, 1.0f, 0.0f), ClampRect.Min.Y),		// Top
						FPlane(FVector(-1.0f, 0.0f, 0.0f), -ClampRect.Max.X),	// Right
						FPlane(FVector(0.0f, -1.0f, 0.0f), -ClampRect.Max.Y)	// Bottom
					};
					
				}
				else if (!bInFrontOfCamera)
				{
					const float ScreenXNorm = ScreenPosition.X / (RectMax.X - RectMin.X);
					const float ScreenYNorm = ScreenPosition.Y / (RectMax.Y - RectMin.Y);
					//we need to pin this thing to the side of the screen
					if (ScreenXNorm < ScreenYNorm)
					{
						if (ScreenXNorm < (-ScreenYNorm + 1.0f))
						{
							ScreenPosition.X = ClampRect.Min.X;
						}
						else
						{
							ScreenPosition.Y = ClampRect.Max.Y;
						}
					}
					else
					{
						if (ScreenXNorm < (-ScreenYNorm + 1.0f))
						{
							ScreenPosition.Y = ClampRect.Min.Y;
						}
						else
						{
							ScreenPosition.X = ClampRect.Max.X;
						}
					}
				}
			}

			CurChild.SetWasIndicatorClamped(bWasIndicatorClamped);

			// Add the information about this child to the output list (ArrangedChildren)
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
				CurChild.GetWidget(),
				ScreenPosition + SlotOffset,
				SlotSize,
				1.f
			));
		}
	}

	if (NextArrowIndex < ArrowIndexLastUpdate)
	{
		for (int32 ArrowRemovedIndex = NextArrowIndex; ArrowRemovedIndex < ArrowIndexLastUpdate; ArrowRemovedIndex++)
		{
			ArrowChildren.GetChildAt(ArrowRemovedIndex)->SetVisibility(EVisibility::Collapsed);
		}
	}

	ArrowIndexLastUpdate = NextArrowIndex;
}

int32 SPopActorCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SPopActorCanvas_OnPaint);

	OptionalPaintGeometry = AllottedGeometry;
	//const bool NeedsTicks = CanvasChildren.Num() > 0 || !IndicatorComponentPtr.IsValid();
	//if(NeedsTicks)
	//{
	//	const_cast<SPopActorCanvas*>(this)->UpdateCanvas(Args.GetCurrentTime(), Args.GetDeltaTime());
	//}
	

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	int32 MaxLayerId = LayerId;

	const FPaintArgs NewArgs = Args.WithNewParent(this);
	const bool bShouldBeEnabled = ShouldBeEnabled(bParentEnabled);

	for (const FArrangedWidget& CurWidget : ArrangedChildren.GetInternalArray())
	{
		if (!IsChildWidgetCulled(MyCullingRect, CurWidget))
		{
			SWidget* MutableWidget = const_cast<SWidget*>(&CurWidget.Widget.Get());

			const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, MyCullingRect, OutDrawElements, bDrawElementsInOrder ? MaxLayerId : LayerId, InWidgetStyle, bShouldBeEnabled);
			MaxLayerId = FMath::Max(MaxLayerId, CurWidgetsMaxLayerId);
		}
		else
		{
			//SlateGI - RemoveContent
		}
	}

	return MaxLayerId;
}

void SPopActorCanvas::OnIndicatorAdded(const TSharedRef<FPopIndicatorDescriptor>& CanvasEntry, TFunctionRef<void(UUserWidget*)> WidgetInit)
{
	if (RequiredTags.IsEmpty())
	{
		AddIndicatorForEntry(CanvasEntry, WidgetInit);	
	}
	else
	{
		if (RequiredTags.HasAll(CanvasEntry->Tags))
		{
			AddIndicatorForEntry(CanvasEntry, WidgetInit);
		}
	}
	
}

void SPopActorCanvas::AddIndicatorForEntry(const TSharedRef<FPopIndicatorDescriptor>& CanvasEntry, TFunctionRef<void(UUserWidget*)> WidgetInit)
{
	// Async load the indicator, and pool the results so that it's easy to use and reuse the widgets.
	TSoftClassPtr<UUserWidget> IndicatorClass = CanvasEntry->GetIndicatorClass();
	if (!IndicatorClass.IsNull())
	{
		AsyncLoad(IndicatorClass, [this, CanvasEntry, IndicatorClass, WidgetInit]() {
			if (UUserWidget* IndicatorWidget = IndicatorPool.GetOrCreateInstance(TSubclassOf<UUserWidget>(IndicatorClass.Get())))
			{
				WidgetInit(IndicatorWidget);
				CanvasEntry->IndicatorWidget = IndicatorWidget;
				CanvasEntry->bJustPoped = true;
				
				AddActorSlot(CanvasEntry)
				[
					SAssignNew(CanvasEntry->CanvasHost, SBox)
					[
						IndicatorWidget->TakeWidget()
					]
				];
			}
		});
		StartAsyncLoading();
	}
}

void SPopActorCanvas::RemoveIndicatorForEntry(const TSharedPtr<FPopIndicatorDescriptor>& CanvasEntry)
{
	if (UUserWidget* IndicatorWidget = CanvasEntry->IndicatorWidget.Get())
	{
		IndicatorPool.Release(IndicatorWidget);
	}

	TSharedPtr<SWidget> CanvasHost = CanvasEntry->CanvasHost.Pin();
	if (CanvasHost.IsValid())
	{
		RemoveActorSlot(CanvasHost.ToSharedRef());
	}
}

SPopActorCanvas::FScopedWidgetSlotArguments SPopActorCanvas::AddActorSlot(const TSharedRef<FPopIndicatorDescriptor>& CanvasEntry)
{
	//float Dir = 
	//TODO  Try to prellaocate these slots instead of allocating new ones.
	TWeakPtr<SPopActorCanvas> WeakCanvas = SharedThis(this);
	return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(CanvasEntry), this->CanvasChildren, INDEX_NONE
		, [WeakCanvas](const FSlot* InSlot, int32)
		{
			const_cast<FSlot*>(InSlot)->HorizontalDirection = FMath::FRandRange(-0.35, 0.35);
			if (TSharedPtr<SPopActorCanvas> Canvas = WeakCanvas.Pin())
			{
				Canvas->UpdateActiveTimer();
			}
		}};
}

int32 SPopActorCanvas::RemoveActorSlot(const TSharedRef<SWidget>& SlotWidget)
{
	for (int32 SlotIdx = 0; SlotIdx < CanvasChildren.Num(); ++SlotIdx)
	{
		if ( SlotWidget == CanvasChildren[SlotIdx].GetWidget() )
		{
			CanvasChildren.RemoveAt(SlotIdx);

			UpdateActiveTimer();

			return SlotIdx;
		}
	}

	return -1;
}

void SPopActorCanvas::GetOffsetAndSize(const TSharedRef<FPopIndicatorDescriptor>& Entry,
	FVector2D& OutSize, 
	FVector2D& OutOffset,
	FVector2D& OutPaddingMin,
	FVector2D& OutPaddingMax) const
{
	//This might get used one day
	FVector2D AllottedSize = FVector2D::ZeroVector;

	//grab the desired size of the child widget
	TSharedPtr<SWidget> CanvasHost = Entry->CanvasHost.Pin();
	if (CanvasHost.IsValid())
	{
		OutSize = CanvasHost->GetDesiredSize();
	}

	//handle horizontal alignment
	switch(Entry->GetHAlign())
	{
		case HAlign_Left: // same as Align_Top
			OutOffset.X = 0.0f;
			OutPaddingMin.X = 0.0f;
			OutPaddingMax.X = OutSize.X;
			break;
		
		case HAlign_Center:
			OutOffset.X = (AllottedSize.X - OutSize.X) / 2.0f;
			OutPaddingMin.X = OutSize.X / 2.0f;
			OutPaddingMax.X = OutPaddingMin.X;
			break;
		
		case HAlign_Right: // same as Align_Bottom
			OutOffset.X = AllottedSize.X - OutSize.X;
			OutPaddingMin.X = OutSize.X;
			OutPaddingMax.X = 0.0f;
			break;
	}

	//Now, handle vertical alignment
	switch(Entry->GetVAlign())
	{
		case VAlign_Top:
			OutOffset.Y = 0.0f;
			OutPaddingMin.Y = 0.0f;
			OutPaddingMax.Y = OutSize.Y;
			break;
		
		case VAlign_Center:
			OutOffset.Y = (AllottedSize.Y - OutSize.Y) / 2.0f;
			OutPaddingMin.Y = OutSize.Y / 2.0f;
			OutPaddingMax.Y = OutPaddingMin.Y;
			break;
		
		case VAlign_Bottom:
			OutOffset.Y = AllottedSize.Y - OutSize.Y;
			OutPaddingMin.Y = OutSize.Y;
			OutPaddingMax.Y = 0.0f;
			break;
	}
}

void SPopActorCanvas::UpdateActiveTimer()
{
	const bool NeedsTicks = CanvasChildren.Num() > 0 || !IndicatorComponentPtr.IsValid();
	
	if (NeedsTicks && !TickHandle.IsValid())
	{
		TickHandle = RegisterActiveTimer(0, FWidgetActiveTimerDelegate::CreateSP(this, &SPopActorCanvas::UpdateCanvas));
	}
	else if (NeedsTicks == false && TickHandle.IsValid())
	{
		UnRegisterActiveTimer(TickHandle.ToSharedRef());
		TickHandle.Reset();
	}
}
