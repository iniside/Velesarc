// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/ArcTerrainErosion.h"

#include "ProceduralNoise.h"
#include "Image/ImageBlur.h"
#include "Math/UnrealMathUtility.h"
#include "Async/ParallelFor.h"
#include "Hash/Blake3.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "DerivedDataCacheInterface.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTerrainErosion, Log, All);

namespace ArcTerrainErosionInternal
{
	using UE::Geometry::FDenseGrid2f;
	using UE::Geometry::FSampledScalarField2f;

	/** World/local XY -> noise sampling coordinate: rotate, scale by frequency, translate. */
	FVector2d NoiseCoord(const FVector2d& LocalXY, const ArcTerrainErosion::FParams& P)
	{
		const double Rad = FMath::DegreesToRadians(P.NoiseRotation);
		const double C = FMath::Cos(Rad);
		const double S = FMath::Sin(Rad);
		const FVector2d Rotated(C * LocalXY.X - S * LocalXY.Y, S * LocalXY.X + C * LocalXY.Y);
		return FVector2d(P.NoiseTranslate.X, P.NoiseTranslate.Y)
			+ FVector2d(Rotated.X * P.NoiseFrequency.X, Rotated.Y * P.NoiseFrequency.Y);
	}

	/** Normalized, redistributed hill height in [0,1] at a noise coordinate. */
	double EvalHeight(const FVector2d& ST, const ArcTerrainErosion::FParams& P)
	{
		const int ClampedOctaves = FMath::Max(1, P.Octaves);
		const double N = UE::Geometry::FractalBrownianMotionNoise<double>(
			UE::Geometry::EFBMMode::Standard, static_cast<uint32>(ClampedOctaves), ST,
			P.Lacunarity, P.Gain, P.Smoothness, P.Gamma);

		const double AmpSum = FMath::IsNearlyEqual(P.Gain, 1.0)
			? static_cast<double>(ClampedOctaves)
			: (1.0 - FMath::Pow(P.Gain, ClampedOctaves)) / (1.0 - P.Gain);

		const double H = FMath::Clamp(N / AmpSum * 0.5 + 0.5, 0.0, 1.0);
		return FMath::Pow(FMath::Clamp(H * P.RedistributionFudge, 0.0, 1.0), P.RedistributionExponent);
	}

	/** Pure integer hash -> deterministic [0,1) from cell coords + seed. No RNG state. */
	float HashUnitFloat(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = static_cast<uint32>(Seed) * 747796405u + 2891336453u;
		H ^= static_cast<uint32>(X) * 374761393u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= static_cast<uint32>(Y) * 2246822519u;
		H = (H ^ (H >> 15)) * 668265263u;
		H ^= (H >> 16);
		return static_cast<float>(H) / static_cast<float>(0xFFFFFFFFu);
	}

	/** 8-neighbour offsets and their planar distance multiplier (1 axial, sqrt(2) diagonal). */
	struct FNeighbour
	{
		int32 DX;
		int32 DY;
		float DistMul;
	};

	static const FNeighbour GNeighbours[8] =
	{
		{ -1,  0, 1.0f }, { 1,  0, 1.0f }, { 0, -1, 1.0f }, { 0, 1, 1.0f },
		{ -1, -1, 1.41421356f }, { 1, -1, 1.41421356f }, { -1, 1, 1.41421356f }, { 1, 1, 1.41421356f }
	};

	// Reference cell size (cm) for scale-invariance: the 100 m / 129-grid footprint -> (10000 + 2*1000) / 128
	// is ~93.75 with skirt, but the carved-feature reference is the *coverage*-only ~78 cm cell. We use the
	// coverage-reference value 78.0 so a fixed cm parameter reproduces the legacy radius-3 / MaxErode behaviour
	// at that footprint and scales proportionally for larger/smaller patches.
	static const double GRefCellCm = 78.0;

	bool AnyNonFinite(const FDenseGrid2f& Grid)
	{
		const TArray64<float>& Values = Grid.GridValues();
		for (int64 Idx = 0; Idx < Values.Num(); ++Idx)
		{
			if (!FMath::IsFinite(Values[Idx]))
			{
				return true;
			}
		}
		return false;
	}

	double TotalMass(const FDenseGrid2f& A, const FDenseGrid2f& B)
	{
		const TArray64<float>& VA = A.GridValues();
		const TArray64<float>& VB = B.GridValues();
		double Sum = 0.0;
		for (int64 Idx = 0; Idx < VA.Num(); ++Idx)
		{
			Sum += static_cast<double>(VA[Idx]) + static_cast<double>(VB[Idx]);
		}
		return Sum;
	}

	/**
	 * Thermal erosion, GATHER form. For each cell c, material flows to neighbours whose drop exceeds the
	 * talus threshold (loss), and flows in from higher neighbours whose own drop to c exceeds it (gain).
	 * Each cell's outgoing total is split proportionally among its qualifying lower neighbours; the symmetric
	 * gather of those same shares guarantees Height conservation (Sum Delta == 0).
	 *
	 * Thermal does NOT touch the conserved suspended-sediment pool: SedimentOut passes SedimentIn through
	 * unchanged. The magnitude of moved talus per cell is recorded separately into DepositionAccum (a
	 * general visual-only "recently moved material" accumulator, EXCLUDED from the conservation invariant).
	 * This is what later gets mixed into the output Sediment channel.
	 */
	void ThermalSubstep(
		const FDenseGrid2f& HeightIn, FDenseGrid2f& HeightOut,
		const FDenseGrid2f& SedimentIn, FDenseGrid2f& SedimentOut,
		FDenseGrid2f& DepositionAccum,
		int32 ResX, int32 ResY, float TalusThresholdPerCell, float ThermalRate)
	{
		ParallelFor(ResY, [&](int32 Y)
		{
			for (int32 X = 0; X < ResX; ++X)
			{
				const float HC = HeightIn.At(X, Y);

				// --- Loss: this cell's outgoing material to its lower neighbours ---
				float SumExcessOut = 0.0f;
				float MaxExcessOut = 0.0f;
				for (int32 N = 0; N < 8; ++N)
				{
					const int32 NX = X + GNeighbours[N].DX;
					const int32 NY = Y + GNeighbours[N].DY;
					if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
					{
						continue;
					}
					const float Threshold = TalusThresholdPerCell * GNeighbours[N].DistMul;
					const float Drop = HC - HeightIn.At(NX, NY) - Threshold;
					if (Drop > 0.0f)
					{
						SumExcessOut += Drop;
						MaxExcessOut = FMath::Max(MaxExcessOut, Drop);
					}
				}
				// Total material this cell sheds this substep (bounded by half the largest excess for stability).
				const float Loss = (SumExcessOut > 0.0f) ? (ThermalRate * 0.5f * MaxExcessOut) : 0.0f;

				// --- Gain: inflow from higher neighbours that shed toward this cell ---
				float Gain = 0.0f;
				for (int32 N = 0; N < 8; ++N)
				{
					const int32 NX = X + GNeighbours[N].DX;
					const int32 NY = Y + GNeighbours[N].DY;
					if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
					{
						continue;
					}
					const float HN = HeightIn.At(NX, NY);
					const float Threshold = TalusThresholdPerCell * GNeighbours[N].DistMul;

					// Recompute neighbour N's outgoing distribution exactly as above, then take c's share.
					float NSumExcess = 0.0f;
					float NMaxExcess = 0.0f;
					float DropToC = 0.0f;
					for (int32 M = 0; M < 8; ++M)
					{
						const int32 MX = NX + GNeighbours[M].DX;
						const int32 MY = NY + GNeighbours[M].DY;
						if (MX < 0 || MY < 0 || MX >= ResX || MY >= ResY)
						{
							continue;
						}
						const float MThreshold = TalusThresholdPerCell * GNeighbours[M].DistMul;
						const float MDrop = HN - HeightIn.At(MX, MY) - MThreshold;
						if (MDrop > 0.0f)
						{
							NSumExcess += MDrop;
							NMaxExcess = FMath::Max(NMaxExcess, MDrop);
							if (MX == X && MY == Y)
							{
								DropToC = MDrop;
							}
						}
					}
					if (NSumExcess > 0.0f && DropToC > 0.0f)
					{
						const float NLoss = ThermalRate * 0.5f * NMaxExcess;
						Gain += NLoss * (DropToC / NSumExcess);
					}
				}

				const float Delta = Gain - Loss;
				HeightOut.At(X, Y) = HC + Delta;
				// Suspended sediment is conserved by thermal: pass it through untouched.
				SedimentOut.At(X, Y) = SedimentIn.At(X, Y);
				// Record talus motion magnitude separately (visual-only, not in the conservation invariant).
				DepositionAccum.At(X, Y) += FMath::Abs(Delta);
			}
		});
	}

	/** Per-cell outflow fraction of a cell's downhill drop toward neighbour at offset N, normalized over all
	 *  of c's downhill drops. Computed IDENTICALLY whether c is the center being debited or a neighbour being
	 *  gathered, against the same frozen HeightIn buffer -> debit == gather. Returns 0 if c is a pit. */
	float TotalDownhillDrop(const FDenseGrid2f& HeightIn, int32 X, int32 Y, int32 ResX, int32 ResY)
	{
		const float HC = HeightIn.At(X, Y);
		float Total = 0.0f;
		for (int32 N = 0; N < 8; ++N)
		{
			const int32 NX = X + GNeighbours[N].DX;
			const int32 NY = Y + GNeighbours[N].DY;
			if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
			{
				continue;
			}
			const float Drop = (HC - HeightIn.At(NX, NY)) / GNeighbours[N].DistMul;
			if (Drop > 0.0f)
			{
				Total += Drop;
			}
		}
		return Total;
	}

