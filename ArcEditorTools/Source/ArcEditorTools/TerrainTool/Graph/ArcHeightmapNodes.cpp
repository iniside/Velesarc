// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/ArcHeightmapNodes.h"

#include "ProceduralNoise.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcHeightmapNodes, Log, All);

namespace ArcHeightmapNodes_Internal
{
	using namespace UE::Geometry;

	/** Map the authorable FBM mode enum onto the engine enum. */
	EFBMMode ToEngineFBMMode(EArcHeightmapFBMMode Mode)
	{
		switch (Mode)
		{
		case EArcHeightmapFBMMode::Turbulent:
			return EFBMMode::Turbulent;
		case EArcHeightmapFBMMode::Ridge:
			return EFBMMode::Ridge;
		case EArcHeightmapFBMMode::Standard:
		default:
			return EFBMMode::Standard;
		}
	}

	/** World XY -> noise sampling coordinate: rotate, scale by frequency, translate. */
	FVector2d NoiseCoord(const FVector2d& WorldXY, double RotationDeg, const FVector2D& Frequency, const FVector2D& Translate)
	{
		const double Rad = FMath::DegreesToRadians(RotationDeg);
		const double C = FMath::Cos(Rad);
		const double S = FMath::Sin(Rad);
		const FVector2d Rotated(C * WorldXY.X - S * WorldXY.Y, S * WorldXY.X + C * WorldXY.Y);
		return FVector2d(Translate.X, Translate.Y)
			+ FVector2d(Rotated.X * Frequency.X, Rotated.Y * Frequency.Y);
	}

	/** Normalized, redistributed hill height in [0,1] at a noise coordinate. */
	double EvalHeight(const FVector2d& ST, EFBMMode FBMMode, int32 Octaves, double Lacunarity, double Gain,
		double Smoothness, double Gamma, double RedistributionExponent, double RedistributionFudge)
	{
		const int32 ClampedOctaves = FMath::Max(1, Octaves);
		const double N = FractalBrownianMotionNoise<double>(
			FBMMode, static_cast<uint32>(ClampedOctaves), ST,
			Lacunarity, Gain, Smoothness, Gamma);

		const double AmpSum = FMath::IsNearlyEqual(Gain, 1.0)
			? static_cast<double>(ClampedOctaves)
			: (1.0 - FMath::Pow(Gain, ClampedOctaves)) / (1.0 - Gain);

		const double H = FMath::Clamp(N / AmpSum * 0.5 + 0.5, 0.0, 1.0);
		return FMath::Pow(FMath::Clamp(H * RedistributionFudge, 0.0, 1.0), RedistributionExponent);
	}

	/** Allocate a zero field from Ctx and return whether the input pointer is usable. */
	bool ResolveInput(const TArray<const FSampledScalarField2f*>& Inputs, int32 Index, const FSampledScalarField2f*& OutPtr)
	{
		OutPtr = (Inputs.IsValidIndex(Index)) ? Inputs[Index] : nullptr;
		return OutPtr != nullptr;
	}

