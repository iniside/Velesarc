/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "RecipeGraph/ArcRecipeGraphLayoutForce.h"

namespace ArcRecipeGraphLayoutForce_Private
{

/** Number of Fruchterman-Reingold integration steps. */
static constexpr int32 NumFRIterations = 200;

/**
 * Desired separation between two node centres during repulsion.
 * Added on top of the average diagonal of the two nodes.
 */
static constexpr float RepulsionPadding = 60.0f;

/**
 * Desired edge length = max node width in the model + this constant.
 * Nodes connected by an edge are pulled together when farther apart than this.
 */
static constexpr float AttractionEdgePad = 150.0f;

/**
 * Velocity–force blend factor per step (Lerp alpha).
 * Lower = smoother / slower convergence.
 */
static constexpr float VelocityBlend = 0.1f;

/** Velocity damping applied each step (multiplicative). */
static constexpr float VelocityDamping = 0.9f;

/**
 * Number of pairwise overlap-separation post-pass iterations.
 * Each iteration pushes pairs of overlapping rects apart.
 */
static constexpr int32 NumOverlapIterations = 15;

/**
 * Half-gap inflated onto each side of a node rect during overlap detection.
 * Matches the intra-layer gap constant (24 px / 2 = 12 each side).
 */
static constexpr float OverlapGapHalf = 12.0f;

/** Return true if the node should participate in the simulation. */
static bool IsActiveNode(const FArcRecipeLayoutNode& Node)
{
	return !Node.bDummy;
}

/** Return the diagonal (Size.Size()) of a node, floored to 1 to avoid zero. */
static float NodeDiagonal(const FArcRecipeLayoutNode& Node)
{
	const float Diag = Node.Size.Size();
	return (Diag > 0.0f) ? Diag : 1.0f;
}

/**
 * Compute the maximum node width among all active nodes.
 * Used to determine the desired attraction edge length.
 */
static float ComputeMaxNodeWidth(const TArray<FArcRecipeLayoutNode>& Nodes)
{
	float MaxWidth = 0.0f;
	for (const FArcRecipeLayoutNode& Node : Nodes)
	{
		if (IsActiveNode(Node) && Node.Size.X > MaxWidth)
		{
			MaxWidth = Node.Size.X;
		}
	}
	return MaxWidth;
}

/**
 * Convert top-left Pos to node centres.
 *
 * Centres[i] is the centre of node i (Pos + Size * 0.5).
 * Inactive (dummy) nodes are left at their top-left Pos.
 */
static void TopLeftToCentres(
	const TArray<FArcRecipeLayoutNode>& Nodes,
	TArray<FVector2D>& OutCentres)
{
	const int32 N = Nodes.Num();
	OutCentres.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		if (IsActiveNode(Nodes[i]))
		{
			OutCentres[i] = Nodes[i].Pos + Nodes[i].Size * 0.5f;
		}
		else
		{
			OutCentres[i] = Nodes[i].Pos;
		}
	}
}

/**
 * Write final centres back to top-left Pos in the model.
 */
static void CentresToTopLeft(
	const TArray<FVector2D>& Centres,
	TArray<FArcRecipeLayoutNode>& Nodes)
{
	const int32 N = Nodes.Num();
	for (int32 i = 0; i < N; ++i)
	{
		if (IsActiveNode(Nodes[i]))
		{
			Nodes[i].Pos = Centres[i] - Nodes[i].Size * 0.5f;
		}
	}
}

/**
 * Run 200 Fruchterman-Reingold integration steps.
 *
 * Repulsion: all pairs O(N²).  When centre distance < desired repulsion distance
 * (average of the two nodes' diagonals + RepulsionPadding), push both apart
 * along the centre-to-centre axis proportionally to the shortfall.
 * Zero-distance guard: if centres coincide, pick a deterministic axis based on
 * index parity (i even → push along +X, odd → push along +Y) so nodes always
 * separate.
 *
 * Attraction: per edge, pull both endpoints toward each other when the centre
 * distance exceeds the desired edge length (MaxNodeWidth + AttractionEdgePad),
 * proportional to the overshoot.
 *
 * Integration per active node each step:
 *   Velocity = Lerp(Velocity, Force, VelocityBlend)
 *   Centre   += Velocity
 *   Velocity *= VelocityDamping
 * A node is frozen when its velocity length drops below KINDA_SMALL_NUMBER.
 * The loop exits early when all active nodes are frozen.
 */