	/**
	 * Fill Out[X,Y] = TotalDownhillDrop(HeightIn, X, Y) for every cell, in parallel. Lets the strictly-serial
	 * flow sweep and the gather transport READ this once-per-iteration instead of recomputing it inline — the
	 * transport pass previously recomputed it for the centre AND for each uphill neighbour (~9x per cell).
	 * Pure function of HeightIn, so values (and summation order) are bit-identical to the inline calls.
	 */
	void FillDownhillDrop(const FDenseGrid2f& HeightIn, int32 ResX, int32 ResY, FDenseGrid2f& Out)
	{
		ParallelFor(ResY, [&](int32 Y)
		{
			for (int32 X = 0; X < ResX; ++X)
			{
				Out.At(X, Y) = TotalDownhillDrop(HeightIn, X, Y, ResX, ResY);
			}
		});
	}

	void ConfigureField(FSampledScalarField2f& Field, int32 ResX, int32 ResY,
		const FVector2f& Origin, const FVector2f& CellDim)
	{
		Field.Resize(ResX, ResY, 0.0f);
		Field.GridOrigin = Origin;
		Field.CellDimensions = CellDim;
	}

	void CopyGridIntoField(const FDenseGrid2f& Src, FSampledScalarField2f& Field)
	{
		TArray64<float>& Dst = Field.GridValues.GridValues();
		const TArray64<float>& S = Src.GridValues();
		for (int64 Idx = 0; Idx < S.Num(); ++Idx)
		{
			Dst[Idx] = S[Idx];
		}
	}

	/**
	 * O(N) LSD radix sort of cell linear indices, ordered by height DESCENDING with ties broken by index
	 * ASCENDING and NaN cells pushed last — bit-for-bit the same ordering the prior StableSort comparator
	 * produced, but the per-cell NaN test runs exactly once here (during key build) instead of ~N*log2(N)
	 * times inside a sort comparator. That comparator, non-inlined under DebugGame, was ~97% of the whole
	 * erosion run (~1.3s of every ~1.35s outer iteration); a counting radix removes both the log-factor and
	 * the hot per-compare function calls.
	 *
	 * Each cell is packed into a uint64 = (heightKey:32 << 32) | cellIndex:32 and the PACKED values are sorted
	 * directly, so every pass reads its array sequentially — no `Key[Index[i]]` gather. At 1M+ cells that
	 * indirect gather was the dominant cost (~6.5s/40-iter @ 1024); sequential access removes it. The height
	 * key occupies the high bits (primary order) and the cell index the low bits, so a key-only radix with
	 * stable passes yields height-DESCENDING with index-ASCENDING ties for free — no secondary key needed.
	 *
	 * PackedSrc / PackedDst are caller-owned uint64 scratch sized to NumCells; OutOrder receives the sorted
	 * cell indices (low 32 bits extracted after the final pass). 4 passes => even ping-pong, result in PackedSrc.
	 */
	void RadixSortCellsByHeightDescending(
		const FDenseGrid2f& H, int32 ResX, int32 NumCells,
		TArray<uint64>& PackedSrc, TArray<uint64>& PackedDst, TArray<int32>& OutOrder)
	{
		for (int32 Idx = 0; Idx < NumCells; ++Idx)
		{
			const int32 X = Idx % ResX;
			const int32 Y = Idx / ResX;
			const float HeightVal = H.At(X, Y);

			uint32 Key;
			if (FMath::IsNaN(HeightVal))
			{
				Key = 0xFFFFFFFFu; // NaN -> largest key -> sorts last; equal NaNs keep index order via the low bits.
			}
			else
			{
				// Canonical radix-sortable mapping: monotonically ASCENDING in float value (flip all bits when
				// negative, only the sign bit when non-negative). Then invert so an ascending radix sweep yields
				// DESCENDING height.
				const uint32 Bits = FMath::AsUInt(HeightVal);
				const uint32 Ascending = (Bits & 0x80000000u) ? ~Bits : (Bits | 0x80000000u);
				Key = ~Ascending;
			}
			PackedSrc[Idx] = (static_cast<uint64>(Key) << 32) | static_cast<uint64>(static_cast<uint32>(Idx));
		}

		// Four stable counting passes over the 8-bit digits of the HIGH 32 bits (the height key), LSB-of-key
		// first. The low 32 bits (cell index) are never a sort digit, so equal-height cells keep input order =
		// index ascending. PackedSrc is read sequentially each pass — that sequential access is the cache win.
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			const uint32 Shift = 32u + static_cast<uint32>(Pass) * 8u;

			uint32 Counts[256] = {};
			for (int32 I = 0; I < NumCells; ++I)
			{
				++Counts[(PackedSrc[I] >> Shift) & 0xFFu];
			}

			uint32 Offsets[256];
			uint32 Running = 0;
			for (int32 Bucket = 0; Bucket < 256; ++Bucket)
			{
				Offsets[Bucket] = Running;
				Running += Counts[Bucket];
			}

			for (int32 I = 0; I < NumCells; ++I)
			{
				const uint64 Value = PackedSrc[I];
				const uint32 Digit = static_cast<uint32>((Value >> Shift) & 0xFFu);
				PackedDst[Offsets[Digit]++] = Value;
			}

			Swap(PackedSrc, PackedDst);
		}

