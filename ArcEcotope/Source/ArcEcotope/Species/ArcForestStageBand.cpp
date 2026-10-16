// Copyright Lukasz Baran. All Rights Reserved.

#include "Species/ArcForestStageBand.h"

#include "Animation/TransformProviderData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace ArcForestStageBand
{
	/** Per-kind discriminator folded into the top half of every visual hash, which is what keeps that hash non-zero. */
	constexpr uint32 StaticMeshTypeSalt = 0x53544D31u;
	constexpr uint32 SkeletalMeshTypeSalt = 0x534B4D31u;
	constexpr uint32 EntityTypeSalt = 0x454E5431u;

	uint32 HashObject(const uint32 Seed, const UObject* Object)
	{
		return HashCombine(Seed, GetTypeHash(FSoftObjectPath(Object)));
	}

	uint32 HashMaterials(uint32 Seed, const TArray<TObjectPtr<UMaterialInterface>>& Materials)
	{
		Seed = HashCombine(Seed, static_cast<uint32>(Materials.Num()));
		for (const TObjectPtr<UMaterialInterface>& Material : Materials)
		{
			Seed = HashObject(Seed, Material.Get());
		}
		return Seed;
	}

	uint32 HashBand(uint32 Seed, const FArcForestStageBand& Band)
	{
		Seed = HashCombine(Seed, static_cast<uint32>(Band.Kind));
		Seed = HashCombine(Seed, GetTypeHash(Band.MinHeightCm));
		Seed = HashCombine(Seed, GetTypeHash(Band.MaxHeightCm));
		Seed = HashCombine(Seed, GetTypeHash(Band.Rotation.MaxYawDeg));
		Seed = HashCombine(Seed, GetTypeHash(Band.Rotation.MaxTiltDeg));
		Seed = HashCombine(Seed, GetTypeHash(Band.Rotation.MaxAlignDeg));
		Seed = HashCombine(Seed, GetTypeHash(Band.Rotation.bIgnoreStemBearing));
		Seed = HashCombine(Seed, GetTypeHash(Band.OffsetAlongUpCm));
		Seed = HashCombine(Seed, static_cast<uint32>(Band.Scale.Mode));
		Seed = HashCombine(Seed, GetTypeHash(Band.Scale.MinScale));
		Seed = HashCombine(Seed, GetTypeHash(Band.Scale.MaxScale));
		Seed = HashCombine(Seed, GetTypeHash(Band.Scale.RandomJitter));
		const FArcForestBandVisual* Visual = Band.Visual.GetPtr();
		return HashCombine(Seed, GetTypeHash(Visual != nullptr ? Visual->ComputeHash() : 0ull));
	}

	uint64 MakeRendererHash(const uint32 TypeSalt, const uint32 Payload)
	{
		return (static_cast<uint64>(TypeSalt) << 32) | static_cast<uint64>(Payload);
	}

	void CheckSelectionWeight(const int32 VariantIndex, const float SelectionWeight, TArray<FString>& OutErrors)
	{
		if (!FMath::IsFinite(SelectionWeight) || SelectionWeight <= 0.0f)
		{
			OutErrors.Add(FString::Printf(
				TEXT("Variants[%d].SelectionWeight must be finite and greater than zero."),
				VariantIndex));
		}
	}

	bool FinalizeErrors(const TArray<FString>& Errors, FText& OutError)
	{
		if (Errors.IsEmpty())
		{
			return true;
		}
		OutError = FText::FromString(FString::Join(Errors, TEXT(" ")));
		return false;
	}
}

uint64 FArcForestStageBand::ComputeHash() const
{
	using namespace ArcForestStageBand;

	return static_cast<uint64>(HashBand(0u, *this));
}

bool FArcForestBandVisual::IsValid(FText& OutError) const
{
	OutError = FText::FromString(TEXT("must select a concrete visual type; the abstract base spawns nothing."));
	return false;
}

uint64 FArcForestBandVisual_StaticMesh::ComputeHash() const
{
	using namespace ArcForestStageBand;

	// Variant order decides which stem draws which mesh, so it folds in as authored.
	uint32 Payload = static_cast<uint32>(Variants.Num());
	for (const FArcForestStaticMeshVariant& Variant : Variants)
	{
		Payload = HashObject(Payload, Variant.StaticMesh.Get());
		Payload = HashMaterials(Payload, Variant.MaterialOverrides);
		Payload = HashCombine(Payload, GetTypeHash(Variant.SelectionWeight));
		Payload = HashCombine(Payload, static_cast<uint32>(Variant.LengthAxis));
	}
	return MakeRendererHash(StaticMeshTypeSalt, Payload);
}

bool FArcForestBandVisual_StaticMesh::IsValid(FText& OutError) const
{
	using namespace ArcForestStageBand;

	TArray<FString> Errors;
	if (Variants.IsEmpty())
	{
		Errors.Add(TEXT("must carry at least one mesh variant."));
	}
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FArcForestStaticMeshVariant& Variant = Variants[VariantIndex];
		if (Variant.StaticMesh == nullptr)
		{
			Errors.Add(FString::Printf(TEXT("Variants[%d] must select a StaticMesh."), VariantIndex));
		}
		CheckSelectionWeight(VariantIndex, Variant.SelectionWeight, Errors);
	}
	return FinalizeErrors(Errors, OutError);
}

