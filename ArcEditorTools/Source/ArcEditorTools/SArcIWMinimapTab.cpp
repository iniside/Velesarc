// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcIWMinimapTab.h"

#include "ArcIWMinimapSettings.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"

#include "MassEntitySubsystem.h"
#include "MassEntityFragments.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SArcIWMinimapTab"

namespace ArcIWMinimap
{
	static UWorld* GetEditorWorld()
	{
		if (GEditor == nullptr)
		{
			return nullptr;
		}
		// PIE world first, then editor world
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return Context.World();
			}
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor && Context.World() != nullptr)
			{
				return Context.World();
			}
		}
		return nullptr;
	}
}

void SArcIWMinimapTab::Construct(const FArguments& InArgs)
{
	EntityCountText = SNew(STextBlock).Text(FText::FromString(TEXT("Entities: 0")));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FocusAll", "Focus All"))
				.OnClicked_Lambda([this]()
				{
					RefreshEntityData();
					FocusAll();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bShowInstances ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bShowInstances = (NewState == ECheckBoxState::Checked); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowInstances", "Instances"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bShowBounds ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bShowBounds = (NewState == ECheckBoxState::Checked); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowBounds", "Bounds"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				EntityCountText.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNullWidget::NullWidget
		]
	];

	UWorld* EditorWorld = ArcIWMinimap::GetEditorWorld();
	if (EditorWorld != nullptr)
	{
		const UArcIWMinimapSettings* Settings = UArcIWMinimapSettings::Get();
		EditorWorld->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			FTimerDelegate::CreateSP(this, &SArcIWMinimapTab::RefreshEntityData),
			Settings->RefreshInterval,
			true);
	}

	// Init camera from editor viewport
	if (GCurrentLevelEditingViewportClient != nullptr)
	{
		const FVector& ViewLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
		CameraCenter = FVector2D(ViewLocation.X, ViewLocation.Y);
	}

	RefreshEntityData();

	if (CachedEntities.Num() > 0)
	{
		FocusAll();
	}
}

FReply SArcIWMinimapTab::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandleLeftClick(MyGeometry, MouseEvent);
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bIsPanning = true;
		LastMousePos = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SArcIWMinimapTab::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bIsPanning)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SArcIWMinimapTab::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsPanning)
	{
		FVector2D CurrentMousePos = MouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentMousePos - LastMousePos;
		CameraCenter -= Delta * WorldUnitsPerPixel;
		LastMousePos = CurrentMousePos;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SArcIWMinimapTab::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	float WheelDelta = MouseEvent.GetWheelDelta();
	float Factor = FMath::Pow(1.1f, -WheelDelta);

	FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D Center = MyGeometry.GetLocalSize() * 0.5;
	FVector2D WorldPosBefore = ScreenToWorld(LocalMousePos, Center);

	WorldUnitsPerPixel = FMath::Clamp(WorldUnitsPerPixel * Factor, 1.0f, 100000.0f);

	FVector2D WorldPosAfter = ScreenToWorld(LocalMousePos, Center);
	CameraCenter += WorldPosBefore - WorldPosAfter;

	return FReply::Handled();
}

FVector2D SArcIWMinimapTab::WorldToScreen(const FVector2D& WorldPos, const FVector2D& Center) const
{
	FVector2D Offset = (WorldPos - CameraCenter) / WorldUnitsPerPixel;
	return Center + FVector2D(Offset.X, -Offset.Y);
}

FVector2D SArcIWMinimapTab::ScreenToWorld(const FVector2D& ScreenPos, const FVector2D& Center) const
{
	FVector2D Offset = ScreenPos - Center;
	return CameraCenter + FVector2D(Offset.X, -Offset.Y) * WorldUnitsPerPixel;
}