		// 4 passes is even -> sorted packed values are back in PackedSrc. Extract the cell index (low 32 bits).
		for (int32 I = 0; I < NumCells; ++I)
		{
			OutOrder[I] = static_cast<int32>(PackedSrc[I] & 0xFFFFFFFFu);
		}
	}

	/**
	 * GridStreamPower: flow-accumulation + stream-power incision + MFD transport/deposit, interleaved with the
	 * existing thermal repose pass. Drives channel cutting from accumulated upstream drainage (A) rather than a
	 * purely local water term, so cuts concentrate into dendritic valleys (erosion ~ K * A^m * S^n).
	 *
	 * Conservation: incision credits removed rock into the suspended-sediment pool (SCur), and transport/deposit
	 * only redistributes that pool back into H; the sole sanctioned leak is sediment routed off the grid edge.
	 *
	 * On entry HCur/SCur hold the current Height/Sediment state. On exit the SAME pointers hold the final state
	 * (the caller's ping-pong pointers are swapped through). FlowAccum receives the final outer iteration's A.
	 */
	void RunStreamPower(
		FDenseGrid2f*& HCur, FDenseGrid2f*& HNext,
		FDenseGrid2f*& SCur, FDenseGrid2f*& SNext,
		FDenseGrid2f& FlowAccum, FDenseGrid2f& D8AccumOut, FDenseGrid2f& DepositionAccum,
		int32 ResX, int32 ResY, double CellSizeCm, double BaselineMass, double MassTolerance,
		float TalusThresholdPerCell, float ThermalRate,
		const ArcTerrainErosion::FParams& Params)
	{
		const int32 OuterIterations = FMath::Max(0, Params.OuterIterations);
		const int32 ThermalSubsteps = FMath::Max(0, Params.ThermalSubsteps);

		const float RainAmount = static_cast<float>(Params.RainAmount);
		// StreamPowerM is REINTERPRETED here as the carve drainage exponent (NOT the old K*A^m*S^n incision
		// power). default 0.5 = broad network (tributaries deepen too); higher = only the trunk channels deepen.
		// StreamPowerK / StreamPowerN are retained on FParams for hash/compat but are unused by channel carving.
		const float DrainageExponent = static_cast<float>(Params.StreamPowerM);
		const float SedimentCapacity = static_cast<float>(Params.SedimentCapacity);
		// Channel-network gate: a cell carves iff its D8 drainage >= ChannelThreshold * MaxD8 (peak drainage).
		const float ChannelThreshold = FMath::Clamp(static_cast<float>(Params.ChannelThreshold), 0.0f, 1.0f);
		// Unified channel-depth knob (shared with the droplet path): governs the total trunk carve depth as a
		// fraction of terrain Height, clamped to [0.1, 0.7].
		const float ChannelDepthFractionClamped = FMath::Clamp(static_cast<float>(Params.ChannelDepthFraction), 0.1f, 0.7f);
		const float StreamPowerDepositRate = static_cast<float>(Params.StreamPowerDepositRate);

		// Scale-invariant incision cap: MaxErodePerStepCm is the cap at the reference footprint (GRefCellCm);
		// scale it by (CellSizeCm / GRefCellCm) so a fixed cm value erodes proportionally across patch sizes.
		const float CellScale = (GRefCellCm > 0.0) ? static_cast<float>(CellSizeCm / GRefCellCm) : 1.0f;
		const float MaxErodePerStepCm = static_cast<float>(Params.MaxErodePerStepCm) * CellScale;

		// Mass-conservation lower bound for grid channel-carving mode. Carve depth is bounded per cell by the
		// total-depth floor (Base - ChannelDepthFraction*Height); with the default 0.25 fraction the worst-case
		// removal is well under half the baseline, so 0.5 (50% of baseline) remains a valid, principled bound
		// (matches the MassBounded_GridStreamPower test) covering carve removal + sediment edge outflow.
		const double MassFloorFraction = 0.5;

		const int32 NumCells = ResX * ResY;

		// Scratch reused across outer iterations.
		FDenseGrid2f Accum(ResX, ResY, 0.0f);    // MFD (multiple-flow) drainage: capacity + Flow/Wetness display.
		FDenseGrid2f D8Accum(ResX, ResY, 0.0f);  // D8 (single steepest-descent) drainage: incision A only.
		FDenseGrid2f DownhillDrop(ResX, ResY, 0.0f); // Per-iteration TotalDownhillDrop cache (refilled per phase).
		// Radix-sort scratch (reused every outer iteration): ping-pong packed (key|index) buffers + sorted-index out.
		TArray<uint64> SortPackedSrc;
		SortPackedSrc.SetNumUninitialized(NumCells);
		TArray<uint64> SortPackedDst;
		SortPackedDst.SetNumUninitialized(NumCells);
		TArray<int32> SortIndexSrc;
		SortIndexSrc.SetNumUninitialized(NumCells);

		// Flow-sweep precompute scratch: the per-cell MATH (rain, MFD shares, D8 target) is computed in
		// PARALLEL so the strictly-serial height-ordered sweep below does only the accumulation adds. All three
		// are rewritten in full each pass (no stale carry-over across outer iters, e.g. a cell turning into/out
		// of a pit). MfdSharePerSlot[c*8 + slot] holds Drop/TotalDrop (0 for non-downhill or out-of-bounds
		// slots); D8TargetSlot[c] is the steepest-descent GNeighbours slot or -1. The downstream cell is reached
		// via c + NeighbourLinearOffset[slot]: for an out-of-bounds slot that offset wraps to a wrong-but-valid
		// index, which is never read because its share is 0 (and D8 only ever stores an in-bounds slot).
		TArray64<float> MfdSharePerSlot;
		MfdSharePerSlot.SetNumUninitialized(static_cast<int64>(NumCells) * 8);
		TArray64<int32> D8TargetSlot;
		D8TargetSlot.SetNumUninitialized(NumCells);
		int32 NeighbourLinearOffset[8];
		for (int32 N = 0; N < 8; ++N)
		{
			NeighbourLinearOffset[N] = GNeighbours[N].DY * ResX + GNeighbours[N].DX;
		}

		// Rain is a pure function of cell coords + seed (HashUnitFloat), identical on every outer iteration,
		// so compute it ONCE here (parallel) instead of per-iteration inside the serial sweep.
		TArray64<float> RainPerCell;
		RainPerCell.SetNumUninitialized(NumCells);
		ParallelFor(ResY, [&](int32 Y)
		{
			for (int32 X = 0; X < ResX; ++X)
			{
				RainPerCell[static_cast<int64>(Y) * ResX + X] = RainAmount * HashUnitFloat(X, Y, Params.Seed);
			}
		});

		// Snapshot the initial fill: the carve total-depth floor is measured against this so channels can never
		// trench below Base - ChannelDepthFraction*Height regardless of how many outer iterations run.
		const FDenseGrid2f BaseHeight = *HCur;

		// Reference slope (drop-per-cell) for the SlopeFactor lerp. Gentle constant in drop/cell units: flat trunk
		// channels (slope < RefSlope) still carve at least 50%, steeper channels carve full depth. Kept small so
		// channels keep carving on near-flat valley floors (zeroing on slope would reintroduce diffusion).
		const float RefSlope = 0.05f;

		// Per-phase wall-clock accumulators — a slow run (e.g. the observed 50s) should be attributable
		// to ONE phase (or to an unexpectedly large OuterIterations), reported in the summary below.
		double SortFlowSeconds = 0.0;
		double SortSeconds = 0.0;  // sub-split of SortFlowSeconds: the per-iteration StableSort only.
		double SweepSeconds = 0.0; // sub-split of SortFlowSeconds: zero + highest-first MFD/D8 sweep + final copies.
		double CarveSeconds = 0.0;
		double TransportSeconds = 0.0;
		double ThermalSeconds = 0.0;
		double EnsureSeconds = 0.0;
		const double RunStartSeconds = FPlatformTime::Seconds();

		// DIAGNOSTIC: the summary log at the end never fires if we hang mid-run, so announce the workload up
		// front and emit a per-iteration line below. A missing "iter N/M done" line pins the stuck iteration.
		UE_LOG(LogArcTerrainErosion, Display,
			TEXT("RunStreamPower: BEGIN %d iter(s) on %dx%d (%d cells), %d thermal substep(s)."),
			OuterIterations, ResX, ResY, NumCells, ThermalSubsteps);

		for (int32 Outer = 0; Outer < OuterIterations; ++Outer)
		{
			const double IterT0 = FPlatformTime::Seconds();

			// =========================================================================================
			// (i) FLOW ACCUMULATION A — STRICTLY SEQUENTIAL (dependency-chain reduction, no ParallelFor).
			//     Order cells by (height DESC, index ASC), NaN last, via an O(N) radix sort so the sweep
			//     visits highest-first. Sorted indices land in SortIndexSrc.
			// =========================================================================================
			const FDenseGrid2f& H = *HCur;
			const double SortT0 = FPlatformTime::Seconds();
			RadixSortCellsByHeightDescending(H, ResX, NumCells, SortPackedSrc, SortPackedDst, SortIndexSrc);
			SortSeconds += FPlatformTime::Seconds() - SortT0;

			const double SweepT0 = FPlatformTime::Seconds();
			// Zero both accumulation grids, then sweep highest-first (one pass fills BOTH MFD and D8).
			{
				TArray64<float>& AccumVals = Accum.GridValues();
				TArray64<float>& D8Vals = D8Accum.GridValues();
				for (int64 Idx = 0; Idx < AccumVals.Num(); ++Idx)
				{
					AccumVals[Idx] = 0.0f;
					D8Vals[Idx] = 0.0f;
				}
			}

			// Parallel precompute of per-cell total downhill drop (denominator for the MFD shares below).
			FillDownhillDrop(H, ResX, ResY, DownhillDrop);

			// Parallel precompute of the per-cell flow MATH: MFD share per neighbour slot + the D8 target slot.
			// Replicates the old serial sweep's Drop/share/D8-best logic EXACTLY (same float ops, same GNeighbours
			// order, same strict-'>' first-seen D8 tie-break, same out-of-bounds skip) so the result is
			// bit-identical; it only moves the work off the serial dependency chain. EVERY slot of EVERY cell is
			// written so nothing stale carries over from a previous iteration (e.g. a cell becoming/leaving a pit).
			ParallelFor(ResY, [&](int32 Y)
			{
				for (int32 X = 0; X < ResX; ++X)
				{
					const int64 Cell = static_cast<int64>(Y) * ResX + X;
					const float HC = H.At(X, Y);
					const float TotalDrop = DownhillDrop.At(X, Y);

					float BestDrop = 0.0f;
					int32 BestSlot = -1;
					for (int32 N = 0; N < 8; ++N)
					{
						const int32 NX = X + GNeighbours[N].DX;
						const int32 NY = Y + GNeighbours[N].DY;
						float Share = 0.0f;
						if (NX >= 0 && NY >= 0 && NX < ResX && NY < ResY)
						{
							const float Drop = (HC - H.At(NX, NY)) / GNeighbours[N].DistMul;
							if (Drop > 0.0f)
							{
								// TotalDrop is > 0 whenever any Drop > 0 (it sums exactly these), so this matches
								// the old `(Drop / TotalDrop)` bit-for-bit and never divides by zero.
								Share = Drop / TotalDrop;
								if (Drop > BestDrop) // strict '>' -> first-seen max wins (deterministic tie-break)
								{
									BestDrop = Drop;
									BestSlot = N;
								}
							}
						}
						MfdSharePerSlot[Cell * 8 + N] = Share;
					}
					D8TargetSlot[Cell] = BestSlot;
				}
			});

			// Serial highest-first accumulation: ONLY the dependency-carrying adds remain. All per-cell math is
			// read from the parallel precompute above. Accum/D8Accum are indexed by LINEAR index via GridValues()
			// (no .At(X,Y) decompose). Donors still arrive in height order, so the summation order — and thus the
			// float result — is identical to the old sweep.
			{
				TArray64<float>& AccumVals = Accum.GridValues();
				TArray64<float>& D8Vals = D8Accum.GridValues();
				for (int32 Sorted = 0; Sorted < NumCells; ++Sorted)
				{
					const int32 Cell = SortIndexSrc[Sorted];

					// Rain seeded first (applied to pits too; they just have no downhill slots / no D8 target).
					const float RainC = RainPerCell[Cell];
					const float CellAccum = AccumVals[Cell] + RainC;
					AccumVals[Cell] = CellAccum;
					const float D8CellAccum = D8Vals[Cell] + RainC;
					D8Vals[Cell] = D8CellAccum;

					// MFD: distribute to each downhill neighbour by its precomputed slope-weighted share.
					const int64 Base = static_cast<int64>(Cell) * 8;
					for (int32 N = 0; N < 8; ++N)
					{
						const float Share = MfdSharePerSlot[Base + N];
						if (Share > 0.0f)
						{
							AccumVals[Cell + NeighbourLinearOffset[N]] += CellAccum * Share;
						}
					}

					// D8: 100% to the single steepest-descent neighbour (none for a pit).
					const int32 D8Slot = D8TargetSlot[Cell];
					if (D8Slot >= 0)
					{
						D8Vals[Cell + NeighbourLinearOffset[D8Slot]] += D8CellAccum;
					}
				}
			}

			// FlowAccum / D8AccumOut are pure outputs: overwritten (not accumulated) and read only after the
			// loop, so only the FINAL iteration's values matter. Copy on the last pass only — earlier copies
			// were dead work (2x NumCells floats per iteration).
			if (Outer == OuterIterations - 1)
			{
				// Final iteration's MFD A -> FlowAccum (Flow/Wetness display channel).
				{
					TArray64<float>& FlowVals = FlowAccum.GridValues();
					const TArray64<float>& AccumVals = Accum.GridValues();
					for (int64 Idx = 0; Idx < FlowVals.Num(); ++Idx)
					{
						FlowVals[Idx] = AccumVals[Idx];
					}
				}

				// Final iteration's D8 A -> D8AccumOut (incision drainage exposed on FResult.IncisionDrainage).
				{
					TArray64<float>& D8OutVals = D8AccumOut.GridValues();
					const TArray64<float>& D8Vals = D8Accum.GridValues();
					for (int64 Idx = 0; Idx < D8OutVals.Num(); ++Idx)
					{
						D8OutVals[Idx] = D8Vals[Idx];
					}
				}
			}

			SweepSeconds += FPlatformTime::Seconds() - SweepT0;
			SortFlowSeconds += FPlatformTime::Seconds() - IterT0;
			const double IterT1 = FPlatformTime::Seconds();

			// =========================================================================================
			// (ii) CHANNEL CARVING — explicit threshold-based incision (REPLACES K*A^m*S^n, which never
			//      concentrated). Concentration is by construction: only cells whose D8 drainage clears
			//      ChannelThreshold*MaxD8 are on the dendritic network and get carved; everything else passes
			//      H/S through untouched (thermal handles hillslopes). A is read-only D8Accum (race-free); each
			//      cell writes ONLY its own H (HNext) and S (SNext): H -= Carve, S += Carve (conservative credit).
			// =========================================================================================
			{
				// MaxD8: deterministic serial max over D8Accum (order-independent reduction; no ParallelFor race).
				float MaxD8 = 0.0f;
				{
					const TArray64<float>& D8Vals = D8Accum.GridValues();
					for (int64 Idx = 0; Idx < D8Vals.Num(); ++Idx)
					{
						MaxD8 = FMath::Max(MaxD8, D8Vals[Idx]);
					}
				}
				const float ChannelThresholdAbs = ChannelThreshold * MaxD8;

				const FDenseGrid2f& HIn = *HCur;
				const FDenseGrid2f& SIn = *SCur;
				FDenseGrid2f& HOut = *HNext;
				FDenseGrid2f& SOut = *SNext;
				ParallelFor(ResY, [&](int32 Y)
				{
					for (int32 X = 0; X < ResX; ++X)
					{
						const float HCc = HIn.At(X, Y);
						const float A = FMath::Max(0.0f, D8Accum.At(X, Y));

						// Off-network cells: NOT incised here. The gate is what concentrates the cut into channels.
						if (A < ChannelThresholdAbs)
						{
							HOut.At(X, Y) = HCc;
							SOut.At(X, Y) = SIn.At(X, Y);
							continue;
						}

						// Drainage rank within the network: trunk (A==MaxD8) -> 1, faintest tributary -> ~0.
						const float Norm = (MaxD8 > ChannelThresholdAbs)
							? (A - ChannelThresholdAbs) / (MaxD8 - ChannelThresholdAbs)
							: 1.0f;

						// Local slope = max downhill drop / cell distance (same LocalSlope pattern as elsewhere).
						float LocalSlope = 0.0f;
						for (int32 N = 0; N < 8; ++N)
						{
							const int32 NX = X + GNeighbours[N].DX;
							const int32 NY = Y + GNeighbours[N].DY;
							if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
							{
								continue;
							}
							const float Drop = (HCc - HIn.At(NX, NY)) / GNeighbours[N].DistMul;
							LocalSlope = FMath::Max(LocalSlope, Drop);
						}
						// Flat trunk channels still carve >= 50%; steeper channels carve full. Never zeroes on slope
						// (a slope-zeroed carve would reintroduce hillslope diffusion into the channel).
						const float SlopeFactor = FMath::Lerp(0.5f, 1.0f, FMath::Clamp(LocalSlope / RefSlope, 0.0f, 1.0f));

						// Carve depth, amortized + scale-invariant: total trunk depth approaches
						// ChannelDepthFraction*Height over all OuterIterations, scaling with Height and iteration
						// count automatically. DrainageExponent (= StreamPowerM) shapes how fast tributaries deepen.
						float Carve = (ChannelDepthFractionClamped * static_cast<float>(Params.Height)
							/ FMath::Max(1, OuterIterations))
							* FMath::Pow(Norm, DrainageExponent) * SlopeFactor;

						// Clamp: by the scaled per-step cap, by available material, and by the total-depth floor
						// (never trench below Base - ChannelDepthFraction*Height).
						Carve = FMath::Min(Carve, MaxErodePerStepCm);
						Carve = FMath::Min(Carve, FMath::Max(0.0f, HCc));
						Carve = FMath::Min(Carve, FMath::Max(0.0f,
							HCc - (BaseHeight.At(X, Y) - ChannelDepthFractionClamped * static_cast<float>(Params.Height))));

						HOut.At(X, Y) = HCc - Carve;
						SOut.At(X, Y) = SIn.At(X, Y) + Carve; // conservative: removed rock becomes suspended sediment
					}
				});
			}
			Swap(HCur, HNext);
			Swap(SCur, SNext);

			CarveSeconds += FPlatformTime::Seconds() - IterT1;
			const double IterT2 = FPlatformTime::Seconds();

			// =========================================================================================
			// (iii) TRANSPORT / DEPOSIT — MFD, GATHER form (race-free, mirrors the old HydraulicTransportPass
			//       gather scheme). Capacity = SedimentCapacity * A * S; excess deposits back into H.
			// =========================================================================================
			{
				const FDenseGrid2f& HIn = *HCur;
				const FDenseGrid2f& SIn = *SCur;
				FDenseGrid2f& HOut = *HNext;
				FDenseGrid2f& SOut = *SNext;

				// Refill the drop cache from the carved height (HIn differs from the sweep's pre-carve field),
				// so the gather below reads centre + neighbour drops instead of recomputing ~9x per cell.
				FillDownhillDrop(HIn, ResX, ResY, DownhillDrop);

				ParallelFor(ResY, [&](int32 Y)
				{
					for (int32 X = 0; X < ResX; ++X)
					{
						const float HCc = HIn.At(X, Y);
						const float SdC = SIn.At(X, Y);

						// Center debit: outflow share of suspended sediment (0 if pit).
						const float TotalDropC = DownhillDrop.At(X, Y);
						const float OutSedC = (TotalDropC > 0.0f) ? SdC : 0.0f;
						float Sed = SdC - OutSedC;

						// Gather: inflow from each higher neighbour n, weighted by n's MFD share toward c.
						for (int32 N = 0; N < 8; ++N)
						{
							const int32 NX = X + GNeighbours[N].DX;
							const int32 NY = Y + GNeighbours[N].DY;
							if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
							{
								continue;
							}
							const float HN = HIn.At(NX, NY);
							const float DropToHere = (HN - HCc) / GNeighbours[N].DistMul; // == drop(n -> c)
							if (DropToHere <= 0.0f)
							{
								continue;
							}
							const float NTotalDrop = DownhillDrop.At(NX, NY);
							if (NTotalDrop <= 0.0f)
							{
								continue;
							}
							const float Share = DropToHere / NTotalDrop;
							Sed += SIn.At(NX, NY) * Share;
						}

						// Local slope for capacity (max downhill drop).
						float LocalSlope = 0.0f;
						for (int32 N = 0; N < 8; ++N)
						{
							const int32 NX = X + GNeighbours[N].DX;
							const int32 NY = Y + GNeighbours[N].DY;
							if (NX < 0 || NY < 0 || NX >= ResX || NY >= ResY)
							{
								continue;
							}
							const float Drop = (HCc - HIn.At(NX, NY)) / GNeighbours[N].DistMul;
							LocalSlope = FMath::Max(LocalSlope, Drop);
						}

						// Capacity uses A (drainage) * S (slope); deposit the excess back into H.
						const float A = FMath::Max(0.0f, Accum.At(X, Y));
						const float Capacity = SedimentCapacity * A * FMath::Max(0.0f, LocalSlope);
						float NewHeight = HCc;
						if (Sed > Capacity)
						{
							const float Deposit = StreamPowerDepositRate * (Sed - Capacity);
							NewHeight += Deposit;
							Sed -= Deposit;
							DepositionAccum.At(X, Y) += Deposit;
						}

						HOut.At(X, Y) = NewHeight;
						SOut.At(X, Y) = FMath::Max(0.0f, Sed);
					}
				});
			}
			Swap(HCur, HNext);
			Swap(SCur, SNext);

			TransportSeconds += FPlatformTime::Seconds() - IterT2;
			const double IterT3 = FPlatformTime::Seconds();

			// =========================================================================================
			// (iv) THERMAL — existing repose pass, now writing the general DepositionAccum.
			// =========================================================================================
			for (int32 K = 0; K < ThermalSubsteps; ++K)
			{
				ThermalSubstep(*HCur, *HNext, *SCur, *SNext, DepositionAccum, ResX, ResY, TalusThresholdPerCell, ThermalRate);
				Swap(HCur, HNext);
				Swap(SCur, SNext);
			}

			ThermalSeconds += FPlatformTime::Seconds() - IterT3;
			const double IterT4 = FPlatformTime::Seconds();

			ensureMsgf(!AnyNonFinite(*HCur), TEXT("ArcTerrainErosion: NaN/Inf in Height after outer iter %d"), Outer);
			ensureMsgf(!AnyNonFinite(*SCur), TEXT("ArcTerrainErosion: NaN/Inf in Sediment after outer iter %d"), Outer);

			// Conservation: incision credits SCur and transport/deposit only redistribute, so H+S is
			// downward-only conservative — sediment routed off the grid edge is the only sanctioned leak.
			const double Mass = TotalMass(*HCur, *SCur);
			ensure(Mass <= BaselineMass + MassTolerance);
			ensure(Mass >= BaselineMass * MassFloorFraction);

			EnsureSeconds += FPlatformTime::Seconds() - IterT4;

			// DIAGNOSTIC per-iteration heartbeat: if the log stops at "iter K/M" the hang is in iteration K+1.
			// %.1f is this iteration's wall time (sum of the phase deltas for this pass only).
			UE_LOG(LogArcTerrainErosion, Display,
				TEXT("RunStreamPower: iter %d/%d done (%.1f ms this iter)."),
				Outer + 1, OuterIterations, (FPlatformTime::Seconds() - IterT0) * 1000.0);
		}

		const double TotalSeconds = FPlatformTime::Seconds() - RunStartSeconds;
		UE_LOG(LogArcTerrainErosion, Display,
			TEXT("RunStreamPower: %d iter(s) on %dx%d (%d cells), %d thermal substep(s) in %.1f ms total ")
			TEXT("[sort+flow %.1f (sort %.1f | sweep %.1f) | carve %.1f | transport %.1f | thermal %.1f | ensure %.1f ms]."),
			OuterIterations, ResX, ResY, NumCells, ThermalSubsteps, TotalSeconds * 1000.0,
			SortFlowSeconds * 1000.0, SortSeconds * 1000.0, SweepSeconds * 1000.0,
			CarveSeconds * 1000.0, TransportSeconds * 1000.0,
			ThermalSeconds * 1000.0, EnsureSeconds * 1000.0);
	}

	/** Inline bilinear height sample on Grid at fractional offset (Fx,Fy) within the 4-cell stencil whose base
	 *  cell is (CX,CY). Caller guarantees CX..CX+1, CY..CY+1 are in range. */
	float BilinearHeight(const FDenseGrid2f& Grid, int32 CX, int32 CY, float Fx, float Fy)
	{
		const float H00 = Grid.At(CX, CY);
		const float H10 = Grid.At(CX + 1, CY);
		const float H01 = Grid.At(CX, CY + 1);
		const float H11 = Grid.At(CX + 1, CY + 1);
		return H00 * (1.0f - Fx) * (1.0f - Fy) + H10 * Fx * (1.0f - Fy)
			+ H01 * (1.0f - Fx) * Fy + H11 * Fx * Fy;
	}

	/**
	 * Deposit `Amount` of sediment into the 4-cell bilinear stencil whose base cell is (CX,CY), respecting a
	 * per-cell ceiling of Base + MaxDepositCm so droplets can never pile material arbitrarily high (keeps
	 * vertices inside the engine's Z bounds). Mirrors the deposit into both Work (height) and DepositionAccum
	 * (the deposition display map). Returns the amount actually deposited (<= Amount); the unplaced remainder
	 * is the caller's to keep carrying. Caller guarantees CX..CX+1, CY..CY+1 are in range.
	 */
	float DepositBilinearClamped(
		FDenseGrid2f& Work, FDenseGrid2f& DepositionAccum, const FDenseGrid2f& Base,
		int32 CX, int32 CY, float Fx, float Fy, float Amount, float MaxDepositCm)
	{
		if (Amount <= 0.0f)
		{
			return 0.0f;
		}

		const float W00 = (1.0f - Fx) * (1.0f - Fy);
		const float W10 = Fx * (1.0f - Fy);
		const float W01 = (1.0f - Fx) * Fy;
		const float W11 = Fx * Fy;

		const int32 CellX[4] = { CX, CX + 1, CX, CX + 1 };
		const int32 CellY[4] = { CY, CY, CY + 1, CY + 1 };
		const float CellW[4] = { W00, W10, W01, W11 };

		float TotalDeposited = 0.0f;
		for (int32 C = 0; C < 4; ++C)
		{
			const int32 X = CellX[C];
			const int32 Y = CellY[C];
			const float Want = Amount * CellW[C];
			if (Want <= 0.0f)
			{
				continue;
			}
			const float Ceil = Base.At(X, Y) + MaxDepositCm;       // upper deposit cap for this cell
			const float Room = Ceil - Work.At(X, Y);               // how much still fits before the cap
			const float Placed = FMath::Max(0.0f, FMath::Min(Want, Room));
			if (Placed > 0.0f)
			{
				Work.At(X, Y) += Placed;
				DepositionAccum.At(X, Y) += Placed;
				TotalDeposited += Placed;
			}
		}
		return TotalDeposited;
	}

	/**
	 * Droplet (particle) erosion — classic Beyer/Lague. STRICTLY SEQUENTIAL: each droplet mutates the shared
	 * working height grid, and the fixed left-to-right droplet/step order IS the determinism guarantee. NO
	 * ParallelFor anywhere here. No RNG: spawn positions come from HashUnitFloat only.
	 *
	 * On entry *HCur holds the base fill. A mutable copy is seeded from it; all droplets carve that copy; on
	 * exit it is copied back into *HCur so the caller's tail reads the eroded height. FlowAccum (path water)
	 * and DepositionAccum (droplet deposition map) are OVERWRITTEN/accumulated from zero. SuspendedSediment
	 * (*SCur) is left untouched (zero) for this mode — droplets settle directly into H, so the output Sediment
	 * channel == DepositionAccum.
	 *
	 * Mass conservation: erode/deposit only moves material within the grid (the sole leak is droplets that
	 * walk off the edge), so the caller's one-sided ensure bounds still hold.
	 */
	void RunDroplet(
		FDenseGrid2f*& HCur,
		FDenseGrid2f& FlowAccum, FDenseGrid2f& DepositionAccum,
		int32 ResX, int32 ResY, double CellSizeCm, double BaselineMass, double MassTolerance,
		const ArcTerrainErosion::FParams& Params)
	{
		const int32 NumDroplets = FMath::Max(0, Params.NumDroplets);
		const int32 DropletLifetime = FMath::Max(0, Params.DropletLifetime);
		const int32 Seed = Params.Seed;

		const float Inertia = static_cast<float>(Params.DropletInertia);
		const float CapacityFactor = static_cast<float>(Params.DropletCapacity);
		const float ErodeRate = static_cast<float>(Params.DropletErodeRate);
		const float DepositRate = static_cast<float>(Params.DropletDepositRate);
		const float Evaporation = static_cast<float>(Params.DropletEvaporation);
		const float Gravity = static_cast<float>(Params.DropletGravity);
		const float MinSlope = static_cast<float>(Params.DropletMinSlope);

		// Scale-invariant brush radius: max(1, round(FeatureSizeCm / CellSizeCm)) keeps the carved feature size
		// constant in world-cm across patch sizes. Supersedes the raw Params.DropletErosionRadius (now unused here).
		const int32 ErosionRadius = (CellSizeCm > 0.0)
			? FMath::Max(1, FMath::RoundToInt(Params.FeatureSizeCm / CellSizeCm))
			: 1;

		// Working height grid: mutable copy of the base fill. All carving happens here.
		FDenseGrid2f Work = *HCur;

		// *HCur is the untouched base fill until the final copy-back; read original heights from it (do NOT
		// modify it during the loop). Used for the per-cell carve floor and deposit ceiling below.
		const FDenseGrid2f& Base = *HCur;

		// Hard depth cap = ChannelDepthFraction of terrain Height: droplets may never carve a cell below
		// Base-MaxCarveCm (kills the 100m trenches) nor pile a cell above Base+MaxDepositCm (keeps vertices
		// inside the engine Z bounds). Configurable depth replaces the old hard-coded 0.25.
		const float ChannelDepthFraction = FMath::Clamp(static_cast<float>(Params.ChannelDepthFraction), 0.1f, 0.7f);
		const float MaxCarveCm = ChannelDepthFraction * static_cast<float>(Params.Height);
		const float MaxDepositCm = ChannelDepthFraction * static_cast<float>(Params.Height);

		// Zero the output accumulators (overwrite semantics, like RunStreamPower's Flow handling).
		{
			TArray64<float>& FlowVals = FlowAccum.GridValues();
			for (int64 Idx = 0; Idx < FlowVals.Num(); ++Idx)
			{
				FlowVals[Idx] = 0.0f;
			}
			TArray64<float>& DepVals = DepositionAccum.GridValues();
			for (int64 Idx = 0; Idx < DepVals.Num(); ++Idx)
			{
				DepVals[Idx] = 0.0f;
			}
		}

		// --- Precompute the erosion brush once (radius is constant): (dx, dy, normalized weight). ---
		// Linear falloff w = max(0, 1 - dist/(radius+1)); normalized so the in-radius weights sum to 1.
		struct FBrushCell
		{
			int32 DX;
			int32 DY;
			float Weight;
		};
		TArray<FBrushCell> Brush;
		{
			float WeightSum = 0.0f;
			for (int32 BY = -ErosionRadius; BY <= ErosionRadius; ++BY)
			{
				for (int32 BX = -ErosionRadius; BX <= ErosionRadius; ++BX)
				{
					const float Dist = FMath::Sqrt(static_cast<float>(BX * BX + BY * BY));
					const float W = FMath::Max(0.0f, 1.0f - Dist / (static_cast<float>(ErosionRadius) + 1.0f));
					if (W > 0.0f)
					{
						Brush.Add(FBrushCell{ BX, BY, W });
						WeightSum += W;
					}
				}
			}
			if (WeightSum > 0.0f)
			{
				for (int32 Idx = 0; Idx < Brush.Num(); ++Idx)
				{
					Brush[Idx].Weight /= WeightSum;
				}
			}
		}

		const float DirEpsilon = 1e-6f;

		for (int32 i = 0; i < NumDroplets; ++i)
		{
			float PosX = HashUnitFloat(i, 0, Seed) * static_cast<float>(ResX - 1);
			float PosY = HashUnitFloat(i, 1, Seed) * static_cast<float>(ResY - 1);

			float Water = 1.0f;
			float Speed = 1.0f;
			float Sediment = 0.0f;
			float DirX = 0.0f;
			float DirY = 0.0f;

			for (int32 Step = 0; Step < DropletLifetime; ++Step)
			{
				// Old cell + fractionals. Clamp the base cell so the 4-cell stencil stays in range.
				int32 CX = FMath::FloorToInt(PosX);
				int32 CY = FMath::FloorToInt(PosY);
				if (CX < 0 || CY < 0 || CX >= ResX - 1 || CY >= ResY - 1)
				{
					break; // at/past the top/right edge — stencil would read out of bounds.
				}
				const float Fx = PosX - static_cast<float>(CX);
				const float Fy = PosY - static_cast<float>(CY);

				// --- Inline bilinear height + gradient over the 4 stencil cells. ---
				const float H00 = Work.At(CX, CY);
				const float H10 = Work.At(CX + 1, CY);
				const float H01 = Work.At(CX, CY + 1);
				const float H11 = Work.At(CX + 1, CY + 1);

				const float GradX = (H10 - H00) * (1.0f - Fy) + (H11 - H01) * Fy;
				const float GradY = (H01 - H00) * (1.0f - Fx) + (H11 - H10) * Fx;
				const float Hold = H00 * (1.0f - Fx) * (1.0f - Fy) + H10 * Fx * (1.0f - Fy)
					+ H01 * (1.0f - Fx) * Fy + H11 * Fx * Fy;

				// --- New direction: blend inertia with the downhill (negative gradient) direction. ---
				DirX = DirX * Inertia - GradX * (1.0f - Inertia);
				DirY = DirY * Inertia - GradY * (1.0f - Inertia);
				const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
				if (DirLen <= DirEpsilon)
				{
					break; // on a flat or in a pit — no defined flow direction.
				}
				DirX /= DirLen;
				DirY /= DirLen;

				// --- Step exactly one cell along the flow direction. ---
				PosX += DirX;
				PosY += DirY;

				// New cell + edge guard (same 4-cell stencil requirement).
				const int32 NCX = FMath::FloorToInt(PosX);
				const int32 NCY = FMath::FloorToInt(PosY);
				if (NCX < 0 || NCY < 0 || NCX >= ResX - 1 || NCY >= ResY - 1)
				{
					break; // droplet left the grid.
				}
				const float NFx = PosX - static_cast<float>(NCX);
				const float NFy = PosY - static_cast<float>(NCY);
				const float Hnew = BilinearHeight(Work, NCX, NCY, NFx, NFy);

				const float DeltaH = Hnew - Hold;

				// --- Carrying capacity at the OLD position. ---
				const float Capacity = FMath::Max(-DeltaH, MinSlope) * Speed * Water * CapacityFactor;

				if (DeltaH > 0.0f || Sediment > Capacity)
				{
					// --- DEPOSIT to the 4 OLD-cell stencil cells by bilinear weights (ceiling-clamped). ---
					const float Deposit = (DeltaH > 0.0f)
						? FMath::Min(Sediment, DeltaH)
						: (Sediment - Capacity) * DepositRate;

					// Deposit only what fits under each cell's ceiling; the remainder stays in Sediment to be
					// placed at a later step (or shed at droplet death — a small, depth-clamp-bounded leak).
					const float ActualDeposited = DepositBilinearClamped(
						Work, DepositionAccum, Base, CX, CY, Fx, Fy, Deposit, MaxDepositCm);
					Sediment -= ActualDeposited;
				}
				else
				{
					// --- ERODE: spread the cut over the brush centered on the OLD cell, floored per-cell. ---
					const float Amount = FMath::Min((Capacity - Sediment) * ErodeRate, -DeltaH);

					// Renormalize the brush weights over the in-bounds subset so the removed total == Amount.
					float InBoundsSum = 0.0f;
					for (int32 B = 0; B < Brush.Num(); ++B)
					{
						const int32 BXi = CX + Brush[B].DX;
						const int32 BYi = CY + Brush[B].DY;
						if (BXi >= 0 && BYi >= 0 && BXi < ResX && BYi < ResY)
						{
							InBoundsSum += Brush[B].Weight;
						}
					}
					if (InBoundsSum > 0.0f)
					{
						// Carve each brush cell only down to Base - MaxCarveCm; credit Sediment with the height
						// actually removed (not the unclamped Amount) so mass stays conserved when the floor clips.
						float ActualRemoved = 0.0f;
						for (int32 B = 0; B < Brush.Num(); ++B)
						{
							const int32 BXi = CX + Brush[B].DX;
							const int32 BYi = CY + Brush[B].DY;
							if (BXi >= 0 && BYi >= 0 && BXi < ResX && BYi < ResY)
							{
								const float Floor = FMath::Max(0.0f, Base.At(BXi, BYi) - MaxCarveCm);
								const float Want = Work.At(BXi, BYi) - Amount * (Brush[B].Weight / InBoundsSum);
								const float NewH = FMath::Max(Floor, Want);
								const float Removed = Work.At(BXi, BYi) - NewH; // actual removable (>= 0)
								Work.At(BXi, BYi) = NewH;
								ActualRemoved += Removed;
							}
						}
						Sediment += ActualRemoved;
					}
				}

				// --- Update kinematics + water, then record the flow path at the new cell. ---
				Speed = FMath::Sqrt(FMath::Max(0.0f, Speed * Speed + (-DeltaH) * Gravity));
				Water *= (1.0f - Evaporation);
				FlowAccum.At(NCX, NCY) += Water;
			}
		}

		// Copy the carved working grid back into the caller's *HCur (the final eroded height).
		{
			TArray64<float>& Dst = HCur->GridValues();
			const TArray64<float>& Src = Work.GridValues();
			for (int64 Idx = 0; Idx < Dst.Num(); ++Idx)
			{
				Dst[Idx] = Src[Idx];
			}
		}

		ensureMsgf(!AnyNonFinite(*HCur), TEXT("ArcTerrainErosion: NaN/Inf in Height after droplet erosion"));

		// Conservation: the per-cell depth clamp (carve floor at Base-MaxCarveCm = ChannelDepthFraction*Height)
		// is the real runaway guard — it caps how much any cell can lose, so total mass can't collapse. Droplets
		// that die while still carrying sediment legitimately shed it from Height (a sanctioned, depth-clamp-
		// bounded leak), so the lower bound is intentionally loose. Derive the floor from ChannelDepthFraction so
		// deeper carves don't trip the ensure: Max(0.05, 1 - 3*depth) (= 0.25 at the legacy 0.25 depth).
		const float MassFloorFraction = FMath::Max(0.05f, 1.0f - 3.0f * ChannelDepthFraction);
		const FDenseGrid2f ZeroSediment(ResX, ResY, 0.0f);
		const double Mass = TotalMass(*HCur, ZeroSediment);
		ensure(Mass <= BaselineMass + MassTolerance);
		ensure(Mass >= BaselineMass * static_cast<double>(MassFloorFraction));
	}

	/**
	 * Shared output-assembly tail for all modes. Builds the FResult fields from the four working buffers:
	 *   Height    = FinalHeight
	 *   Flow      = FlowAccum
	 *   Sediment  = SuspendedSediment + DepositionAccum  (loose / recently-moved material display signal)
	 *   Curvature = 5-point Laplacian of FinalHeight
	 *   Wetness   = normalized ln(Flow / max(Slope, eps))
	 * Optionally HeatEquation-smooths Sediment + Curvature.
	 */
	void AssembleResult(
		ArcTerrainErosion::FResult& OutResult,
		const FDenseGrid2f& FinalHeight, const FDenseGrid2f& FlowAccum,
		const FDenseGrid2f& D8AccumResult,
		const FDenseGrid2f& SuspendedSediment, const FDenseGrid2f& DepositionAccum,
		int32 ResX, int32 ResY, const FVector2f& Origin, const FVector2f& CellDim,
		double CellSizeCm, const ArcTerrainErosion::FParams& Params)
	{
		ConfigureField(OutResult.Height, ResX, ResY, Origin, CellDim);
		ConfigureField(OutResult.Sediment, ResX, ResY, Origin, CellDim);
		ConfigureField(OutResult.Flow, ResX, ResY, Origin, CellDim);
		ConfigureField(OutResult.IncisionDrainage, ResX, ResY, Origin, CellDim);
		ConfigureField(OutResult.Curvature, ResX, ResY, Origin, CellDim);
		ConfigureField(OutResult.Wetness, ResX, ResY, Origin, CellDim);

		CopyGridIntoField(FinalHeight, OutResult.Height);
		CopyGridIntoField(FlowAccum, OutResult.Flow);
		CopyGridIntoField(D8AccumResult, OutResult.IncisionDrainage);

		// Sediment CHANNEL = suspended sediment + recently-moved material (deposition accumulator). Display-only.
		{
			TArray64<float>& Dst = OutResult.Sediment.GridValues.GridValues();
			const TArray64<float>& Susp = SuspendedSediment.GridValues();
			const TArray64<float>& Moved = DepositionAccum.GridValues();
			for (int64 Idx = 0; Idx < Dst.Num(); ++Idx)
			{
				Dst[Idx] = Susp[Idx] + Moved[Idx];
			}
		}

		// --- Curvature: 5-point Laplacian of final Height ---
		{
			TArray64<float>& Curv = OutResult.Curvature.GridValues.GridValues();
			for (int32 Y = 0; Y < ResY; ++Y)
			{
				for (int32 X = 0; X < ResX; ++X)
				{
					const int32 XL = FMath::Max(0, X - 1);
					const int32 XR = FMath::Min(ResX - 1, X + 1);
					const int32 YD = FMath::Max(0, Y - 1);
					const int32 YU = FMath::Min(ResY - 1, Y + 1);
					const float Lap = FinalHeight.At(XL, Y) + FinalHeight.At(XR, Y)
						+ FinalHeight.At(X, YD) + FinalHeight.At(X, YU) - 4.0f * FinalHeight.At(X, Y);
					Curv[OutResult.Curvature.GridValues.GetIndex(X, Y)] = Lap;
				}
			}
		}

		// --- Wetness: ln(Flow / max(Slope, eps)), normalized to [0,1] ---
		{
			const float SlopeEps = 1e-4f;
			TArray<float> Raw;
			Raw.SetNumUninitialized(ResX * ResY);
			float MinW = TNumericLimits<float>::Max();
			float MaxW = TNumericLimits<float>::Lowest();
			for (int32 Y = 0; Y < ResY; ++Y)
			{
				for (int32 X = 0; X < ResX; ++X)
				{
					float GradX = 0.0f;
					float GradY = 0.0f;
					OutResult.Height.BilinearSampleGradientClamped(
						FVector2f(Origin.X + X * CellDim.X, Origin.Y + Y * CellDim.Y), GradX, GradY);
					const float Slope = FMath::Sqrt(GradX * GradX + GradY * GradY);
					const float FlowVal = FMath::Max(0.0f, FlowAccum.At(X, Y));
					const float W = FMath::Loge((FlowVal + 1.0f) / FMath::Max(Slope, SlopeEps));
					Raw[Y * ResX + X] = W;
					MinW = FMath::Min(MinW, W);
					MaxW = FMath::Max(MaxW, W);
				}
			}
			const float Range = FMath::Max(MaxW - MinW, KINDA_SMALL_NUMBER);
			TArray64<float>& Wet = OutResult.Wetness.GridValues.GridValues();
			for (int32 Idx = 0; Idx < ResX * ResY; ++Idx)
			{
				Wet[Idx] = FMath::Clamp((Raw[Idx] - MinW) / Range, 0.0f, 1.0f);
			}
		}

		// --- Optional smoothing of Sediment + Curvature (Sigma in grid cells, not cm) ---
		if (Params.SedimentSmoothingRadiusCm > 0.0 && CellSizeCm > 0.0)
		{
			const float Sigma = static_cast<float>(Params.SedimentSmoothingRadiusCm / CellSizeCm);
			UE::Geometry::HeatEquationImplicitAOS<float>(OutResult.Sediment, Sigma, Sigma);
			UE::Geometry::HeatEquationImplicitAOS<float>(OutResult.Curvature, Sigma, Sigma);
		}
	}

	// --- Binary serialization for DDC (same-machine; no endianness concern) ---

	/** Current FResult binary-format version. Bump on layout change (alongside ArcErosionDDCVersion). */
	static const int32 GErosionSerializeVersion = 3;

	/** Serialize one scalar field: int32 W,H; GridOrigin; CellDimensions; then the bulk float buffer. */
	void SerializeField(FArchive& Ar, FSampledScalarField2f& F)
	{
		int32 W = F.GridValues.Width();
		int32 H = F.GridValues.Height();
		Ar << W;
		Ar << H;
		Ar << F.GridOrigin.X;
		Ar << F.GridOrigin.Y;
		Ar << F.CellDimensions.X;
		Ar << F.CellDimensions.Y;

		if (Ar.IsLoading())
		{
			F.GridValues.Resize(W, H);
		}

		const int64 NumBytes = static_cast<int64>(W) * static_cast<int64>(H) * static_cast<int64>(sizeof(float));
		Ar.Serialize(F.GridValues.GridValues().GetData(), NumBytes);
	}

	/**
	 * Serialize a full FResult. Writes an int32 version first; on load, a version mismatch leaves R default
	 * (treated as a cache miss by the caller) and bails.
	 */
	void SerializeResult(FArchive& Ar, ArcTerrainErosion::FResult& R)
	{
		int32 Version = GErosionSerializeVersion;
		Ar << Version;

		if (Ar.IsLoading() && Version != GErosionSerializeVersion)
		{
			ensureMsgf(false, TEXT("ArcTerrainErosion: serialized FResult version %d != %d; treating as cache miss"),
				Version, GErosionSerializeVersion);
			R = ArcTerrainErosion::FResult();
			return;
		}

		SerializeField(Ar, R.Height);
		SerializeField(Ar, R.Sediment);
		SerializeField(Ar, R.Flow);
		SerializeField(Ar, R.IncisionDrainage);
		SerializeField(Ar, R.Curvature);
		SerializeField(Ar, R.Wetness);
	}
}

