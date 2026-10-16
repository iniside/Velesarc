// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainLakeModifier.h"

#include "Hash/Blake3.h"

namespace ArcTerrainLakeModifier_Local
{
	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	/** Provider holding the field by REFCOUNT: GetSourceDescriptor runs per stamp on several per-cell
	 *  paths and the basin raster is megabytes, so a per-call copy is unaffordable. Sound because the
	 *  field is immutable and a regeneration replaces it wholesale rather than mutating it. */
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(TSharedRef<const FArcTerrainBakedField> Field)
	{
		return [Field](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			if (!Field->IsValid())
			{
				return false;
			}
			OutValues = Field->Values;
			OutDims = Field->Dims;
			return true;
		};
	}

	/** Geometry the basin and the composite's falloff mask share, in patch-local cm. */
	struct FBasinShape
	{
		bool bCircle = false;
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Half = FVector2D::ZeroVector;
		double CircleRadius = 0.0;
	};

	/** Distance to the shape's edge, normalized so 1.0 IS the edge — the same measure
	 *  StampCircleMask/StampEdgeMask reach zero at, so the bowl and the mask cannot disagree. */
	double NormalizedShapeDistance(const FBasinShape& Shape, const FVector2D& PatchXY)
	{
		const FVector2D Local = PatchXY - Shape.Center;
		if (Shape.bCircle)
		{
			if (Shape.CircleRadius > 0.0)
			{
				return Local.Size() / Shape.CircleRadius;
			}
			return FVector2D(Local.X / Shape.Half.X, Local.Y / Shape.Half.Y).Size();
		}
		return FMath::Max(FMath::Abs(Local.X) / Shape.Half.X, FMath::Abs(Local.Y) / Shape.Half.Y);
	}

	/** The bowl, in the normalized measure NormalizedShapeDistance returns. RimBand is the width of the
	 *  rise from the waterline to the rim clearance; 0 means the waterline IS the shape edge and there
	 *  is no rim. */
	struct FBasinProfile
	{
		double WaterZ = 0.0;
		double Depth = 0.0;
		double Shore = 1.0;
		double Exponent = 1.0;
		double Rim = 0.0;
		double RimBand = 0.0;
	};

	/** Bowl inside the waterline, smoothstep rim outside it. Both branches equal the water surface at
	 *  the waterline, and the no-rim case is flat there, so the field is continuous for every profile
	 *  BuildBasinField can produce — a step would ring under the composite's bicubic reconstruction. */
	double BasinProfileZ(const FBasinProfile& Profile, double NormalizedDistance)
	{
		if (NormalizedDistance <= Profile.Shore)
		{
			const double Radial = NormalizedDistance / Profile.Shore;
			return Profile.WaterZ
				- Profile.Depth * FMath::Pow(FMath::Max(1.0 - Radial * Radial, 0.0), Profile.Exponent);
		}

		if (Profile.RimBand <= 0.0)
		{
			return Profile.WaterZ;
		}

		const double RimParam = FMath::Clamp((NormalizedDistance - Profile.Shore) / Profile.RimBand, 0.0, 1.0);
		return Profile.WaterZ + Profile.Rim * RimParam * RimParam * (3.0 - 2.0 * RimParam);
	}