FLinearColor SArcIWMinimapTab::GetColorForClass(const FString& ClassName) const
{
	const UArcIWMinimapSettings* Settings = UArcIWMinimapSettings::Get();

	for (const TPair<TSoftClassPtr<AActor>, FLinearColor>& Pair : Settings->ActorClassColors)
	{
		if (Pair.Key.GetAssetName() == ClassName)
		{
			return Pair.Value;
		}
	}

	uint32 Hash = GetTypeHash(ClassName);
	FRandomStream Stream(Hash);
	float Hue = Stream.FRandRange(0.0f, 360.0f);
	FLinearColor Color = FLinearColor::MakeFromHSV8(
		static_cast<uint8>(Hue / 360.0f * 255.0f),
		180,
		220);
	return Color;
}

void SArcIWMinimapTab::RefreshEntityData()
{
	CachedEntities.Reset();
	CachedPartitionBounds.Reset();
	VisibleClassColors.Reset();

	UWorld* World = ArcIWMinimap::GetEditorWorld();
	if (World == nullptr)
	{
		if (EntityCountText.IsValid())
		{
			EntityCountText->SetText(FText::FromString(TEXT("Entities: 0 (no world)")));
		}
		return;
	}

	// Primary path: iterate partition actors directly
	for (AArcIWMassISMPartitionActor* PartitionActor : TActorRange<AArcIWMassISMPartitionActor>(World))
	{
		const TArray<FArcIWActorClassData>& ClassEntries = PartitionActor->GetActorClassEntries();

		FVector2D BoundsMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D BoundsMax(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
		bool bHasTransforms = false;

		for (const FArcIWActorClassData& ClassData : ClassEntries)
		{
			FString ClassName = ClassData.ActorClass != nullptr ? ClassData.ActorClass->GetName() : TEXT("Unknown");
			FLinearColor Color = GetColorForClass(ClassName);

			if (!VisibleClassColors.Contains(ClassName))
			{
				VisibleClassColors.Add(ClassName, Color);
			}

			for (const FTransform& Transform : ClassData.InstanceTransforms)
			{
				FVector2D Position(Transform.GetLocation().X, Transform.GetLocation().Y);

				FArcIWMinimapEntry Entry;
				Entry.Position = Position;
				Entry.Color = Color;
				Entry.bHydrated = false;
				CachedEntities.Add(Entry);

				BoundsMin.X = FMath::Min(BoundsMin.X, Position.X);
				BoundsMin.Y = FMath::Min(BoundsMin.Y, Position.Y);
				BoundsMax.X = FMath::Max(BoundsMax.X, Position.X);
				BoundsMax.Y = FMath::Max(BoundsMax.Y, Position.Y);
				bHasTransforms = true;
			}
		}

		if (bHasTransforms)
		{
			// Generate a muted color from the actor name hash
			uint32 NameHash = GetTypeHash(PartitionActor->GetFName());
			FRandomStream ColorStream(NameHash);
			float Hue = ColorStream.FRandRange(0.0f, 360.0f);
			FLinearColor BoundsColor = FLinearColor::MakeFromHSV8(
				static_cast<uint8>(Hue / 360.0f * 255.0f),
				80,
				160);
			BoundsColor.A = 0.6f;

			FArcIWMinimapPartitionBounds PartitionBounds;
			PartitionBounds.Bounds = FBox2D(BoundsMin, BoundsMax);
			PartitionBounds.Color = BoundsColor;
			PartitionBounds.PartitionActor = PartitionActor;
			CachedPartitionBounds.Add(PartitionBounds);
		}
	}

	// Fallback: if no partition actors found, use subsystem-based path
	if (CachedEntities.Num() == 0)
	{
		UArcIWVisualizationSubsystem* VisSub = World->GetSubsystem<UArcIWVisualizationSubsystem>();
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (VisSub != nullptr && MassSub != nullptr)
		{
			FMassEntityManager& EntityManager = MassSub->GetMutableEntityManager();
			const TMap<FIntVector, TArray<FMassEntityHandle>>& MeshCellEntities = VisSub->GetMeshCellEntities();

			for (const TPair<FIntVector, TArray<FMassEntityHandle>>& CellPair : MeshCellEntities)
			{
				for (const FMassEntityHandle& EntityHandle : CellPair.Value)
				{
					if (!EntityManager.IsEntityValid(EntityHandle))
					{
						continue;
					}

					const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
					if (TransformFrag == nullptr)
					{
						continue;
					}

					const FTransform& Transform = TransformFrag->GetTransform();
					FVector2D Position(Transform.GetLocation().X, Transform.GetLocation().Y);

					const FArcIWVisConfigFragment* ConfigFrag = EntityManager.GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(EntityHandle);
					FString ClassName = TEXT("Unknown");
					if (ConfigFrag != nullptr && ConfigFrag->ActorClass != nullptr)
					{
						ClassName = ConfigFrag->ActorClass->GetName();
					}

					const FArcIWInstanceFragment* InstanceFrag = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
					bool bHydrated = (InstanceFrag != nullptr) && InstanceFrag->bIsActorRepresentation;

					FLinearColor Color = GetColorForClass(ClassName);

					if (!VisibleClassColors.Contains(ClassName))
					{
						VisibleClassColors.Add(ClassName, Color);
					}

					FArcIWMinimapEntry Entry;
					Entry.Position = Position;
					Entry.Color = Color;
					Entry.bHydrated = bHydrated;
					CachedEntities.Add(Entry);
				}
			}
		}
	}

	if (EntityCountText.IsValid())
	{
		EntityCountText->SetText(FText::FromString(FString::Printf(TEXT("Entities: %d"), CachedEntities.Num())));
	}
}

void SArcIWMinimapTab::FocusAll()
{
	if (CachedEntities.Num() == 0)
	{
		CameraCenter = FVector2D::ZeroVector;
		return;
	}

	FVector2D MinBounds(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D MaxBounds(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());

	for (const FArcIWMinimapEntry& Entry : CachedEntities)
	{
		MinBounds.X = FMath::Min(MinBounds.X, Entry.Position.X);
		MinBounds.Y = FMath::Min(MinBounds.Y, Entry.Position.Y);
		MaxBounds.X = FMath::Max(MaxBounds.X, Entry.Position.X);
		MaxBounds.Y = FMath::Max(MaxBounds.Y, Entry.Position.Y);
	}

	CameraCenter = (MinBounds + MaxBounds) * 0.5;

	FVector2D Extent = MaxBounds - MinBounds;
	float MaxExtent = FMath::Max(Extent.X, Extent.Y);

	// Assume a reasonable viewport size; actual size will adjust on paint
	WorldUnitsPerPixel = FMath::Clamp(MaxExtent / 600.0f, 1.0f, 100000.0f);
}

int32 SArcIWMinimapTab::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	int32 CurrentLayerId = LayerId + 1;
	PaintBackground(AllottedGeometry, OutDrawElements, CurrentLayerId);
	PaintGrid(AllottedGeometry, OutDrawElements, CurrentLayerId);
	PaintPartitionBounds(AllottedGeometry, OutDrawElements, CurrentLayerId);
	if (bShowInstances)
	{
		PaintEntities(AllottedGeometry, OutDrawElements, CurrentLayerId);
	}
	PaintLegend(AllottedGeometry, OutDrawElements, CurrentLayerId);

	return CurrentLayerId;
}

void SArcIWMinimapTab::PaintBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
	FVector2D Size = AllottedGeometry.GetLocalSize();

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.02f, 0.02f, 0.03f, 1.0f));

	++LayerId;
}