namespace ArcTerrainErosion
{
	FResult Simulate(const FParams& Params)
	{
		using namespace ArcTerrainErosionInternal;

		const int32 ResX = FMath::Max(2, Params.GridResX);
		const int32 ResY = FMath::Max(2, Params.GridResY);

		// --- Grid geometry (component-local cm) ---
		const double TotalX = Params.CoverageX + 2.0 * Params.SkirtSize;
		const double TotalY = Params.CoverageY + 2.0 * Params.SkirtSize;
		const FVector2f Origin(
			static_cast<float>(-Params.CoverageX * 0.5 - Params.SkirtSize),
			static_cast<float>(-Params.CoverageY * 0.5 - Params.SkirtSize));
		const FVector2f CellDim(
			static_cast<float>(TotalX / (ResX - 1)),
			static_cast<float>(TotalY / (ResY - 1)));
		const double CellSizeCm = 0.5 * (static_cast<double>(CellDim.X) + static_cast<double>(CellDim.Y));

		// =====================================================================================
		// (a) SHARED BASE-FILL: EvalHeight(NoiseCoord(cellLocalXY)) * Height into HeightA.
		// =====================================================================================
		FDenseGrid2f HeightA(ResX, ResY, 0.0f);
		FDenseGrid2f HeightB(ResX, ResY, 0.0f);
		FDenseGrid2f SedimentA(ResX, ResY, 0.0f);
		FDenseGrid2f SedimentB(ResX, ResY, 0.0f);
		FDenseGrid2f FlowAccum(ResX, ResY, 0.0f);
		// D8 single-flow drainage grid for the final outer iteration — used to populate FResult.IncisionDrainage.
		// Only meaningful in GridStreamPower mode; Droplet mode leaves it zero (default).
		FDenseGrid2f D8AccumResult(ResX, ResY, 0.0f);
		// General visual-only "recently moved material" accumulator (incision deposit + thermal talus).
		// NOT part of the conservation invariant; mixed into the output Sediment channel alongside SCur.
		FDenseGrid2f DepositionAccum(ResX, ResY, 0.0f);

		// Use caller-provided input grid when set and resolution matches; fall back to noise synthesis otherwise.
		const bool bHaveInput = Params.InitialHeight.IsValid();
		const bool bInputSizeMatch = bHaveInput
			&& Params.InitialHeight->GridValues.Width()  == ResX
			&& Params.InitialHeight->GridValues.Height() == ResY;

		if (bHaveInput && !bInputSizeMatch)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ArcTerrainErosion: InitialHeight resolution (%d x %d) does not match GridRes (%d x %d); falling back to noise synthesis."),
				Params.InitialHeight->GridValues.Width(), Params.InitialHeight->GridValues.Height(),
				ResX, ResY);
		}

		if (bInputSizeMatch)
		{
			// Copy input heightfield cell-by-cell (absolute cm, no Height scale applied).
			const TArray64<float>& SrcVals = Params.InitialHeight->GridValues.GridValues();
			TArray64<float>& DstVals = HeightA.GridValues();
			for (int64 Idx = 0; Idx < SrcVals.Num(); ++Idx)
			{
				DstVals[Idx] = SrcVals[Idx];
			}
		}
		else
		{
			// Synthesize from FBM noise.
			for (int32 Y = 0; Y < ResY; ++Y)
			{
				for (int32 X = 0; X < ResX; ++X)
				{
					const double LocalX = static_cast<double>(Origin.X) + static_cast<double>(X) * static_cast<double>(CellDim.X);
					const double LocalY = static_cast<double>(Origin.Y) + static_cast<double>(Y) * static_cast<double>(CellDim.Y);
					const double Hr = EvalHeight(NoiseCoord(FVector2d(LocalX, LocalY), Params), Params);
					HeightA.At(X, Y) = static_cast<float>(Hr * Params.Height);
				}
			}
		}

		// Talus threshold: vertical drop per axial cell at the repose angle.
		const float TalusThresholdPerCell = static_cast<float>(FMath::Tan(FMath::DegreesToRadians(Params.TalusAngleDeg)) * CellSizeCm);
		const float ThermalRate = FMath::Clamp(static_cast<float>(Params.ThermalRate), 0.0f, 1.0f);

		// Conservation baseline (Height + Sediment only redistribute; edge outflow may lower it).
		const double BaselineMass = TotalMass(HeightA, SedimentA);
		const double MassTolerance = FMath::Max(1.0, FMath::Abs(BaselineMass) * 1e-3);

		// Ping-pong: *Cur holds the current state at the top of each pass.
		FDenseGrid2f* HCur = &HeightA;
		FDenseGrid2f* HNext = &HeightB;
		FDenseGrid2f* SCur = &SedimentA;
		FDenseGrid2f* SNext = &SedimentB;

		// =====================================================================================
		// (b) MODE DISPATCH — both modes leave the final state in *HCur / *SCur and populate
		//     FlowAccum + DepositionAccum for the shared tail.
		// =====================================================================================
		const double ModeStartSeconds = FPlatformTime::Seconds();
		switch (Params.Mode)
		{
		case EArcErosionMode::GridStreamPower:
			RunStreamPower(HCur, HNext, SCur, SNext, FlowAccum, D8AccumResult, DepositionAccum,
				ResX, ResY, CellSizeCm, BaselineMass, MassTolerance, TalusThresholdPerCell, ThermalRate, Params);
			break;

		case EArcErosionMode::Droplet:
			// Particle erosion carves *HCur in place and fills FlowAccum + DepositionAccum. SCur (suspended
			// sediment) stays zero for this mode — droplets settle directly into Height.
			RunDroplet(HCur, FlowAccum, DepositionAccum,
				ResX, ResY, CellSizeCm, BaselineMass, MassTolerance, Params);
			break;
		}
		const double ModeSeconds = FPlatformTime::Seconds() - ModeStartSeconds;

		// =====================================================================================
		// (c) SHARED OUTPUT-ASSEMBLY TAIL.
		// =====================================================================================
		const double TailStartSeconds = FPlatformTime::Seconds();
		FResult Result;
		AssembleResult(Result, *HCur, FlowAccum, D8AccumResult, *SCur, DepositionAccum,
			ResX, ResY, Origin, CellDim, CellSizeCm, Params);
		const double TailSeconds = FPlatformTime::Seconds() - TailStartSeconds;

		UE_LOG(LogArcTerrainErosion, Display,
			TEXT("Simulate (%s) on %dx%d: mode/erosion %.1f ms, output tail (incl. sediment smoothing) %.1f ms."),
			Params.Mode == EArcErosionMode::Droplet ? TEXT("Droplet") : TEXT("GridStreamPower"),
			ResX, ResY, ModeSeconds * 1000.0, TailSeconds * 1000.0);
		return Result;
	}

	FIoHash HashParams(const FParams& P)
	{
		FBlake3 Hasher;

		// NOTE: TFunction (owns a copy of the lambda), NOT TFunctionRef — a TFunctionRef bound to a
		// temporary lambda dangles after the initializing statement and crashes when later invoked.
		const TFunction<void(double)> HashDouble = [&Hasher](double V) -> void
		{
			Hasher.Update(reinterpret_cast<const uint8*>(&V), sizeof(double));
		};
		const TFunction<void(int32)> HashInt = [&Hasher](int32 V) -> void
		{
			Hasher.Update(reinterpret_cast<const uint8*>(&V), sizeof(int32));
		};
		const TFunction<void(uint8)> HashByte = [&Hasher](uint8 V) -> void
		{
			Hasher.Update(reinterpret_cast<const uint8*>(&V), sizeof(uint8));
		};
		const TFunction<void(const FVector2D&)> HashVec2 = [&HashDouble](const FVector2D& V) -> void
		{
			HashDouble(V.X);
			HashDouble(V.Y);
		};

		// Mode
		HashByte(static_cast<uint8>(P.Mode));

		// Grid
		HashInt(P.GridResX);
		HashInt(P.GridResY);
		HashDouble(P.CoverageX);
		HashDouble(P.CoverageY);
		HashDouble(P.SkirtSize);

		// Noise
		HashVec2(P.NoiseFrequency);
		HashVec2(P.NoiseTranslate);
		HashDouble(P.NoiseRotation);
		HashInt(P.Octaves);
		HashDouble(P.Lacunarity);
		HashDouble(P.Gain);
		HashDouble(P.Smoothness);
		HashDouble(P.Gamma);
		HashDouble(P.RedistributionExponent);
		HashDouble(P.RedistributionFudge);
		HashDouble(P.Height);

		// Erosion
		HashInt(P.OuterIterations);
		HashInt(P.ThermalSubsteps);
		HashDouble(P.RainAmount);
		HashDouble(P.DepositRate);
		HashDouble(P.SedimentCapacity);
		HashDouble(P.TalusAngleDeg);
		HashDouble(P.ThermalRate);
		HashInt(P.Seed);
		HashDouble(P.SedimentSmoothingRadiusCm);

		// Stream-power
		HashDouble(P.StreamPowerK);
		HashDouble(P.StreamPowerM);
		HashDouble(P.StreamPowerN);
		HashDouble(P.MaxErodePerStepCm);
		HashDouble(P.StreamPowerDepositRate);
		HashDouble(P.ChannelThreshold);

		// Droplet
		HashInt(P.NumDroplets);
		HashInt(P.DropletLifetime);
		HashDouble(P.DropletInertia);
		HashDouble(P.DropletCapacity);
		HashDouble(P.DropletErodeRate);
		HashDouble(P.DropletDepositRate);
		HashDouble(P.DropletEvaporation);
		HashDouble(P.DropletGravity);
		HashInt(P.DropletErosionRadius);
		HashDouble(P.DropletMinSlope);

		// Scale-invariance + channel depth
		HashDouble(P.ChannelDepthFraction);
		HashDouble(P.FeatureSizeCm);

		// InitialHeight — fold content only when set, so noise-path cache keys are unchanged.
		if (P.InitialHeight.IsValid())
		{
			// Helper for float fields (GridOrigin / CellDimensions are TVector2<float>).
			const TFunction<void(float)> HashFloat = [&Hasher](float V) -> void
			{
				Hasher.Update(reinterpret_cast<const uint8*>(&V), sizeof(float));
			};

			const int32 W = P.InitialHeight->GridValues.Width();
			const int32 H = P.InitialHeight->GridValues.Height();
			HashInt(W);
			HashInt(H);

			HashFloat(P.InitialHeight->GridOrigin.X);
			HashFloat(P.InitialHeight->GridOrigin.Y);
			HashFloat(P.InitialHeight->CellDimensions.X);
			HashFloat(P.InitialHeight->CellDimensions.Y);

			const TArray64<float>& Vals = P.InitialHeight->GridValues.GridValues();
			Hasher.Update(reinterpret_cast<const uint8*>(Vals.GetData()),
				static_cast<uint64>(Vals.Num()) * sizeof(float));
		}

		return FIoHash(Hasher.Finalize());
	}
}