	/**
	 * Shared erosion run-helper: sets grid dimensions from Ctx, copies the input field as InitialHeight,
	 * calls SimulateCached, and writes the result to Out. Params.Mode and all simulation parameters must
	 * be set by the caller before invoking this function.
	 */
	void RunErosionField(ArcTerrainErosion::FParams& Params,
		const TArray<const FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		const FString& NodeLabel,
		FSampledScalarField2f& Out)
	{
		const FSampledScalarField2f* InputField = nullptr;
		if (!ResolveInput(Inputs, 0, InputField))
		{
			UE_LOG(LogArcHeightmapNodes, Warning,
				TEXT("Erosion (%s): missing input; emitting zero field."), *NodeLabel);
			Ctx.AllocateField(Out);
			return;
		}

		Params.GridResX = FMath::Max(2, Ctx.ResX);
		Params.GridResY = FMath::Max(2, Ctx.ResY);
		Params.CoverageX = Ctx.CoverageX;
		Params.CoverageY = Ctx.CoverageY;
		Params.SkirtSize = 0.0;

		// DIAGNOSTIC: this runs synchronously on the game thread at mega-mesh build / level load. Cost is
		// NumCells (ResX*ResY) * OuterIterations; a hang here is almost always a resolution explosion at build
		// time vs. the editor preview. Log the actual driving inputs BEFORE the (potentially minutes-long) call.
		UE_LOG(LogArcHeightmapNodes, Display,
			TEXT("Erosion (%s): ENTER SimulateCached — %dx%d (%d cells), OuterIterations=%d, ThermalSubsteps=%d, ")
			TEXT("Coverage=(%.0f x %.0f), Mode=%d."),
			*NodeLabel, Params.GridResX, Params.GridResY, Params.GridResX * Params.GridResY,
			FMath::Max(0, Params.OuterIterations), FMath::Max(0, Params.ThermalSubsteps),
			Ctx.CoverageX, Ctx.CoverageY, static_cast<int32>(Params.Mode));

		// Deep copy so the sim owns the input height grid for its duration. Sanitize on this mutable
		// copy BEFORE handing it to Params (whose InitialHeight is const): non-finite input would
		// propagate NaN/Inf through the whole sim (and historically hung the flow-accumulation sort).
		// Replace non-finite cells with 0 and warn loudly so the upstream node is the suspect, not
		// erosion silently emitting a garbage mesh.
		TSharedRef<FSampledScalarField2f> SanitizedInput = MakeShared<FSampledScalarField2f>(*InputField);
		{
			TArray64<float>& InitVals = SanitizedInput->GridValues.GridValues();
			int32 NonFiniteCount = 0;
			for (int64 Idx = 0; Idx < InitVals.Num(); ++Idx)
			{
				if (!FMath::IsFinite(InitVals[Idx]))
				{
					InitVals[Idx] = 0.0f;
					++NonFiniteCount;
				}
			}
			if (NonFiniteCount > 0)
			{
				UE_LOG(LogArcHeightmapNodes, Warning,
					TEXT("Erosion (%s): %d non-finite (NaN/Inf) input cell(s) sanitized to 0 — check the ")
					TEXT("upstream node feeding this erosion input."), *NodeLabel, NonFiniteCount);
			}
		}
		Params.InitialHeight = SanitizedInput;

		const TSharedPtr<const ArcTerrainErosion::FResult> Result = ArcTerrainErosion::SimulateCached(Params);
		UE_LOG(LogArcHeightmapNodes, Display,
			TEXT("Erosion (%s): EXIT SimulateCached (valid=%d)."), *NodeLabel, Result.IsValid() ? 1 : 0);
		if (!Result.IsValid())
		{
			UE_LOG(LogArcHeightmapNodes, Warning,
				TEXT("Erosion (%s): SimulateCached returned null; emitting zero field."), *NodeLabel);
			Ctx.AllocateField(Out);
			return;
		}

		Out = Result->Height;
	}

