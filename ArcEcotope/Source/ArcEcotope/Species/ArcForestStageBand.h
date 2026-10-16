// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPath.h"

#include "ArcForestStageBand.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UTransformProviderData;

/**
 * Which physical state of a stem a height band answers for. It is the axis the
 * instance builder branches on before size: the five dead kinds map one-to-one
 * onto EArcForestDeadwoodType, and every living record — individual, cohort stem
 * or patch stem — is Alive.
 *
 * A Kind with no band at all produces no instances: the builder logs a warning
 * naming the skipped count and moves on. The dead kinds are the usual victims,
 * because nothing in the authoring flow asks for them.
 */
UENUM(BlueprintType)
enum class EArcForestVisualKind : uint8
{
	Alive,
	Snag,
	Log,
	Stump,
	Uprooted,
	Branch,
};

/**
 * Side of the World Partition cell whose partition actor carries a species'
 * components. A doubling series rather than a free number: the cell side is
 * folded into the partition actor's grid guid and its name, so an arbitrary
 * value silently forks a whole new actor family per species.
 */
UENUM()
enum class EArcForestGridSize : int32
{
	Cm800 = 800 UMETA(DisplayName = "8 m"),
	Cm1600 = 1600 UMETA(DisplayName = "16 m"),
	Cm3200 = 3200 UMETA(DisplayName = "32 m"),
	Cm6400 = 6400 UMETA(DisplayName = "64 m"),
	Cm12800 = 12800 UMETA(DisplayName = "128 m"),
	Cm25600 = 25600 UMETA(DisplayName = "256 m"),
	Cm51200 = 51200 UMETA(DisplayName = "512 m"),
	Cm102400 = 102400 UMETA(DisplayName = "1024 m"),
	Cm204800 = 204800 UMETA(DisplayName = "2048 m"),
	Cm409600 = 409600 UMETA(DisplayName = "4096 m"),
};

/**
 * Per-instance orientation spread for one height band.
 *
 * Yaw is drawn symmetrically about the mesh's authored facing, so the default
 * 180 is the full circle and small values keep a species roughly aligned —
 * useful for anything whose silhouette reads directionally, such as a
 * wind-combed shrub or a leaning snag.
 *
 * Tilt leans the whole instance away from vertical by a random angle up to
 * MaxTiltDeg, in a random horizontal direction, and it composes on top of
 * whatever rotation the instance already has. On the dead kinds that means
 * it is added to the deadwood record's own stored rotation rather than
 * replacing it. Keep it small for live plants; a few degrees is enough to break
 * the "planted with a plumb line" look, and large values on a trunk mesh read
 * as damage rather than variation.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestInstanceRotation
{
	GENERATED_BODY()

	/** Half-width of the yaw draw, in degrees. 180 is a full uniform circle. Log and Uprooted bands author 0: the draw composes on top of the fall bearing the simulation stored and re-scatters a storm's parallel stems. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYawDeg = 180.0f;

	/** Largest lean away from vertical, in degrees. Zero leaves instances upright. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxTiltDeg = 0.0f;

	/** Ceiling on the rotation toward the terrain normal, in degrees. A value at or above the slope angle reaches full normal alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxAlignDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
	bool bIgnoreStemBearing = false;
};

/**
 * Chooses what drives per-instance scale.
 *
 * Random is independent of the plant: a uniform draw across the authored range,
 * which is what a ground-cover or rock-like visual usually wants.
 *
 * FromHeight and FromAge instead track the simulated plant, normalizing against
 * MaxHeightCm and MaxAgeYears respectively, so one authored mesh can carry a
 * band across the whole span it covers, and a stem grows with every simulated
 * year rather than jumping between sizes. FromAge on a dead kind sits at the
 * bottom of the range, MinScale scaled by that stem's jitter: deadwood records
 * store no plant age, only the height and diameter the stem had when it died,
 * so a dead band that should read as sized wants FromHeight.
 */
UENUM(BlueprintType)
enum class EArcForestInstanceScaleMode : uint8
{
	Random,
	FromHeight,
	FromAge,
};