// Process-wide in-memory cache for SimulateCached, fronting the DDC. Unique file-scoped namespace.
namespace ArcTerrainErosionCache
{
	static FCriticalSection GCacheLock;
	static TMap<FIoHash, TSharedPtr<const ArcTerrainErosion::FResult>> GCache;
	static TArray<FIoHash> GInsertOrder;
	static const int32 GCacheCap = 8;

	/**
	 * Insert under GCacheLock and evict oldest-inserted keys past the cap. Never evicts the just-inserted
	 * Key (it is at the back of GInsertOrder; we only ever pop the front, and stop at the cap). TSharedPtr
	 * keeps any concurrent reader's data alive past eviction.
	 */
	void InsertLocked(const FIoHash& Key, const TSharedPtr<const ArcTerrainErosion::FResult>& Value)
	{
		GCache.Add(Key, Value);
		GInsertOrder.Add(Key);

		// Collect the oldest keys to evict in one pass, then bulk-remove (two-pass; no RemoveAt-in-loop).
		int32 EvictCount = 0;
		while (GCache.Num() - EvictCount > GCacheCap && EvictCount < GInsertOrder.Num())
		{
			const FIoHash& Oldest = GInsertOrder[EvictCount];
			if (Oldest == Key)
			{
				// Never evict the key we just inserted; stop here.
				break;
			}
			GCache.Remove(Oldest);
			++EvictCount;
		}
		if (EvictCount > 0)
		{
			GInsertOrder.RemoveAt(0, EvictCount);
		}
	}
}