	/**
	 * Hand-written 2D Voronoi (cellular) noise at a noise coordinate.
	 *
	 * Decomposes Coord into an integer cell + fractional offset, searches the 3x3 cell neighbourhood for
	 * hash-jittered feature points, and tracks the two nearest distances (F1 closest, F2 second) plus the
	 * hash of the F1 cell. Returns the requested output channel.
	 *
	 * Output ranges (Jitter <= 0.5, so feature points stay within +-0.5 of their cell centre):
	 *  - F1:        ~[0, ~1.5] (closest feature distance; metric-dependent magnitude)
	 *  - F2:        >= F1
	 *  - F2MinusF1: >= 0 (ridge/edge field, 0 on cell boundaries)
	 *  - CellValue: [0, 1] (per-cell constant from the F1 cell hash)
	 */
	double Voronoi2D(const FVector2d& Coord, double Jitter, int32 Seed,
		EArcHeightmapDistanceMetric Metric, EArcHeightmapVoronoiOutput Output)
	{
		const int32 nx = FMath::FloorToInt(Coord.X);
		const double fx = Coord.X - static_cast<double>(nx);
		const int32 ny = FMath::FloorToInt(Coord.Y);
		const double fy = Coord.Y - static_cast<double>(ny);

		double F1 = TNumericLimits<double>::Max();
		double F2 = TNumericLimits<double>::Max();
		uint32 ClosestHash = 0u;

		for (int32 j = -1; j <= 1; ++j)
		{
			for (int32 i = -1; i <= 1; ++i)
			{
				const int32 cx = nx + i;
				const int32 cy = ny + j;

				uint32 h = static_cast<uint32>(Seed)
					^ (static_cast<uint32>(cx) * 501125321u)
					^ (static_cast<uint32>(cy) * 1136930381u);
				h *= 0x27d4eb2du;

				const double jx = (static_cast<double>(h & 0xFFFFu) / 65536.0 - 0.5) * 2.0 * Jitter;
				const double jy = (static_cast<double>((h >> 16) & 0xFFFFu) / 65536.0 - 0.5) * 2.0 * Jitter;

				const double rx = static_cast<double>(i) - fx + jx;
				const double ry = static_cast<double>(j) - fy + jy;

				double Dist;
				switch (Metric)
				{
				case EArcHeightmapDistanceMetric::Manhattan:
					Dist = FMath::Abs(rx) + FMath::Abs(ry);
					break;
				case EArcHeightmapDistanceMetric::Chebyshev:
					Dist = FMath::Max(FMath::Abs(rx), FMath::Abs(ry));
					break;
				case EArcHeightmapDistanceMetric::Hybrid:
					Dist = (FMath::Abs(rx) + FMath::Abs(ry)) + (rx * rx + ry * ry);
					break;
				case EArcHeightmapDistanceMetric::Euclidean:
				default:
					// Squared; sqrt deferred until after the search so ordering is preserved.
					Dist = rx * rx + ry * ry;
					break;
				}

				if (Dist < F1)
				{
					F2 = F1;
					F1 = Dist;
					ClosestHash = h;
				}
				else if (Dist < F2)
				{
					F2 = Dist;
				}
			}
		}

		if (Metric == EArcHeightmapDistanceMetric::Euclidean)
		{
			F1 = FMath::Sqrt(F1);
			F2 = FMath::Sqrt(F2);
		}

		switch (Output)
		{
		case EArcHeightmapVoronoiOutput::F2:
			return F2;
		case EArcHeightmapVoronoiOutput::F2MinusF1:
			return F2 - F1;
		case EArcHeightmapVoronoiOutput::CellValue:
			return static_cast<double>(static_cast<int32>(ClosestHash)) * (1.0 / 2147483648.0) * 0.5 + 0.5;
		case EArcHeightmapVoronoiOutput::F1:
		default:
			return F1;
		}
	}

	/**
	 * Build the doubled 512-entry permutation table for simplex hashing.
	 *
	 * Seeds src[0..255]=i, runs a Fisher-Yates shuffle driven by a Knuth LCG, then mirrors the
	 * 256 entries into [256,512) so Simplex2D can index Perm[x + Perm[y]] without wrapping.
	 */
	void BuildSimplexPerm(int32 Seed, uint8 Perm[512])
	{
		uint8 src[256];
		for (int32 i = 0; i < 256; ++i)
		{
			src[i] = static_cast<uint8>(i);
		}

		uint32 rng = static_cast<uint32>(Seed);
		for (int32 i = 255; i > 0; --i)
		{
			rng = rng * 1664525u + 1013904223u;
			const int32 k = static_cast<int32>((rng >> 16) % static_cast<uint32>(i + 1));
			Swap(src[i], src[k]);
		}

		for (int32 i = 0; i < 256; ++i)
		{
			Perm[i] = Perm[i + 256] = src[i];
		}
	}