void SArcIWMinimapTab::PaintGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const UArcIWMinimapSettings* Settings = UArcIWMinimapSettings::Get();
	float GridSpacing = Settings->GridSpacing;
	FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D Center = Size * 0.5;
	FLinearColor GridColor(0.15f, 0.15f, 0.2f, 1.0f);

	// Calculate world bounds visible in the widget
	FVector2D WorldTopLeft = ScreenToWorld(FVector2D::ZeroVector, Center);
	FVector2D WorldBottomRight = ScreenToWorld(Size, Center);

	float WorldMinX = FMath::Min(WorldTopLeft.X, WorldBottomRight.X);
	float WorldMaxX = FMath::Max(WorldTopLeft.X, WorldBottomRight.X);
	float WorldMinY = FMath::Min(WorldTopLeft.Y, WorldBottomRight.Y);
	float WorldMaxY = FMath::Max(WorldTopLeft.Y, WorldBottomRight.Y);

	float StartX = FMath::FloorToFloat(WorldMinX / GridSpacing) * GridSpacing;
	float StartY = FMath::FloorToFloat(WorldMinY / GridSpacing) * GridSpacing;

	// Vertical lines
	for (float WorldX = StartX; WorldX <= WorldMaxX; WorldX += GridSpacing)
	{
		FVector2D Top = WorldToScreen(FVector2D(WorldX, WorldMaxY), Center);
		FVector2D Bottom = WorldToScreen(FVector2D(WorldX, WorldMinY), Center);

		TArray<FVector2d> Points;
		Points.Add(FVector2d(Top.X, Top.Y));
		Points.Add(FVector2d(Bottom.X, Bottom.Y));

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			GridColor,
			true,
			1.0f);
	}

	// Horizontal lines
	for (float WorldY = StartY; WorldY <= WorldMaxY; WorldY += GridSpacing)
	{
		FVector2D Left = WorldToScreen(FVector2D(WorldMinX, WorldY), Center);
		FVector2D Right = WorldToScreen(FVector2D(WorldMaxX, WorldY), Center);

		TArray<FVector2d> Points;
		Points.Add(FVector2d(Left.X, Left.Y));
		Points.Add(FVector2d(Right.X, Right.Y));

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			GridColor,
			true,
			1.0f);
	}

	++LayerId;
}