/**
 * Per-instance uniform scale for one height band. The emitted scale is ALWAYS
 * inside [MinScale, MaxScale] — jitter is applied first and the result is then
 * clamped back into the range, so the authored bounds are a hard guarantee
 * rather than a nominal target.
 *
 * Jitter is what stops a cohort from looking cloned. Cohorts and patches expand
 * many stems out of a single record with one mean height and one mean age, so
 * in the two derived modes every stem of a record computes the same base value
 * and only the jitter, which is drawn per stem, separates them. A derived mode
 * with jitter at zero renders a record as identical copies.
 *
 * The jitter is a fixed multiplier for the life of a stem, so a stem keeps its
 * offset from the record mean instead of resizing every simulated year.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestInstanceScale
{
	GENERATED_BODY()

	/** See EArcForestInstanceScaleMode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
	EArcForestInstanceScaleMode Mode = EArcForestInstanceScaleMode::Random;

	/** Hard lower bound on the emitted scale, and the value the derived modes start from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale", meta = (ClampMin = "0.0001"))
	float MinScale = 1.0f;

	/** Hard upper bound on the emitted scale, and the value the derived modes reach at full size or full age. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale", meta = (ClampMin = "0.0001"))
	float MaxScale = 1.0f;

	/** Multiplicative spread applied after the mode, as a fraction: 0.1 means +/-10%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale", meta = (ClampMin = "0.0"))
	float RandomJitter = 0.1f;
};

UENUM(BlueprintType)
enum class EArcForestMeshLengthAxis : uint8
{
	PlusZ,
	PlusX,
	PlusY,
	MinusZ,
	MinusX,
	MinusY
};

/**
 * One interchangeable static look for a height band. A band draws one variant
 * per stem by SelectionWeight, so a cohort of a thousand seedlings expanded from
 * a single record still shows every authored variant.
 *
 * Each variant becomes its own instanced component: N variants on a band means N
 * draw batches for that band. A handful is cheap, dozens are not.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestStaticMeshVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/**
	 * Relative draw weight against the other variants of the same band. Only the
	 * ratios matter, and it must be strictly positive — a variant that should
	 * never appear is deleted, not weighted to zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (ClampMin = "0.0001"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	EArcForestMeshLengthAxis LengthAxis = EArcForestMeshLengthAxis::PlusZ;
};

/** One interchangeable skeletal look for a height band. See FArcForestStaticMeshVariant for the draw and batching cost. */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestSkeletalMeshVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UTransformProviderData> TransformProvider;

	/** Provider animation index. Index zero selects ref-pose mode when no provider is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0"))
	int32 AnimationIndex = 0;

	/** See FArcForestStaticMeshVariant::SelectionWeight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (ClampMin = "0.0001"))
	float SelectionWeight = 1.0f;
};

/** What one height band spawns. The concrete type decides which component or entity the instance builder creates, and bands of one species may each pick a different one. */
USTRUCT()
struct ARCECOTOPE_API FArcForestBandVisual
{
	GENERATED_BODY()

	virtual ~FArcForestBandVisual() = default;

	/**
	 * Folded into the species visual hash. A concrete visual never returns zero:
	 * zero is indistinguishable from "unset" and would collide across visual
	 * kinds. The base returns zero rather than PURE_VIRTUAL because the
	 * instanced-struct picker can still hand an unspecialized base struct to a
	 * hashing call site, and PURE_VIRTUAL is a fatal error outside shipping-safe paths.
	 */
	virtual uint64 ComputeHash() const { return 0; }

	virtual bool IsValid(FText& OutError) const;
};

/** Draws the band through UInstancedStaticMeshComponent, from the species' static-mesh descriptor. */
USTRUCT()
struct ARCECOTOPE_API FArcForestBandVisual_StaticMesh : public FArcForestBandVisual
{
	GENERATED_BODY()

	virtual uint64 ComputeHash() const override;
	virtual bool IsValid(FText& OutError) const override;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	TArray<FArcForestStaticMeshVariant> Variants;
};

/** Draws the band through UInstancedSkinnedMeshComponent, from the species' skinned-mesh descriptor. */
USTRUCT()
struct ARCECOTOPE_API FArcForestBandVisual_SkinnedMesh : public FArcForestBandVisual
{
	GENERATED_BODY()

	virtual uint64 ComputeHash() const override;
	virtual bool IsValid(FText& OutError) const override;

	UPROPERTY(EditAnywhere, Category = "Mesh")
	TArray<FArcForestSkeletalMeshVariant> Variants;
};

/** Spawns the band as Mass entities on the placed-entity partition actors. The entity backend has no interchangeable variants: a band is one config. */
USTRUCT()
struct ARCECOTOPE_API FArcForestBandVisual_Entity : public FArcForestBandVisual
{
	GENERATED_BODY()

	virtual uint64 ComputeHash() const override;
	virtual bool IsValid(FText& OutError) const override;

	/**
	 * FSoftObjectPath rather than TObjectPtr<UArcMassEntityConfigAsset>: this struct lives in the
	 * runtime ArcEcotope module, which loads at PostConfigInit and so cannot take a module dependency
	 * on ArcMass (Default). The editor module loads and casts it; AllowedClasses keeps the picker
	 * honest without the compile-time dependency.
	 */
	UPROPERTY(EditAnywhere, Category = "Entity",
		meta = (AllowedClasses = "/Script/ArcMass.ArcMassEntityConfigAsset"))
	FSoftObjectPath EntityConfig;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestStageBand
{
	GENERATED_BODY()

	/** The stem state this band answers for. See EArcForestVisualKind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	EArcForestVisualKind Kind = EArcForestVisualKind::Alive;

	/** Smallest size this band covers, inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.0", Units = "cm"))
	float MinHeightCm = 0.0f;

	/** Largest size this band covers, inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxHeightCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FArcForestInstanceRotation Rotation;

	/** Per-instance uniform scale. See FArcForestInstanceScale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FArcForestInstanceScale Scale;

	/** Shift along the align axis, scaled by the emitted instance scale; negative sinks the mesh into the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform", meta = (Units = "cm"))
	float OffsetAlongUpCm = 0.0f;

	/** What this band spawns. See FArcForestBandVisual. */
	UPROPERTY(EditAnywhere, Category = "Visual", meta = (ExcludeBaseStruct))
	TInstancedStruct<FArcForestBandVisual> Visual;

	/** Folds the authored band fields and the visual's own hash. */
	uint64 ComputeHash() const;
};

namespace UE::ArcForest
{
	ARCECOTOPE_API double SanitizeAngleDegrees(float Degrees, double MaxDegrees);

	/** Carries the authored length axis onto +Z, the axis the simulation's stem rotation orients. */
	ARCECOTOPE_API FQuat MakeMeshAxisFix(EArcForestMeshLengthAxis LengthAxis);

	ARCECOTOPE_API FQuat MakeTerrainAlign(const FVector& Normal, float MaxAlignDeg);
}
