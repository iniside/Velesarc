// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "UObject/Object.h"
#include "ArcMassFragments.generated.h"

USTRUCT()
struct ARCMASS_API FArcMassLastUpdateTimeFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	double LastUpdateTime = 0.0;
};

USTRUCT()
struct ARCMASS_API FArcAgentCapsuleFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float HalfHeight = 88.f;
};

UENUM()
enum class EArcMassFocusKind : uint8
{
	None,
	Entity,
	Location
};

USTRUCT()
struct ARCMASS_API FArcMassFocusFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	EArcMassFocusKind Kind = EArcMassFocusKind::None;

	UPROPERTY(VisibleAnywhere)
	FMassEntityHandle TargetEntity;

	UPROPERTY(VisibleAnywhere)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, meta = (ToolTip = "Serial of the action that set this focus; only that action's exit removes it."))
	uint32 OwnerSerial = 0;

	void SetEntity(FMassEntityHandle InEntity)
	{
		Kind = EArcMassFocusKind::Entity;
		TargetEntity = InEntity;
		TargetLocation = FVector::ZeroVector;
	}

	void SetLocation(const FVector& InLocation)
	{
		Kind = EArcMassFocusKind::Location;
		TargetEntity = FMassEntityHandle();
		TargetLocation = InLocation;
	}
};