void SArcIWMinimapTab::PaintPartitionBounds(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	if (!bShowBounds || CachedPartitionBounds.Num() == 0)
	{
		return;
	}

	FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D Center = Size * 0.5;

	for (const FArcIWMinimapPartitionBounds& PartBounds : CachedPartitionBounds)
	{
		FVector2D ScreenMin = WorldToScreen(PartBounds.Bounds.Min, Center);
		FVector2D ScreenMax = WorldToScreen(PartBounds.Bounds.Max, Center);

		// Y flips due to WorldToScreen inverting Y
		float Left = FMath::Min(ScreenMin.X, ScreenMax.X);
		float Right = FMath::Max(ScreenMin.X, ScreenMax.X);
		float Top = FMath::Min(ScreenMin.Y, ScreenMax.Y);
		float Bottom = FMath::Max(ScreenMin.Y, ScreenMax.Y);

		TArray<FVector2d> Points;
		Points.Add(FVector2d(Left, Top));
		Points.Add(FVector2d(Right, Top));
		Points.Add(FVector2d(Right, Bottom));
		Points.Add(FVector2d(Left, Bottom));
		Points.Add(FVector2d(Left, Top));

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			PartBounds.Color,
			true,
			2.0f);
	}

	++LayerId;
}

void SArcIWMinimapTab::PaintEntities(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const UArcIWMinimapSettings* Settings = UArcIWMinimapSettings::Get();
	float DotRadius = Settings->EntityDotRadius;
	FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D Center = Size * 0.5;
	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));

	for (const FArcIWMinimapEntry& Entry : CachedEntities)
	{
		FVector2D ScreenPos = WorldToScreen(Entry.Position, Center);

		// Cull offscreen
		if (ScreenPos.X < -DotRadius || ScreenPos.X > Size.X + DotRadius ||
			ScreenPos.Y < -DotRadius || ScreenPos.Y > Size.Y + DotRadius)
		{
			continue;
		}

		FVector2D DotSize(DotRadius * 2.0f, DotRadius * 2.0f);
		FVector2D DotPos(ScreenPos.X - DotRadius, ScreenPos.Y - DotRadius);

		if (Entry.bHydrated)
		{
			// Outer box (entity color)
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
				WhiteBrush,
				ESlateDrawEffect::None,
				Entry.Color);

			// Inner dark box (hollow effect)
			float InnerInset = FMath::Max(1.0f, DotRadius * 0.3f);
			FVector2D InnerSize(DotSize.X - InnerInset * 2.0f, DotSize.Y - InnerInset * 2.0f);
			FVector2D InnerPos(DotPos.X + InnerInset, DotPos.Y + InnerInset);

			if (InnerSize.X > 0.0f && InnerSize.Y > 0.0f)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(InnerSize, FSlateLayoutTransform(InnerPos)),
					WhiteBrush,
					ESlateDrawEffect::None,
					FLinearColor(0.02f, 0.02f, 0.03f, 1.0f));
			}
		}
		else
		{
			// Filled box
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPos)),
				WhiteBrush,
				ESlateDrawEffect::None,
				Entry.Color);
		}
	}

	++LayerId;
}

