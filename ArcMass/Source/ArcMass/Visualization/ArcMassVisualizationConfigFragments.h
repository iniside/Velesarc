// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMassVisualizationConfigFragments.generated.h"

class UStaticMesh;

USTRUCT()
struct ARCMASS_API FArcMassStaticMeshConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcMassStaticMeshConfigFragment() = default;
	explicit FArcMassStaticMeshConfigFragment(const UStaticMesh* InMesh)
		: Mesh(InMesh)
	{}

	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	TWeakObjectPtr<const UStaticMesh> Mesh;
};

USTRUCT()
struct ARCMASS_API FArcMassVisualizationMeshConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	int64 CustomDepthStencilValue = 0;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	int32 TranslucencySortPriority = 0;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 HiddenInGame : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastShadow : 1 = true;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastShadowAsTwoSided : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastHiddenShadow : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 AffectDynamicIndirectLighting : 1 = true;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 AffectIndirectLightingWhileHidden : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 ReceivesDecals : 1 = true;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 NeverDistanceCull : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastFarShadow : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastInsetShadow : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 CastContactShadow : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Mesh)
	uint8 RenderCustomDepth : 1 = false;
};

template<>
struct TMassFragmentTraits<FArcMassVisualizationMeshConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

namespace ArcMass::Visualization
{
	inline FMassStaticMeshFragment ToEngineFragment(const FArcMassStaticMeshConfigFragment& Config)
	{
		FMassStaticMeshFragment Result;
		Result.Mesh = Config.Mesh;
		return Result;
	}

	inline FMassVisualizationMeshFragment ToEngineFragment(const FArcMassVisualizationMeshConfigFragment& Config)
	{
		FMassVisualizationMeshFragment Result;
		Result.CustomDepthStencilValue = Config.CustomDepthStencilValue;
		Result.TranslucencySortPriority = Config.TranslucencySortPriority;
		Result.HiddenInGame = Config.HiddenInGame;
		Result.CastShadow = Config.CastShadow;
		Result.CastShadowAsTwoSided = Config.CastShadowAsTwoSided;
		Result.CastHiddenShadow = Config.CastHiddenShadow;
		Result.AffectDynamicIndirectLighting = Config.AffectDynamicIndirectLighting;
		Result.AffectIndirectLightingWhileHidden = Config.AffectIndirectLightingWhileHidden;
		Result.ReceivesDecals = Config.ReceivesDecals;
		Result.NeverDistanceCull = Config.NeverDistanceCull;
		Result.CastFarShadow = Config.CastFarShadow;
		Result.CastInsetShadow = Config.CastInsetShadow;
		Result.CastContactShadow = Config.CastContactShadow;
		Result.RenderCustomDepth = Config.RenderCustomDepth;
		return Result;
	}
}