uint64 FArcForestBandVisual_SkinnedMesh::ComputeHash() const
{
	using namespace ArcForestStageBand;

	uint32 Payload = static_cast<uint32>(Variants.Num());
	for (const FArcForestSkeletalMeshVariant& Variant : Variants)
	{
		Payload = HashObject(Payload, Variant.SkeletalMesh.Get());
		Payload = HashMaterials(Payload, Variant.MaterialOverrides);
		Payload = HashObject(Payload, Variant.TransformProvider.Get());
		Payload = HashCombine(Payload, static_cast<uint32>(Variant.AnimationIndex));
		Payload = HashCombine(Payload, GetTypeHash(Variant.SelectionWeight));
	}
	return MakeRendererHash(SkeletalMeshTypeSalt, Payload);
}

bool FArcForestBandVisual_SkinnedMesh::IsValid(FText& OutError) const
{
	using namespace ArcForestStageBand;

	TArray<FString> Errors;
	if (Variants.IsEmpty())
	{
		Errors.Add(TEXT("must carry at least one mesh variant."));
	}
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FArcForestSkeletalMeshVariant& Variant = Variants[VariantIndex];
		if (Variant.SkeletalMesh == nullptr)
		{
			Errors.Add(FString::Printf(TEXT("Variants[%d] must select a SkeletalMesh."), VariantIndex));
		}
		if (Variant.TransformProvider == nullptr)
		{
			if (Variant.AnimationIndex != 0)
			{
				Errors.Add(FString::Printf(
					TEXT("Variants[%d] ref-pose mode requires AnimationIndex zero when no TransformProvider is set."),
					VariantIndex));
			}
		}
		else
		{
			const uint32 AnimationCount = Variant.TransformProvider->GetUniqueAnimationCount(Variant.SkeletalMesh.Get());
			if (Variant.AnimationIndex < 0 || static_cast<uint32>(Variant.AnimationIndex) >= AnimationCount)
			{
				Errors.Add(FString::Printf(
					TEXT("Variants[%d].AnimationIndex must be inside [0, %u) for its TransformProvider."),
					VariantIndex,
					AnimationCount));
			}
		}
		CheckSelectionWeight(VariantIndex, Variant.SelectionWeight, Errors);
	}
	return FinalizeErrors(Errors, OutError);
}

uint64 FArcForestBandVisual_Entity::ComputeHash() const
{
	using namespace ArcForestStageBand;

	return MakeRendererHash(EntityTypeSalt, GetTypeHash(EntityConfig));
}

bool FArcForestBandVisual_Entity::IsValid(FText& OutError) const
{
	using namespace ArcForestStageBand;

	TArray<FString> Errors;
	if (EntityConfig.IsNull())
	{
		Errors.Add(TEXT("must select an entity config asset."));
	}
	return FinalizeErrors(Errors, OutError);
}

namespace UE::ArcForest
{
	double SanitizeAngleDegrees(const float Degrees, const double MaxDegrees)
	{
		return FMath::IsFinite(Degrees) ? FMath::Clamp(static_cast<double>(Degrees), 0.0, MaxDegrees) : 0.0;
	}

	FQuat MakeMeshAxisFix(const EArcForestMeshLengthAxis LengthAxis)
	{
		switch (LengthAxis)
		{
		case EArcForestMeshLengthAxis::PlusZ:
			return FQuat::Identity;
		case EArcForestMeshLengthAxis::PlusX:
			return FQuat(FVector::YAxisVector, -UE_DOUBLE_HALF_PI);
		case EArcForestMeshLengthAxis::PlusY:
			return FQuat(FVector::XAxisVector, UE_DOUBLE_HALF_PI);
		case EArcForestMeshLengthAxis::MinusZ:
			return FQuat(FVector::XAxisVector, UE_DOUBLE_PI);
		case EArcForestMeshLengthAxis::MinusX:
			return FQuat(FVector::YAxisVector, UE_DOUBLE_HALF_PI);
		case EArcForestMeshLengthAxis::MinusY:
			return FQuat(FVector::XAxisVector, -UE_DOUBLE_HALF_PI);
		}
		return FQuat::Identity;
	}

	FQuat MakeTerrainAlign(const FVector& Normal, const float MaxAlignDeg)
	{
		const double MaxRadians = FMath::DegreesToRadians(SanitizeAngleDegrees(MaxAlignDeg, 90.0));
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Normal).GetSafeNormal();
		if (!(MaxRadians > 0.0) || Axis.IsNearlyZero())
		{
			return FQuat::Identity;
		}
		const double Angle = FMath::Acos(FMath::Clamp(Normal.Z, -1.0, 1.0));
		return FQuat(Axis, FMath::Min(Angle, MaxRadians));
	}
}