	/**
	 * Classic Gustavson 2D simplex noise at P using a 512-entry permutation table.
	 *
	 * Skews the input to the simplex grid, unskews the cell origin, accumulates the three corner
	 * contributions (radial falloff x gradient dot), and scales by 70 to land in ~[-1, 1].
	 */
	double Simplex2D(const FVector2d& P, const uint8 Perm[512])
	{
		const double F2 = 0.5 * (FMath::Sqrt(3.0) - 1.0);
		const double G2 = (3.0 - FMath::Sqrt(3.0)) / 6.0;

		const double s = (P.X + P.Y) * F2;
		const int32 i = FMath::FloorToInt(P.X + s);
		const int32 j = FMath::FloorToInt(P.Y + s);

		const double t = static_cast<double>(i + j) * G2;
		const double X0 = static_cast<double>(i) - t;
		const double Y0 = static_cast<double>(j) - t;
		const double x0 = P.X - X0;
		const double y0 = P.Y - Y0;

		int32 i1, j1;
		if (x0 > y0)
		{
			i1 = 1;
			j1 = 0;
		}
		else
		{
			i1 = 0;
			j1 = 1;
		}

		const double x1 = x0 - static_cast<double>(i1) + G2;
		const double y1 = y0 - static_cast<double>(j1) + G2;
		const double x2 = x0 - 1.0 + 2.0 * G2;
		const double y2 = y0 - 1.0 + 2.0 * G2;

		static const double Grad[8][2] = {
			{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

		const int32 ii = i & 255;
		const int32 jj = j & 255;
		const int32 gi0 = Perm[ii + Perm[jj]] & 7;
		const int32 gi1 = Perm[ii + i1 + Perm[jj + j1]] & 7;
		const int32 gi2 = Perm[ii + 1 + Perm[jj + 1]] & 7;

		double n0 = 0.0;
		double t0 = 0.5 - x0 * x0 - y0 * y0;
		if (t0 > 0.0)
		{
			t0 *= t0;
			n0 = t0 * t0 * (Grad[gi0][0] * x0 + Grad[gi0][1] * y0);
		}

		double n1 = 0.0;
		double t1 = 0.5 - x1 * x1 - y1 * y1;
		if (t1 > 0.0)
		{
			t1 *= t1;
			n1 = t1 * t1 * (Grad[gi1][0] * x1 + Grad[gi1][1] * y1);
		}

		double n2 = 0.0;
		double t2 = 0.5 - x2 * x2 - y2 * y2;
		if (t2 > 0.0)
		{
			t2 *= t2;
			n2 = t2 * t2 * (Grad[gi2][0] * x2 + Grad[gi2][1] * y2);
		}

		return 70.0 * (n0 + n1 + n2);
	}

	/** Small fBM over FMath::PerlinNoise2D (lacunarity 2, gain 0.5); returns a ~[-1,1]-ish accumulation. */
	double PerlinFBM2D(const FVector2d& P, int32 Octaves)
	{
		const int32 ClampedOctaves = FMath::Max(1, Octaves);
		double Amp = 1.0;
		double Sum = 0.0;
		FVector2d S = P;
		for (int32 Oct = 0; Oct < ClampedOctaves; ++Oct)
		{
			Sum += Amp * static_cast<double>(FMath::PerlinNoise2D(FVector2D(S.X, S.Y)));
			S *= 2.0;        // lacunarity
			Amp *= 0.5;      // gain
		}
		return Sum;          // ~[-1,1]-ish accumulation
	}
}

void FArcHeightmapNode_Noise::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	Ctx.AllocateField(Out);

	const UE::Geometry::EFBMMode FBMMode = ToEngineFBMMode(Mode);
	const int32 W = Out.Width();
	const int32 H = Out.Height();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			const FVector2d ST = NoiseCoord(FVector2d(LocalX, LocalY), Rotation, Frequency, Translate);
			const double Hgt = EvalHeight(ST, FBMMode, Octaves, Lacunarity, Gain, Smoothness, Gamma,
				RedistributionExponent, RedistributionFudge);

			Out.GridValues.At(X, Y) = static_cast<float>(Hgt * Amplitude);
		}
	}
}

