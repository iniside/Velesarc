// Copyright Lukasz Baran. All Rights Reserved.

#include "Paths/ArcTownPathComponent.h"

namespace ArcTownPathComponentPrivate
{
	constexpr float FallbackHalfWidth = 200.0f;

	float ResolveDefaultHalfWidth(const UObject& Metadata)
	{
		const UArcTownPathComponent* const Path = Metadata.GetTypedOuter<UArcTownPathComponent>();
		return Path != nullptr ? Path->DefaultHalfWidth : FallbackHalfWidth;
	}

	void ReindexInputKeys(FInterpCurveFloat& Curve)
	{
		for (int32 PointIndex = 0; PointIndex < Curve.Points.Num(); ++PointIndex)
		{
			Curve.Points[PointIndex].InVal = static_cast<float>(PointIndex);
		}
	}
}

UArcTownPathMetadata::UArcTownPathMetadata(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTownPathMetadata::InsertPoint(const int32 Index, const float t, const bool bClosedLoop)
{
	check(Index >= 0);

	const int32 NumPoints = HalfWidth.Points.Num();
	if (Index >= NumPoints)
	{
		AddPoint(static_cast<float>(Index));
		return;
	}

	Modify();

	const int32 PrevIndex = (bClosedLoop && Index == 0) ? NumPoints - 1 : Index - 1;
	float NewValue = HalfWidth.Points[Index].OutVal;
	if (PrevIndex >= 0 && PrevIndex < NumPoints)
	{
		NewValue = FMath::LerpStable(HalfWidth.Points[PrevIndex].OutVal, NewValue, t);
	}

	HalfWidth.Points.Insert(FInterpCurvePoint<float>(static_cast<float>(Index), NewValue), Index);
	ArcTownPathComponentPrivate::ReindexInputKeys(HalfWidth);
}

void UArcTownPathMetadata::UpdatePoint(const int32 Index, const float t, const bool bClosedLoop)
{
	const int32 NumPoints = HalfWidth.Points.Num();
	if (Index < 0 || Index >= NumPoints)
	{
		return;
	}

	const int32 PrevIndex = (bClosedLoop && Index == 0) ? NumPoints - 1 : Index - 1;
	const int32 NextIndex = (bClosedLoop && Index + 1 >= NumPoints) ? 0 : Index + 1;
	if (PrevIndex < 0 || PrevIndex >= NumPoints || NextIndex < 0 || NextIndex >= NumPoints)
	{
		return;
	}

	Modify();
	HalfWidth.Points[Index].OutVal =
		FMath::LerpStable(HalfWidth.Points[PrevIndex].OutVal, HalfWidth.Points[NextIndex].OutVal, t);
}

void UArcTownPathMetadata::AddPoint(const float InputKey)
{
	Modify();

	const int32 LastIndex = HalfWidth.Points.Num() - 1;
	const float NewValue = LastIndex >= 0
		? HalfWidth.Points[LastIndex].OutVal
		: ArcTownPathComponentPrivate::ResolveDefaultHalfWidth(*this);
	HalfWidth.Points.Emplace(static_cast<float>(LastIndex + 1), NewValue);
}

void UArcTownPathMetadata::RemovePoint(const int32 Index)
{
	if (!HalfWidth.Points.IsValidIndex(Index))
	{
		return;
	}

	Modify();
	HalfWidth.Points.RemoveAt(Index);
	ArcTownPathComponentPrivate::ReindexInputKeys(HalfWidth);
}

void UArcTownPathMetadata::DuplicatePoint(const int32 Index)
{
	if (!HalfWidth.Points.IsValidIndex(Index))
	{
		return;
	}

	Modify();
	HalfWidth.Points.Insert(FInterpCurvePoint<float>(HalfWidth.Points[Index]), Index);
	ArcTownPathComponentPrivate::ReindexInputKeys(HalfWidth);
}

void UArcTownPathMetadata::CopyPoint(const USplineMetadata* FromSplineMetadata, const int32 FromIndex, const int32 ToIndex)
{
	const UArcTownPathMetadata* const From = Cast<UArcTownPathMetadata>(FromSplineMetadata);
	if (From == nullptr || !From->HalfWidth.Points.IsValidIndex(FromIndex) || !HalfWidth.Points.IsValidIndex(ToIndex))
	{
		return;
	}

	Modify();
	HalfWidth.Points[ToIndex].OutVal = From->HalfWidth.Points[FromIndex].OutVal;
}

void UArcTownPathMetadata::Reset(const int32 NumPoints)
{
	Modify();
	HalfWidth.Points.Reset(NumPoints);
}

void UArcTownPathMetadata::Fixup(const int32 NumPoints, USplineComponent* SplineComp)
{
	const UArcTownPathComponent* const Path = Cast<UArcTownPathComponent>(SplineComp);
	const float DefaultValue = Path != nullptr
		? Path->DefaultHalfWidth
		: ArcTownPathComponentPrivate::ResolveDefaultHalfWidth(*this);

	ArcTownPathComponentPrivate::ReindexInputKeys(HalfWidth);
	while (HalfWidth.Points.Num() < NumPoints)
	{
		HalfWidth.Points.Emplace(static_cast<float>(HalfWidth.Points.Num()), DefaultValue);
	}
	if (HalfWidth.Points.Num() > NumPoints)
	{
		HalfWidth.Points.RemoveAt(NumPoints, HalfWidth.Points.Num() - NumPoints);
	}
}

UArcTownPathComponent::UArcTownPathComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PathMetadata = ObjectInitializer.CreateDefaultSubobject<UArcTownPathMetadata>(this, TEXT("PathMetadata"));
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
}

USplineMetadata* UArcTownPathComponent::GetSplinePointsMetadata()
{
	return PathMetadata;
}

const USplineMetadata* UArcTownPathComponent::GetSplinePointsMetadata() const
{
	return PathMetadata;
}

void UArcTownPathComponent::PostLoad()
{
	Super::PostLoad();
	SynchronizeMetadata();
}

void UArcTownPathComponent::OnRegister()
{
	Super::OnRegister();
	SynchronizeMetadata();
}

#if WITH_EDITOR

void UArcTownPathComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SynchronizeMetadata();
}

void UArcTownPathComponent::PostEditImport()
{
	Super::PostEditImport();
	SynchronizeMetadata();
}

#endif

void UArcTownPathComponent::SynchronizeMetadata()
{
	if (PathMetadata != nullptr)
	{
		PathMetadata->Fixup(GetNumberOfSplinePoints(), this);
	}
}