	/** Deterministic content identity: the raster is a pure function of these parameters, so an
	 *  unchanged knob set MUST yield an unchanged id. The id is folded into the cell collision key that
	 *  also keys the baked sections, and the field is no UPROPERTY — a Blueprint actor move drops it
	 *  through the component instance-data cache and OnRegister rebuilds it, which under a fresh GUID
	 *  would re-invalidate the whole footprint on every move.
	 *  BUMP FormatVersion whenever BasinProfileZ or the raster layout changes: the parameters alone
	 *  would otherwise name two different rasters. */
	FGuid MakeContentId(const FBasinShape& Shape, const FBasinProfile& Profile, int32 Resolution,
		const FVector2D& Coverage)
	{
		static const FGuid FormatVersion(0x7B1C4E20, 0x4F3A4D91, 0x9C2E5A08, 0x1D6B7F34);

		FBlake3 Hash;
		HashPod(Hash, FormatVersion.A);
		HashPod(Hash, FormatVersion.B);
		HashPod(Hash, FormatVersion.C);
		HashPod(Hash, FormatVersion.D);
		HashPod(Hash, Resolution);
		HashPod(Hash, Coverage.X);
		HashPod(Hash, Coverage.Y);
		HashPod(Hash, static_cast<uint8>(Shape.bCircle ? 1 : 0));
		HashPod(Hash, Shape.Center.X);
		HashPod(Hash, Shape.Center.Y);
		HashPod(Hash, Shape.Half.X);
		HashPod(Hash, Shape.Half.Y);
		HashPod(Hash, Shape.CircleRadius);
		HashPod(Hash, Profile.WaterZ);
		HashPod(Hash, Profile.Depth);
		HashPod(Hash, Profile.Shore);
		HashPod(Hash, Profile.Exponent);
		HashPod(Hash, Profile.Rim);
		HashPod(Hash, Profile.RimBand);

		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

#if WITH_EDITOR
	bool IsShapeProperty(FName PropertyName, FName MemberName)
	{
		static const FName ShapeNames[] = {
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, WaterSurfaceZ),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, BasinDepth),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, ShorelineExtent),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, BasinProfileExponent),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, RimClearance),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, CarveResolution),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, bCircleMask),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, CircleCenterOffset),
			GET_MEMBER_NAME_CHECKED(UArcTerrainLakeModifier, CircleRadius),
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, CoverageXY)
		};

		for (const FName& ShapeName : ShapeNames)
		{
			if (PropertyName == ShapeName || MemberName == ShapeName)
			{
				return true;
			}
		}
		return false;
	}
#endif // WITH_EDITOR
}

UArcTerrainLakeModifier::UArcTerrainLakeModifier()
{
	// The basin deepens the surface, never stacks on it: the composite reads BlendMode off the
	// component (FArcStampSourceDesc carries no blend mode).
	BlendMode = EArcTerrainBlendMode::Min;
	CoverageXY = FVector2D(100000.0, 100000.0);
}

void UArcTerrainLakeModifier::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	uint8 bHasBasinField = BasinField.IsValid() ? 1 : 0;
	Ar << bHasBasinField;

	if (Ar.IsLoading())
	{
		BasinField.Reset();
		if (bHasBasinField != 0)
		{
			TSharedRef<FArcTerrainBakedField> Loaded = MakeShared<FArcTerrainBakedField>();
			FArcTerrainBakedField::StaticStruct()->SerializeItem(Ar, &Loaded.Get(), nullptr);
			BasinField = Loaded;
		}
	}
	else if (bHasBasinField != 0)
	{
		// Saving reads the struct; the const_cast avoids duplicating the whole raster to serialize it.
		FArcTerrainBakedField* Field = const_cast<FArcTerrainBakedField*>(BasinField.Get());
		FArcTerrainBakedField::StaticStruct()->SerializeItem(Ar, Field, nullptr);
	}
}

void UArcTerrainLakeModifier::OnRegister()
{
	// A freshly placed component has no serialized field; build it before the base registers the stamp,
	// so the first composite already sees the basin.
	if (!BasinField.IsValid() || !BasinField->IsValid())
	{
		BuildBasinField();
	}

	Super::OnRegister();
}

void UArcTerrainLakeModifier::RegenerateBasin()
{
	Modify();
	BuildBasinField();
	NotifyStampChanged();
}

