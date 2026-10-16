// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

// Plain shape buffer filled by IArcEditorEntityDrawContributor implementations and replayed each
// frame through UE::DebugDraw:: by UArcEditorEntityDrawSubsystem. Holds only POD shape data — no
// UObject references, no component, nothing serialized. Submit() is the single bridge to the
// DebugDraw plugin and is defined in ArcEditorDrawCollector.cpp (this header pulls no DebugDraw
// dependency, so contributor modules can include it freely).
struct FArcEditorDrawCollector
{
	struct FLine
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FColor Color = FColor::White;
		float Thickness = 1.f;
	};

	struct FSphere
	{
		FVector Center = FVector::ZeroVector;
		float Radius = 0.f;
		FColor Color = FColor::White;
	};

	struct FCircle
	{
		FVector Center = FVector::ZeroVector;
		float Radius = 0.f;
		FVector YAxis = FVector::YAxisVector;
		FVector ZAxis = FVector::ZAxisVector;
		FColor Color = FColor::White;
		float Thickness = 2.f;
		int32 Segments = 48;
	};

	struct FArrow
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FColor Color = FColor::White;
		float ArrowSize = 40.f;
		float Thickness = 3.f;
	};

	struct FText
	{
		FVector Location = FVector::ZeroVector;
		FString Text;
		FColor Color = FColor::White;
	};

	struct FBox
	{
		FVector Center = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FColor Color = FColor::White;
		float Thickness = 1.f;
	};

	struct FCapsule
	{
		FVector Center = FVector::ZeroVector;
		float HalfHeight = 0.f;
		float Radius = 0.f;
		FQuat Rotation = FQuat::Identity;
		FColor Color = FColor::White;
		float Thickness = 1.f;
	};

	// Collection API — signatures kept identical to the retired UArcEditorEntityDrawComponent so
	// existing contributors compile unchanged. AddBox / AddCapsule are new (used by the collision
	// contributor).
	void AddLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 1.f)
	{
		Lines.Add(FLine{ Start, End, Color, Thickness });
	}

	void AddSphere(const FVector& Center, float Radius, const FColor& Color)
	{
		Spheres.Add(FSphere{ Center, Radius, Color });
	}

	void AddCircle(const FVector& Center, float Radius, const FVector& YAxis, const FVector& ZAxis, const FColor& Color, float Thickness = 2.f, int32 Segments = 48)
	{
		Circles.Add(FCircle{ Center, Radius, YAxis, ZAxis, Color, Thickness, Segments });
	}

	void AddText(const FVector& Location, const FString& Text, const FColor& Color)
	{
		Texts.Add(FText{ Location, Text, Color });
	}

	void AddArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 40.f, float Thickness = 3.f)
	{
		Arrows.Add(FArrow{ Start, End, Color, ArrowSize, Thickness });
	}

	void AddBox(const FVector& Center, const FVector& Extent, const FQuat& Rotation, const FColor& Color, float Thickness = 1.f)
	{
		Boxes.Add(FBox{ Center, Extent, Rotation, Color, Thickness });
	}

	void AddCapsule(const FVector& Center, float HalfHeight, float Radius, const FQuat& Rotation, const FColor& Color, float Thickness = 1.f)
	{
		Capsules.Add(FCapsule{ Center, HalfHeight, Radius, Rotation, Color, Thickness });
	}

	void Reset()
	{
		Lines.Reset();
		Spheres.Reset();
		Circles.Reset();
		Arrows.Reset();
		Texts.Reset();
		Boxes.Reset();
		Capsules.Reset();
	}

	bool IsEmpty() const
	{
		return Lines.IsEmpty() && Spheres.IsEmpty() && Circles.IsEmpty()
			&& Arrows.IsEmpty() && Texts.IsEmpty() && Boxes.IsEmpty() && Capsules.IsEmpty();
	}

	// Replays every stored shape through UE::DebugDraw:: for one frame. Game thread only.
	// World is the scene key the shapes are submitted for; null submits scene-agnostic shapes
	// that render in every viewport.
	void Submit(const UWorld* World) const;

	TArray<FLine> Lines;
	TArray<FSphere> Spheres;
	TArray<FCircle> Circles;
	TArray<FArrow> Arrows;
	TArray<FText> Texts;
	TArray<FBox> Boxes;
	TArray<FCapsule> Capsules;
};