void FArcHeightmapNode_NoiseBase::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	Ctx.AllocateField(Out);

	const UE::Geometry::EFBMMode FBMMode = ToEngineFBMMode(GetFBMMode());
	const int32 W = Out.Width();
	const int32 H = Out.Height();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			const FVector2d ST = NoiseCoord(FVector2d(LocalX, LocalY), Rotation, Frequency, Translate);
			const double Hgt = EvalHeight(ST, FBMMode, Octaves, Lacunarity, Gain, Smoothness, Gamma,
				RedistributionExponent, RedistributionFudge);

			Out.GridValues.At(X, Y) = static_cast<float>(Hgt * Amplitude);
		}
	}
}

void FArcHeightmapNode_Simplex::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	Ctx.AllocateField(Out);

	// Build the permutation table once; the per-cell FBM loop below is self-contained (FBM over
	// simplex, Standard mode) and replicates EvalHeight's normalization + redistribution exactly,
	// so the engine FractalBrownianMotionNoise (Perlin) path is untouched.
	uint8 Perm[512];
	BuildSimplexPerm(Seed, Perm);

	const int32 W = Out.Width();
	const int32 H = Out.Height();

	const int32 ClampedOctaves = FMath::Max(1, Octaves);
	const double AmpSum = FMath::IsNearlyEqual(Gain, 1.0)
		? static_cast<double>(ClampedOctaves)
		: (1.0 - FMath::Pow(Gain, ClampedOctaves)) / (1.0 - Gain);

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			const FVector2d ST = NoiseCoord(FVector2d(LocalX, LocalY), Rotation, Frequency, Translate);

			double Amp = 1.0;
			double N = 0.0;
			FVector2d Sample = ST;
			for (int32 Oct = 0; Oct < ClampedOctaves; ++Oct)
			{
				N += Amp * Simplex2D(Sample, Perm);
				Sample *= Lacunarity;
				Amp *= Gain;
			}

			const double Hnorm = FMath::Clamp(N / AmpSum * 0.5 + 0.5, 0.0, 1.0);
			const double Hgt = FMath::Pow(FMath::Clamp(Hnorm * RedistributionFudge, 0.0, 1.0), RedistributionExponent);

			Out.GridValues.At(X, Y) = static_cast<float>(Hgt * Amplitude);
		}
	}
}

void FArcHeightmapNode_Voronoi::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	Ctx.AllocateField(Out);

	const int32 W = Out.Width();
	const int32 H = Out.Height();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			const FVector2d ST = NoiseCoord(FVector2d(LocalX, LocalY), Rotation, Frequency, Translate);
			const double V = Voronoi2D(ST, JitterAmount, Seed, Metric, Output);

			Out.GridValues.At(X, Y) = static_cast<float>(V * Amplitude);
		}
	}
}

void FArcHeightmapNode_Erosion::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	ArcTerrainErosion::FParams Params;
	Params.Mode = (Mode == EArcHeightmapErosionMode::Droplet)
		? ArcTerrainErosion::EArcErosionMode::Droplet
		: ArcTerrainErosion::EArcErosionMode::GridStreamPower;

	Params.OuterIterations = OuterIterations;
	Params.ThermalSubsteps = ThermalSubsteps;
	Params.RainAmount = RainAmount;
	Params.DepositRate = DepositRate;
	Params.SedimentCapacity = SedimentCapacity;
	Params.TalusAngleDeg = TalusAngleDeg;
	Params.ThermalRate = ThermalRate;
	Params.Seed = Seed;
	Params.SedimentSmoothingRadiusCm = SedimentSmoothingRadiusCm;

	Params.StreamPowerK = StreamPowerK;
	Params.StreamPowerM = StreamPowerM;
	Params.StreamPowerN = StreamPowerN;
	Params.MaxErodePerStepCm = MaxErodePerStepCm;
	Params.StreamPowerDepositRate = StreamPowerDepositRate;
	Params.ChannelThreshold = ChannelThreshold;

	Params.NumDroplets = NumDroplets;
	Params.DropletLifetime = DropletLifetime;
	Params.DropletInertia = DropletInertia;
	Params.DropletCapacity = DropletCapacity;
	Params.DropletErodeRate = DropletErodeRate;
	Params.DropletDepositRate = DropletDepositRate;
	Params.DropletEvaporation = DropletEvaporation;
	Params.DropletGravity = DropletGravity;
	Params.DropletErosionRadius = DropletErosionRadius;
	Params.DropletMinSlope = DropletMinSlope;

	Params.ChannelDepthFraction = ChannelDepthFraction;
	Params.FeatureSizeCm = FeatureSizeCm;

	RunErosionField(Params, Inputs, Ctx, NodeId.ToString(), Out);
}