namespace ArcTerrainErosion
{
	TSharedPtr<const FResult> SimulateCached(const FParams& P)
	{
		using namespace ArcTerrainErosionCache;

		const FIoHash Key = HashParams(P);

		// Hold the lock across the whole operation. On a cold miss this serializes the (seconds-long) Simulate
		// for the FIRST caller only; concurrent tile workers then wait once and hit the warm cache. Intentional.
		FScopeLock Lock(&GCacheLock);

		if (const TSharedPtr<const FResult>* Existing = GCache.Find(Key))
		{
			UE_LOG(LogArcTerrainErosion, Display, TEXT("SimulateCached: in-memory cache HIT (%s)."), *LexToString(Key));
			return *Existing;
		}

		const FString KeyString = LexToString(Key);
		const FString DDCKey = FDerivedDataCacheInterface::BuildCacheKey(
			TEXT("ARCHILLSEROSION_"), ArcTerrainErosion::ArcErosionDDCVersion, *KeyString);

		TArray64<uint8> Bytes;
		if (GetDerivedDataCacheRef().GetSynchronous(*DDCKey, Bytes, TEXT("ArcHillsErosion")))
		{
			FResult Loaded;
			FMemoryReaderView Reader(MakeArrayView(Bytes.GetData(), Bytes.Num()));
			ArcTerrainErosionInternal::SerializeResult(Reader, Loaded);
			UE_LOG(LogArcTerrainErosion, Display, TEXT("SimulateCached: DDC HIT (%s)."), *KeyString);
			const TSharedPtr<const FResult> Shared = MakeShared<FResult>(MoveTemp(Loaded));
			InsertLocked(Key, Shared);
			return Shared;
		}

		// Cold miss: run the full sim, persist to DDC, cache, return.
		UE_LOG(LogArcTerrainErosion, Display,
			TEXT("SimulateCached: COLD MISS (%s) — running full Simulate synchronously (this is the slow path)."),
			*KeyString);
		FResult R = Simulate(P);

		TArray64<uint8> OutBytes;
		FMemoryWriter64 Writer(OutBytes);
		ArcTerrainErosionInternal::SerializeResult(Writer, R);
		GetDerivedDataCacheRef().Put(*DDCKey, OutBytes, TEXT("ArcHillsErosion"));

		const TSharedPtr<const FResult> Shared = MakeShared<FResult>(MoveTemp(R));
		InsertLocked(Key, Shared);
		return Shared;
	}
}