static void RunFruchtermanReingold(
	const TArray<FArcRecipeLayoutNode>& Nodes,
	const TArray<FArcRecipeLayoutEdge>& Edges,
	TArray<FVector2D>& Centres,
	float DesiredEdgeLength)
{
	const int32 N = Nodes.Num();

	TArray<FVector2D> Velocities;
	Velocities.SetNumZeroed(N);

	TArray<bool> Frozen;
	Frozen.SetNumZeroed(N);

	// Pre-mark inactive nodes as permanently frozen.
	for (int32 i = 0; i < N; ++i)
	{
		if (!IsActiveNode(Nodes[i]))
		{
			Frozen[i] = true;
		}
	}

	for (int32 Iter = 0; Iter < NumFRIterations; ++Iter)
	{
		// Check early-out: all active nodes frozen.
		bool bAnyActive = false;
		for (int32 i = 0; i < N; ++i)
		{
			if (!Frozen[i])
			{
				bAnyActive = true;
				break;
			}
		}
		if (!bAnyActive)
		{
			break;
		}

		TArray<FVector2D> Forces;
		Forces.SetNumZeroed(N);

		// --- Repulsion: all pairs ---
		for (int32 i = 0; i < N - 1; ++i)
		{
			if (!IsActiveNode(Nodes[i]))
			{
				continue;
			}

			for (int32 j = i + 1; j < N; ++j)
			{
				if (!IsActiveNode(Nodes[j]))
				{
					continue;
				}

				const float DesiredRepulsion =
					(NodeDiagonal(Nodes[i]) + NodeDiagonal(Nodes[j])) * 0.5f
					+ RepulsionPadding;

				FVector2D Delta = Centres[j] - Centres[i];
				const float DistSq = Delta.SizeSquared();
				const float Dist = FMath::Sqrt(DistSq);

				FVector2D Axis;
				if (Dist < KINDA_SMALL_NUMBER)
				{
					// Coincident centres — deterministic separation axis.
					Axis = ((i % 2) == 0) ? FVector2D(1.0f, 0.0f) : FVector2D(0.0f, 1.0f);
				}
				else
				{
					Axis = Delta / Dist;
				}

				if (Dist < DesiredRepulsion)
				{
					const float Shortfall = DesiredRepulsion - Dist;
					const FVector2D Push = Axis * (Shortfall * 0.5f);
					Forces[i] -= Push;
					Forces[j] += Push;
				}
			}
		}

		// --- Attraction: per edge ---
		for (const FArcRecipeLayoutEdge& Edge : Edges)
		{
			if (!Nodes.IsValidIndex(Edge.From) || !Nodes.IsValidIndex(Edge.To))
			{
				continue;
			}
			if (!IsActiveNode(Nodes[Edge.From]) || !IsActiveNode(Nodes[Edge.To]))
			{
				continue;
			}

			const FVector2D Delta = Centres[Edge.To] - Centres[Edge.From];
			const float Dist = Delta.Size();

			if (Dist > DesiredEdgeLength && Dist > KINDA_SMALL_NUMBER)
			{
				const float Overshoot = Dist - DesiredEdgeLength;
				const FVector2D Pull = (Delta / Dist) * (Overshoot * 0.5f);
				Forces[Edge.From] += Pull;
				Forces[Edge.To]   -= Pull;
			}
		}

		// --- Integration ---
		for (int32 i = 0; i < N; ++i)
		{
			if (Frozen[i])
			{
				continue;
			}

			Velocities[i] = FMath::Lerp(Velocities[i], Forces[i], VelocityBlend);
			Centres[i]   += Velocities[i];
			Velocities[i] *= VelocityDamping;

			if (Velocities[i].SizeSquared() < KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
			{
				Frozen[i] = true;
			}
		}
	}
}

/**
 * Post-pass: 15 iterations of pairwise overlap separation.
 *
 * Each node rect is inflated by OverlapGapHalf on each side before testing
 * overlap.  When two inflated rects overlap, both nodes are pushed apart along
 * the penetration axis with the smaller penetration depth (half the depth each).
 * Positions are top-left throughout this pass (centres are not used).
 */
static void SeparateOverlaps(TArray<FArcRecipeLayoutNode>& Nodes)
{
	const int32 N = Nodes.Num();

	for (int32 Iter = 0; Iter < NumOverlapIterations; ++Iter)
	{
		for (int32 i = 0; i < N - 1; ++i)
		{
			if (!IsActiveNode(Nodes[i]))
			{
				continue;
			}

			for (int32 j = i + 1; j < N; ++j)
			{
				if (!IsActiveNode(Nodes[j]))
				{
					continue;
				}

				// Inflated rect extents for node i (centre-based half-extents).
				const FVector2D HalfI = Nodes[i].Size * 0.5f + FVector2D(OverlapGapHalf, OverlapGapHalf);
				const FVector2D CentreI = Nodes[i].Pos + Nodes[i].Size * 0.5f;

				// Inflated rect extents for node j.
				const FVector2D HalfJ = Nodes[j].Size * 0.5f + FVector2D(OverlapGapHalf, OverlapGapHalf);
				const FVector2D CentreJ = Nodes[j].Pos + Nodes[j].Size * 0.5f;

				const FVector2D Delta = CentreJ - CentreI;
				const float PenX = (HalfI.X + HalfJ.X) - FMath::Abs(Delta.X);
				const float PenY = (HalfI.Y + HalfJ.Y) - FMath::Abs(Delta.Y);

				// No overlap (neither axis penetrates).
				if (PenX <= 0.0f || PenY <= 0.0f)
				{
					continue;
				}

				// Separate along the axis with smaller penetration.
				if (PenX < PenY)
				{
					const float HalfPen = PenX * 0.5f;
					const float SignX = (Delta.X >= 0.0f) ? 1.0f : -1.0f;
					Nodes[i].Pos.X -= SignX * HalfPen;
					Nodes[j].Pos.X += SignX * HalfPen;
				}
				else
				{
					const float HalfPen = PenY * 0.5f;
					const float SignY = (Delta.Y >= 0.0f) ? 1.0f : -1.0f;
					Nodes[i].Pos.Y -= SignY * HalfPen;
					Nodes[j].Pos.Y += SignY * HalfPen;
				}
			}
		}
	}
}

} // namespace ArcRecipeGraphLayoutForce_Private