void FArcHeightmapNode_ErosionFlow::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	ArcTerrainErosion::FParams Params;
	Params.Mode = ArcTerrainErosion::EArcErosionMode::GridStreamPower;
	Params.OuterIterations = OuterIterations;
	Params.ChannelThreshold = ChannelThreshold;
	Params.ChannelDepthFraction = ChannelDepthFraction;
	Params.StreamPowerM = StreamPowerM;
	Params.RainAmount = RainAmount;

	RunErosionField(Params, Inputs, Ctx, NodeId.ToString(), Out);
}

void FArcHeightmapNode_Math::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* FieldA = nullptr;
	const UE::Geometry::FSampledScalarField2f* FieldB = nullptr;
	if (!ResolveInput(Inputs, 0, FieldA))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Math (%s): missing input A; treating as zero."), *NodeId.ToString());
	}
	if (!ResolveInput(Inputs, 1, FieldB))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Math (%s): missing input B; treating as zero."), *NodeId.ToString());
	}

	Ctx.AllocateField(Out);

	const int64 Count = Out.GridValues.GridValues().Num();
	const TArray64<float>* ValsA = (FieldA != nullptr) ? &FieldA->GridValues.GridValues() : nullptr;
	const TArray64<float>* ValsB = (FieldB != nullptr) ? &FieldB->GridValues.GridValues() : nullptr;
	TArray64<float>& OutVals = Out.GridValues.GridValues();

	for (int64 Idx = 0; Idx < Count; ++Idx)
	{
		const float A = (ValsA != nullptr && ValsA->IsValidIndex(Idx)) ? (*ValsA)[Idx] : 0.0f;
		const float B = (ValsB != nullptr && ValsB->IsValidIndex(Idx)) ? (*ValsB)[Idx] : 0.0f;

		float R = A;
		switch (Op)
		{
		case EArcHeightmapMathOp::Add:
			R = A + B;
			break;
		case EArcHeightmapMathOp::Subtract:
			R = A - B;
			break;
		case EArcHeightmapMathOp::Multiply:
			R = A * B;
			break;
		case EArcHeightmapMathOp::Divide:
			R = (B == 0.0f) ? A : (A / B);
			break;
		case EArcHeightmapMathOp::Min:
			R = FMath::Min(A, B);
			break;
		case EArcHeightmapMathOp::Max:
			R = FMath::Max(A, B);
			break;
		case EArcHeightmapMathOp::Power:
			R = FMath::Pow(FMath::Max(A, 0.0f), B);
			break;
		default:
			R = A;
			break;
		}

		OutVals[Idx] = FMath::Lerp(A, R, static_cast<float>(Strength));
	}
}

void FArcHeightmapNode_Constant::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	Ctx.AllocateField(Out);
	Out.GridValues.Assign(static_cast<float>(Value));
}

void FArcHeightmapNode_Output::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* InputField = nullptr;
	if (!ResolveInput(Inputs, 0, InputField))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Output (%s): missing input; emitting zero field."), *NodeId.ToString());
		Ctx.AllocateField(Out);
		return;
	}

	Out = *InputField;
}