void SArcIWMinimapTab::HandleLeftClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (CachedPartitionBounds.Num() == 0 || GEditor == nullptr)
	{
		return;
	}

	FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D Center = MyGeometry.GetLocalSize() * 0.5;
	FVector2D WorldClick = ScreenToWorld(LocalMousePos, Center);

	AActor* BestActor = nullptr;
	double SmallestArea = TNumericLimits<double>::Max();

	for (const FArcIWMinimapPartitionBounds& PartBounds : CachedPartitionBounds)
	{
		if (!PartBounds.Bounds.IsInside(WorldClick))
		{
			continue;
		}

		FVector2D BoundsExtent = PartBounds.Bounds.Max - PartBounds.Bounds.Min;
		double Area = BoundsExtent.X * BoundsExtent.Y;

		if (Area < SmallestArea && PartBounds.PartitionActor.IsValid())
		{
			SmallestArea = Area;
			BestActor = PartBounds.PartitionActor.Get();
		}
	}

	if (BestActor != nullptr)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(BestActor, true, true, true);
	}
}

void SArcIWMinimapTab::PaintLegend(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	if (VisibleClassColors.Num() == 0)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	float LineHeight = 16.0f;
	float SwatchSize = 10.0f;
	float Padding = 6.0f;
	float TextPaddingLeft = 4.0f;

	// Calculate legend dimensions
	float MaxTextWidth = 0.0f;
	for (const TPair<FString, FLinearColor>& Pair : VisibleClassColors)
	{
		FVector2D TextSize = FontMeasure->Measure(Pair.Key, FontInfo);
		MaxTextWidth = FMath::Max(MaxTextWidth, static_cast<float>(TextSize.X));
	}

	float LegendWidth = Padding + SwatchSize + TextPaddingLeft + MaxTextWidth + Padding;
	float LegendHeight = Padding + VisibleClassColors.Num() * LineHeight + Padding;

	FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	FVector2D LegendPos(WidgetSize.X - LegendWidth - Padding, Padding);

	// Background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(LegendWidth, LegendHeight), FSlateLayoutTransform(LegendPos)),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	++LayerId;

	// Entries
	float CurrentY = LegendPos.Y + Padding;
	for (const TPair<FString, FLinearColor>& Pair : VisibleClassColors)
	{
		// Color swatch
		FVector2D SwatchPos(LegendPos.X + Padding, CurrentY + (LineHeight - SwatchSize) * 0.5f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(SwatchSize, SwatchSize), FSlateLayoutTransform(SwatchPos)),
			WhiteBrush,
			ESlateDrawEffect::None,
			Pair.Value);

		// Class name text
		FVector2D TextPos(LegendPos.X + Padding + SwatchSize + TextPaddingLeft, CurrentY);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(MaxTextWidth, LineHeight), FSlateLayoutTransform(TextPos)),
			Pair.Key,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor::White);

		CurrentY += LineHeight;
	}

	++LayerId;
}

#undef LOCTEXT_NAMESPACE