// ----------------------------------------------------------------------------

void ArcRecipeGraphLayoutForce::ApplyForceDirected(FArcRecipeLayoutModel& Model)
{
	const int32 N = Model.Nodes.Num();
	if (N == 0)
	{
		return;
	}

	// Desired edge length is based on the widest node.
	const float MaxNodeWidth = ArcRecipeGraphLayoutForce_Private::ComputeMaxNodeWidth(Model.Nodes);
	const float DesiredEdgeLength = MaxNodeWidth + ArcRecipeGraphLayoutForce_Private::AttractionEdgePad;

	// Convert top-left positions to node centres for the FR simulation.
	TArray<FVector2D> Centres;
	ArcRecipeGraphLayoutForce_Private::TopLeftToCentres(Model.Nodes, Centres);

	// Run the Fruchterman-Reingold simulation on centres.
	ArcRecipeGraphLayoutForce_Private::RunFruchtermanReingold(
		Model.Nodes,
		Model.Edges,
		Centres,
		DesiredEdgeLength);

	// Write simulated centres back to top-left Pos.
	ArcRecipeGraphLayoutForce_Private::CentresToTopLeft(Centres, Model.Nodes);

	// Post-pass: pairwise overlap separation on actual rect extents.
	ArcRecipeGraphLayoutForce_Private::SeparateOverlaps(Model.Nodes);
}