void FArcHeightmapNode_Add::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* FieldA = nullptr;
	const UE::Geometry::FSampledScalarField2f* FieldB = nullptr;
	if (!ResolveInput(Inputs, 0, FieldA))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Add (%s): missing input A; treating as zero."), *NodeId.ToString());
	}
	if (!ResolveInput(Inputs, 1, FieldB))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Add (%s): missing input B; treating as zero."), *NodeId.ToString());
	}

	Ctx.AllocateField(Out);

	const int64 Count = Out.GridValues.GridValues().Num();
	const TArray64<float>* ValsA = (FieldA != nullptr) ? &FieldA->GridValues.GridValues() : nullptr;
	const TArray64<float>* ValsB = (FieldB != nullptr) ? &FieldB->GridValues.GridValues() : nullptr;
	TArray64<float>& OutVals = Out.GridValues.GridValues();

	for (int64 Idx = 0; Idx < Count; ++Idx)
	{
		const float A = (ValsA != nullptr && ValsA->IsValidIndex(Idx)) ? (*ValsA)[Idx] : 0.0f;
		const float B = (ValsB != nullptr && ValsB->IsValidIndex(Idx)) ? (*ValsB)[Idx] : 0.0f;
		OutVals[Idx] = A + B;
	}
}

void FArcHeightmapNode_Multiply::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* FieldA = nullptr;
	const UE::Geometry::FSampledScalarField2f* FieldB = nullptr;
	if (!ResolveInput(Inputs, 0, FieldA))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Multiply (%s): missing input A; treating as 1.0."), *NodeId.ToString());
	}
	if (!ResolveInput(Inputs, 1, FieldB))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Multiply (%s): missing input B; treating as 1.0."), *NodeId.ToString());
	}

	Ctx.AllocateField(Out);

	const int64 Count = Out.GridValues.GridValues().Num();
	const TArray64<float>* ValsA = (FieldA != nullptr) ? &FieldA->GridValues.GridValues() : nullptr;
	const TArray64<float>* ValsB = (FieldB != nullptr) ? &FieldB->GridValues.GridValues() : nullptr;
	TArray64<float>& OutVals = Out.GridValues.GridValues();

	for (int64 Idx = 0; Idx < Count; ++Idx)
	{
		const float A = (ValsA != nullptr && ValsA->IsValidIndex(Idx)) ? (*ValsA)[Idx] : 1.0f;
		const float B = (ValsB != nullptr && ValsB->IsValidIndex(Idx)) ? (*ValsB)[Idx] : 1.0f;
		OutVals[Idx] = A * B;
	}
}

void FArcHeightmapNode_Lerp::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* FieldA = nullptr;
	const UE::Geometry::FSampledScalarField2f* FieldB = nullptr;
	const UE::Geometry::FSampledScalarField2f* FieldAlpha = nullptr;
	if (!ResolveInput(Inputs, 0, FieldA))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Lerp (%s): missing input A; treating as zero."), *NodeId.ToString());
	}
	if (!ResolveInput(Inputs, 1, FieldB))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Lerp (%s): missing input B; treating as zero."), *NodeId.ToString());
	}
	ResolveInput(Inputs, 2, FieldAlpha);

	Ctx.AllocateField(Out);

	const int64 Count = Out.GridValues.GridValues().Num();
	const TArray64<float>* ValsA = (FieldA != nullptr) ? &FieldA->GridValues.GridValues() : nullptr;
	const TArray64<float>* ValsB = (FieldB != nullptr) ? &FieldB->GridValues.GridValues() : nullptr;
	const TArray64<float>* ValsAlpha = (FieldAlpha != nullptr) ? &FieldAlpha->GridValues.GridValues() : nullptr;
	TArray64<float>& OutVals = Out.GridValues.GridValues();
	const float ScalarAlpha = static_cast<float>(Alpha);

	for (int64 Idx = 0; Idx < Count; ++Idx)
	{
		const float A = (ValsA != nullptr && ValsA->IsValidIndex(Idx)) ? (*ValsA)[Idx] : 0.0f;
		const float B = (ValsB != nullptr && ValsB->IsValidIndex(Idx)) ? (*ValsB)[Idx] : 0.0f;
		const float a = (ValsAlpha != nullptr && ValsAlpha->IsValidIndex(Idx))
			? FMath::Clamp((*ValsAlpha)[Idx], 0.0f, 1.0f)
			: ScalarAlpha;
		OutVals[Idx] = FMath::Lerp(A, B, a);
	}
}