void UArcTerrainLakeModifier::BuildBasinField()
{
	using namespace ArcTerrainLakeModifier_Local;

	const FVector2D Half = CoverageXY * 0.5;
	if (Half.X <= 0.0 || Half.Y <= 0.0)
	{
		BasinField.Reset();
		return;
	}

	FBasinShape Shape;
	Shape.bCircle = bCircleMask;
	Shape.Center = CircleCenterOffset;
	Shape.Half = Half;
	Shape.CircleRadius = static_cast<double>(CircleRadius);

	const int32 Resolution = FMath::Clamp(CarveResolution, 64, 2048);
	// Texel (0,0) is the footprint's min corner and texel (Resolution-1, *) its max corner: the
	// composite maps the source's texel CENTRES onto the footprint edges (ComputeSampleMap).
	const double SpanX = CoverageXY.X / static_cast<double>(Resolution - 1);
	const double SpanY = CoverageXY.Y / static_cast<double>(Resolution - 1);

	FBasinProfile Profile;
	Profile.WaterZ = static_cast<double>(WaterSurfaceZ);
	Profile.Depth = FMath::Max(static_cast<double>(BasinDepth), 0.0);
	Profile.Exponent = FMath::Max(static_cast<double>(BasinProfileExponent), 0.01);
	Profile.Rim = FMath::Max(static_cast<double>(RimClearance), 0.0);

	const double AuthoredShore = FMath::Clamp(static_cast<double>(ShorelineExtent), 0.05, 1.0);
	if (AuthoredShore >= 1.0)
	{
		// The waterline is the shape edge: there is no room for a rim, and none is needed — the mask
		// is already zero outside the shape.
		Profile.Shore = AuthoredShore;
		Profile.RimBand = 0.0;
	}
	else
	{
		// Texels one normalized unit of the shape spans, along its tightest axis.
		const double TexelsPerUnit = (Shape.bCircle && Shape.CircleRadius > 0.0)
			? Shape.CircleRadius / FMath::Max(SpanX, SpanY)
			: 0.5 * static_cast<double>(Resolution - 1);
		// A rim thinner than this rasterises as a step, whatever its height, and the height path's
		// bicubic reconstruction rings on it at the waterline.
		const double MinRimBand = TexelsPerUnit > 0.0 ? 4.0 / TexelsPerUnit : 0.0;
		Profile.Shore = FMath::Clamp(FMath::Min(AuthoredShore, 1.0 - MinRimBand), 0.05, 1.0);
		Profile.RimBand = 1.0 - Profile.Shore;
	}

	TArray<float> Values;
	Values.SetNumUninitialized(Resolution * Resolution);
	for (int32 TexelY = 0; TexelY < Resolution; ++TexelY)
	{
		const double PatchY = -Half.Y + static_cast<double>(TexelY) * SpanY;
		for (int32 TexelX = 0; TexelX < Resolution; ++TexelX)
		{
			const FVector2D PatchXY(-Half.X + static_cast<double>(TexelX) * SpanX, PatchY);
			const double NormalizedDistance = NormalizedShapeDistance(Shape, PatchXY);
			Values[TexelY * Resolution + TexelX] =
				static_cast<float>(BasinProfileZ(Profile, NormalizedDistance));
		}
	}

	TSharedRef<FArcTerrainBakedField> Field = MakeShared<FArcTerrainBakedField>();
	Field->Values = MoveTemp(Values);
	Field->Dims = FIntPoint(Resolution, Resolution);
	Field->ContentId = MakeContentId(Shape, Profile, Resolution, CoverageXY);
	BasinField = Field;
}

bool UArcTerrainLakeModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	if (!BasinField.IsValid() || !BasinField->IsValid())
	{
		return false;
	}

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = BasinField->ContentId;
	OutDesc.SourceDims = BasinField->Dims;
	OutDesc.BakedProvider = ArcTerrainLakeModifier_Local::MakeProvider(BasinField.ToSharedRef());
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	// The field stores stamp-local cm, so the composite decodes the texel unchanged.
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;

	OutDesc.EdgeFalloffBand = EdgeFalloffBand;
	OutDesc.EdgeSharpness = EdgeSharpness;
	OutDesc.bCircleMask = bCircleMask;
	OutDesc.CircleCenterOffset = CircleCenterOffset;
	OutDesc.CircleRadius = CircleRadius;
	OutDesc.CircleFalloffBand = CircleFalloffBand;
	return true;
}

#if WITH_EDITOR
void UArcTerrainLakeModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Interactive slider drags would rebuild the whole raster on every mouse delta; the release event
	// carries the final value.
	if (PropertyChangedEvent.Property != nullptr
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& ArcTerrainLakeModifier_Local::IsShapeProperty(
			PropertyChangedEvent.GetPropertyName(), PropertyChangedEvent.GetMemberPropertyName()))
	{
		// Modify() before the swap so the transaction snapshots the previous field.
		Modify();
		BuildBasinField();
	}

	// Super routes the recomposite, so the new field must already be published.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR
