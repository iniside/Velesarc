// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The one CPU copy of the stamp falloff masks, shared by the composite evaluation and the data-map
 *  bake. The HLSL copy (Shaders/ArcTerrainComposite.usf) is hand-synced with this one. */
namespace ArcTerrainStampMask
{
	struct FMaskParams
	{
		float EdgeFalloffBand = 0.0f;
		float EdgeSharpness = 1.0f;
		float CornerRadius = 0.0f;
		bool bCircleMask = false;
		FVector2f CircleCenterOffset = FVector2f::ZeroVector;
		float CircleRadius = 0.0f;
		float CircleFalloffBand = 0.0f;
	};

	// Port of StampEdgeMask (ArcTerrainComposite.usf).
	inline float EdgeMask(const FMaskParams& Params, float PatchX, float PatchY, float HalfX, float HalfY)
	{
		const float R = FMath::Clamp(Params.CornerRadius, 0.0f, FMath::Min(HalfX, HalfY));
		const float QX = FMath::Abs(PatchX) - HalfX + R;
		const float QY = FMath::Abs(PatchY) - HalfY + R;
		const float MaxQX = FMath::Max(QX, 0.0f);
		const float MaxQY = FMath::Max(QY, 0.0f);
		const float Signed = FMath::Min(FMath::Max(QX, QY), 0.0f)
			+ FMath::Sqrt(MaxQX * MaxQX + MaxQY * MaxQY) - R;
		const float DistToEdge = -Signed;
		if (Params.EdgeFalloffBand <= 0.0f)
		{
			return (DistToEdge >= 0.0f) ? 1.0f : 0.0f;
		}
		const float T = FMath::Clamp(DistToEdge / Params.EdgeFalloffBand, 0.0f, 1.0f);
		// Smoothstep is COMPOSED over the pow ramp, not substituted for it: zero tangent at the inner
		// edge for every EdgeSharpness (pow alone has slope EdgeSharpness at T == 1), while
		// EdgeSharpness still shapes the ramp. The outer edge lands flat only above 0.5.
		const float U = FMath::Pow(T, FMath::Max(Params.EdgeSharpness, 1.0e-4f));
		return U * U * (3.0f - 2.0f * U); // HLSL smoothstep(0, 1, U)
	}

	// Port of StampCircleMask (ArcTerrainComposite.usf).
	inline float CircleMask(const FMaskParams& Params, float PatchX, float PatchY, float HalfX, float HalfY)
	{
		const float LocalX = PatchX - Params.CircleCenterOffset.X;
		const float LocalY = PatchY - Params.CircleCenterOffset.Y;
		const float LocalLength = FMath::Sqrt(LocalX * LocalX + LocalY * LocalY);

		float DistToEdge = 0.0f;
		if (Params.CircleRadius > 0.0f)
		{
			DistToEdge = Params.CircleRadius - LocalLength;
		}
		else
		{
			const float RadiusX = FMath::Max(HalfX, 1.0e-4f);
			const float RadiusY = FMath::Max(HalfY, 1.0e-4f);
			const float NormX = LocalX / RadiusX;
			const float NormY = LocalY / RadiusY;
			const float NormDist = FMath::Sqrt(NormX * NormX + NormY * NormY);
			if (NormDist <= 1.0e-4f)
			{
				return 1.0f;
			}
			DistToEdge = (LocalLength / NormDist) * (1.0f - NormDist);
		}

		if (Params.CircleFalloffBand <= 0.0f)
		{
			return (DistToEdge >= 0.0f) ? 1.0f : 0.0f;
		}
		const float T = FMath::Clamp(DistToEdge / Params.CircleFalloffBand, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T); // HLSL smoothstep(0, 1, T)
	}

	/** EdgeMask, then CircleMask when Params.bCircleMask and the edge term is still positive — the
	 *  multiplication order ComputeStamp uses (ArcTerrainComposite.usf:201-205). */
	inline float Evaluate(const FMaskParams& Params, float PatchX, float PatchY, float HalfX, float HalfY)
	{
		float Mask = EdgeMask(Params, PatchX, PatchY, HalfX, HalfY);
		if (Params.bCircleMask && Mask > 0.0f)
		{
			Mask *= CircleMask(Params, PatchX, PatchY, HalfX, HalfY);
		}
		return Mask;
	}
}