void FArcHeightmapNode_Levels::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* FieldA = nullptr;
	if (!ResolveInput(Inputs, 0, FieldA))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_Levels (%s): missing input; treating as zero."), *NodeId.ToString());
	}

	Ctx.AllocateField(Out);

	const int64 Count = Out.GridValues.GridValues().Num();
	const TArray64<float>* ValsA = (FieldA != nullptr) ? &FieldA->GridValues.GridValues() : nullptr;
	TArray64<float>& OutVals = Out.GridValues.GridValues();

	const float FInBlack = static_cast<float>(InBlack);
	const float FInRange = FMath::Max(static_cast<float>(InWhite - InBlack), static_cast<float>(KINDA_SMALL_NUMBER));
	const float FOutBlack = static_cast<float>(OutBlack);
	const float FOutWhite = static_cast<float>(OutWhite);
	const float FGammaInv = 1.0f / static_cast<float>(Gamma);

	for (int64 Idx = 0; Idx < Count; ++Idx)
	{
		const float A = (ValsA != nullptr && ValsA->IsValidIndex(Idx)) ? (*ValsA)[Idx] : 0.0f;
		float t = FMath::Clamp((A - FInBlack) / FInRange, 0.0f, 1.0f);
		t = FMath::Pow(t, FGammaInv);
		OutVals[Idx] = FMath::Lerp(FOutBlack, FOutWhite, t);
	}
}

void FArcHeightmapNode_Circle::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	Ctx.AllocateField(Out);

	const int32 W = Out.Width();
	const int32 H = Out.Height();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			const double Dist = (FVector2D(LocalX, LocalY) - Center).Size();

			double T;
			if (FalloffBand <= 0.0)
			{
				T = (Dist <= Radius) ? 1.0 : 0.0;
			}
			else
			{
				T = FMath::Clamp((Radius - Dist) / FalloffBand, 0.0, 1.0);
			}

			const double M = FMath::Pow(T, Sharpness);
			Out.GridValues.At(X, Y) = static_cast<float>(bInvert ? 1.0 - M : M);
		}
	}
}

// Model B — bilinear resample of the rasterized input; clamped sampling = edge repetition at the border.
void FArcHeightmapNode_DomainWarp::Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& Out) const
{
	using namespace ArcHeightmapNodes_Internal;

	const UE::Geometry::FSampledScalarField2f* InputField = nullptr;
	if (!ResolveInput(Inputs, 0, InputField))
	{
		UE_LOG(LogArcHeightmapNodes, Warning,
			TEXT("FArcHeightmapNode_DomainWarp (%s): missing input; emitting zero field."), *NodeId.ToString());
		Ctx.AllocateField(Out);
		return;
	}

	Ctx.AllocateField(Out);

	// Decorrelation offsets so the X and Y displacement channels sample different regions of the noise.
	const FVector2d OffA(static_cast<double>(Seed) * 0.13, static_cast<double>(Seed) * 0.71);
	const FVector2d OffB = OffA + FVector2d(5.2, 1.3);

	const int32 W = Out.Width();
	const int32 H = Out.Height();

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const double LocalX = Ctx.GridOrigin.X + static_cast<double>(X) * Ctx.CellDimensions.X;
			const double LocalY = Ctx.GridOrigin.Y + static_cast<double>(Y) * Ctx.CellDimensions.Y;

			FVector2d P(LocalX * WarpFrequency.X, LocalY * WarpFrequency.Y);   // displacement-noise space
			FVector2d D(0.0, 0.0);                                              // accumulated world displacement (cm)
			for (int32 It = 0; It < FMath::Max(1, Iterations); ++It)
			{
				const double qx = PerlinFBM2D(P + OffA, WarpOctaves);
				const double qy = PerlinFBM2D(P + OffB, WarpOctaves);
				D += FVector2d(qx, qy) * WarpAmplitude;        // cm
				P += FVector2d(qx, qy);                         // advance in noise space for recursion
			}

			const float Sampled = InputField->BilinearSampleClamped(
				FVector2f(static_cast<float>(LocalX + D.X), static_cast<float>(LocalY + D.Y)));
			Out.GridValues.At(X, Y) = Sampled;
		}
	}
}
