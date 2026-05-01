// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "ArcKnowledgeTagBitmask.h"
#include "ArcKnowledgeTypes.h"

// ============================================================================
// FArcKnowledgeRTreeBounds — 3D axis-aligned bounding box
// ============================================================================

struct FArcKnowledgeRTreeBounds
{
	FVector Min = FVector(MAX_dbl);
	FVector Max = FVector(-MAX_dbl);

	void Expand(const FVector& Point)
	{
		Min = Min.ComponentMin(Point);
		Max = Max.ComponentMax(Point);
	}

	void Expand(const FArcKnowledgeRTreeBounds& Other)
	{
		Min = Min.ComponentMin(Other.Min);
		Max = Max.ComponentMax(Other.Max);
	}

	double Volume() const
	{
		const FVector Size = Max - Min;
		if (Size.X <= 0.0 || Size.Y <= 0.0 || Size.Z <= 0.0)
		{
			return 0.0;
		}
		return Size.X * Size.Y * Size.Z;
	}

	double VolumeIncrease(const FArcKnowledgeRTreeBounds& Other) const
	{
		FArcKnowledgeRTreeBounds Merged;
		Merged.Min = Min.ComponentMin(Other.Min);
		Merged.Max = Max.ComponentMax(Other.Max);
		return Merged.Volume() - Volume();
	}

	bool IntersectsSphere(const FVector& Center, double RadiusSq) const
	{
		// Closest point on AABB to sphere center
		const FVector Closest(
			FMath::Clamp(Center.X, Min.X, Max.X),
			FMath::Clamp(Center.Y, Min.Y, Max.Y),
			FMath::Clamp(Center.Z, Min.Z, Max.Z)
		);
		return FVector::DistSquared(Center, Closest) <= RadiusSq;
	}

	bool IntersectsBox(const FArcKnowledgeRTreeBounds& Other) const
	{
		return Min.X <= Other.Max.X && Max.X >= Other.Min.X
			&& Min.Y <= Other.Max.Y && Max.Y >= Other.Min.Y
			&& Min.Z <= Other.Max.Z && Max.Z >= Other.Min.Z;
	}

	bool IsValid() const
	{
		return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
	}
};

// ============================================================================
// FArcKnowledgeRTreeLeafEntry — data stored in leaf nodes
// ============================================================================

struct FArcKnowledgeRTreeLeafEntry
{
	FMassEntityHandle Entity;
	FVector Position = FVector::ZeroVector;
	FArcKnowledgeTagBitmask TagBitmask;
	FArcKnowledgeHandle Handle;
};

// ============================================================================
// FArcKnowledgeRTree — 3D R-tree with per-leaf tag bitmask filtering
// ============================================================================

class ARCKNOWLEDGE_API FArcKnowledgeRTree
{
public:
	static constexpr int32 MinChildren = 4;
	static constexpr int32 MaxChildren = 8;

	FArcKnowledgeRTree();
	~FArcKnowledgeRTree();

	// Non-copyable
	FArcKnowledgeRTree(const FArcKnowledgeRTree&) = delete;
	FArcKnowledgeRTree& operator=(const FArcKnowledgeRTree&) = delete;

	// Movable
	FArcKnowledgeRTree(FArcKnowledgeRTree&& Other) noexcept;
	FArcKnowledgeRTree& operator=(FArcKnowledgeRTree&& Other) noexcept;

	/** Insert a single entry into the tree. */
	void Insert(const FArcKnowledgeRTreeLeafEntry& Entry);

	/** Remove the entry matching the given entity. Returns true if found. */
	bool Remove(FMassEntityHandle Entity);

	/** Sphere query with tag bitmask filtering. Appends to OutEntries. */
	void QuerySphere(const FVector& Center, double Radius, const FArcKnowledgeTagBitmask& RequiredMask, TArray<FArcKnowledgeRTreeLeafEntry>& OutEntries) const;

	/** Box query with tag bitmask filtering. Appends to OutEntries. */
	void QueryBox(const FArcKnowledgeRTreeBounds& Box, const FArcKnowledgeTagBitmask& RequiredMask, TArray<FArcKnowledgeRTreeLeafEntry>& OutEntries) const;

	/** Bulk-load entries using Sort-Tile-Recursive packing. Clears existing tree first. */
	void BulkLoad(TArray<FArcKnowledgeRTreeLeafEntry>& Entries);

	/** Remove all entries and free all nodes. */
	void Clear();

	int32 GetEntryCount() const { return EntryCount; }
	int32 GetNodeCount() const { return NodeCount; }

private:
	struct FNode
	{
		FArcKnowledgeRTreeBounds Bounds;
		FArcKnowledgeTagBitmask AggregatedMask;
		bool bIsLeaf = true;

		TArray<FArcKnowledgeRTreeLeafEntry, TInlineAllocator<MaxChildren>> LeafEntries;
		TArray<FNode*, TInlineAllocator<MaxChildren>> Children;

		void RecomputeBounds();
		void RecomputeAggregatedMask();
	};

	/** Find the leaf node with minimum volume increase. Builds path from root to leaf. */
	FNode* ChooseLeaf(FNode* CurrentNode, const FArcKnowledgeRTreeBounds& EntryBounds, TArray<FNode*>& OutPath) const;

	/** Quadratic split: redistribute entries/children of an overflowed node. Returns new sibling. */
	FNode* SplitNode(FNode* Node);

	/** Recursively delete a node and all its children. */
	void DeleteNode(FNode* Node);

	/** Build STR subtree from a slice of entries at a given depth. Returns root of subtree. */
	FNode* BuildSTR(TArray<FArcKnowledgeRTreeLeafEntry>& Entries, int32 Start, int32 Count, int32 Depth);

	FNode* Root = nullptr;
	int32 EntryCount = 0;
	int32 NodeCount = 0;
};
