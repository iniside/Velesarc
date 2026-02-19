// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpatialWorldData.h"
#include "DrawDebugHelpers.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcSpatialWorldData, Log, All);
DEFINE_LOG_CATEGORY(LogArcSpatialWorldData);

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<bool> CVarDebugDrawSpatialWorldData(
	TEXT("arc.mass.DebugDrawSpatialWorldData"),
	false,
	TEXT("Draw occupied cells of all spatial world data layers."),
	ECVF_Cheat);
#endif

// ---------------------------------------------------------------------------
// FArcSpatialDataLayer
// ---------------------------------------------------------------------------

FIntVector FArcSpatialDataLayer::WorldToGrid(const FVector& WorldPos) const
{
	const float Inv = 1.f / Config.CellSize;
	return FIntVector(
		FMath::FloorToInt(WorldPos.X * Inv),
		FMath::FloorToInt(WorldPos.Y * Inv),
		Config.bIs2D ? 0 : FMath::FloorToInt(WorldPos.Z * Inv)
	);
}

FVector FArcSpatialDataLayer::GridToWorld(const FIntVector& GridCoords) const
{
	return FVector(
		(static_cast<float>(GridCoords.X) + 0.5f) * Config.CellSize,
		(static_cast<float>(GridCoords.Y) + 0.5f) * Config.CellSize,
		Config.bIs2D ? 0.f : (static_cast<float>(GridCoords.Z) + 0.5f) * Config.CellSize
	);
}

void FArcSpatialDataLayer::SetCellData(const FIntVector& GridCoords, const FInstancedStruct& Data)
{
	Cells.FindOrAdd(GridCoords) = Data;
}

const FInstancedStruct* FArcSpatialDataLayer::GetCellData(const FIntVector& GridCoords) const
{
	return Cells.Find(GridCoords);
}

bool FArcSpatialDataLayer::RemoveCellData(const FIntVector& GridCoords)
{
	return Cells.Remove(GridCoords) > 0;
}

TArray<FIntVector> FArcSpatialDataLayer::GetAllOccupiedCells() const
{
	TArray<FIntVector> Result;
	Cells.GetKeys(Result);
	return Result;
}

TArray<FIntVector> FArcSpatialDataLayer::QueryCellsInRadius(const FVector& Center, float Radius) const
{
	TArray<FIntVector> Result;
	const float RadiusSq = Radius * Radius;
	const int32 CellSpan = FMath::CeilToInt(Radius / Config.CellSize);
	const FIntVector CenterGrid = WorldToGrid(Center);

	const int32 ZMin = Config.bIs2D ? 0 : -CellSpan;
	const int32 ZMax = Config.bIs2D ? 0 : CellSpan;

	for (int32 X = -CellSpan; X <= CellSpan; ++X)
	{
		for (int32 Y = -CellSpan; Y <= CellSpan; ++Y)
		{
			for (int32 Z = ZMin; Z <= ZMax; ++Z)
			{
				const FIntVector Coords = CenterGrid + FIntVector(X, Y, Z);
				if (Cells.Contains(Coords))
				{
					const FVector CellCenter = GridToWorld(Coords);
					if (FVector::DistSquared(Center, CellCenter) <= RadiusSq)
					{
						Result.Add(Coords);
					}
				}
			}
		}
	}
	return Result;
}

TArray<FIntVector> FArcSpatialDataLayer::QueryCellsInBox(const FBox& Box) const
{
	TArray<FIntVector> Result;
	const FIntVector MinGrid = WorldToGrid(Box.Min);
	const FIntVector MaxGrid = WorldToGrid(Box.Max);

	for (int32 X = MinGrid.X; X <= MaxGrid.X; ++X)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; ++Y)
		{
			const int32 ZMin = Config.bIs2D ? 0 : MinGrid.Z;
			const int32 ZMax = Config.bIs2D ? 0 : MaxGrid.Z;
			for (int32 Z = ZMin; Z <= ZMax; ++Z)
			{
				const FIntVector Coords(X, Y, Z);
				if (Cells.Contains(Coords))
				{
					Result.Add(Coords);
				}
			}
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// UArcSpatialWorldDataSubsystem
// ---------------------------------------------------------------------------

void UArcSpatialWorldDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UArcSpatialWorldDataSettings* Settings = GetDefault<UArcSpatialWorldDataSettings>();
	if (Settings)
	{
		for (const FArcSpatialDataLayerConfig& LayerConfig : Settings->Layers)
		{
			if (LayerConfig.LayerTag.IsValid())
			{
				CreateLayer(LayerConfig);
			}
		}
	}
}

void UArcSpatialWorldDataSubsystem::Deinitialize()
{
	Layers.Empty();
	Super::Deinitialize();
}

void UArcSpatialWorldDataSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
	if (CVarDebugDrawSpatialWorldData.GetValueOnGameThread())
	{
		DebugDraw();
	}
#endif
}

TStatId UArcSpatialWorldDataSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcSpatialWorldDataSubsystem, STATGROUP_Tickables);
}

bool UArcSpatialWorldDataSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// -- Layer management -------------------------------------------------------

FArcSpatialDataLayer* UArcSpatialWorldDataSubsystem::CreateLayer(const FArcSpatialDataLayerConfig& Config)
{
	if (!Config.LayerTag.IsValid())
	{
		UE_LOG(LogArcSpatialWorldData, Warning, TEXT("CreateLayer called with invalid LayerTag."));
		return nullptr;
	}

	if (Layers.Contains(Config.LayerTag))
	{
		UE_LOG(LogArcSpatialWorldData, Warning, TEXT("Layer '%s' already exists."), *Config.LayerTag.ToString());
		return &Layers[Config.LayerTag];
	}

	FArcSpatialDataLayer& Layer = Layers.Add(Config.LayerTag);
	Layer.Config = Config;
	UE_LOG(LogArcSpatialWorldData, Log, TEXT("Created spatial data layer '%s' (CellSize=%.0f, 2D=%d)"),
		*Config.LayerTag.ToString(), Config.CellSize, Config.bIs2D);
	return &Layer;
}

FArcSpatialDataLayer* UArcSpatialWorldDataSubsystem::GetLayer(FGameplayTag LayerTag)
{
	return Layers.Find(LayerTag);
}

const FArcSpatialDataLayer* UArcSpatialWorldDataSubsystem::GetLayer(FGameplayTag LayerTag) const
{
	return Layers.Find(LayerTag);
}

void UArcSpatialWorldDataSubsystem::RemoveLayer(FGameplayTag LayerTag)
{
	Layers.Remove(LayerTag);
}

// -- Blueprint-callable API -------------------------------------------------

void UArcSpatialWorldDataSubsystem::SetDataFromStruct(FGameplayTag LayerTag, FVector Location, const FInstancedStruct& Data)
{
	FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		UE_LOG(LogArcSpatialWorldData, Warning, TEXT("SetDataFromStruct: layer '%s' not found."), *LayerTag.ToString());
		return;
	}
	Layer->SetCellData(Layer->WorldToGrid(Location), Data);
}

FInstancedStruct UArcSpatialWorldDataSubsystem::GetDataFromStruct(FGameplayTag LayerTag, FVector Location) const
{
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return FInstancedStruct();
	}
	const FInstancedStruct* Cell = Layer->GetCellData(Layer->WorldToGrid(Location));
	return Cell ? *Cell : FInstancedStruct();
}

bool UArcSpatialWorldDataSubsystem::HasData(FGameplayTag LayerTag, FVector Location) const
{
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return false;
	}
	return Layer->GetCellData(Layer->WorldToGrid(Location)) != nullptr;
}

bool UArcSpatialWorldDataSubsystem::RemoveData(FGameplayTag LayerTag, FVector Location)
{
	FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return false;
	}
	return Layer->RemoveCellData(Layer->WorldToGrid(Location));
}

// -- Query API --------------------------------------------------------------

void UArcSpatialWorldDataSubsystem::QueryDataInRadius(FGameplayTag LayerTag, FVector Center, float Radius, TArray<FArcSpatialDataQueryResult>& OutResults) const
{
	OutResults.Reset();
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return;
	}

	const TArray<FIntVector> CellCoords = Layer->QueryCellsInRadius(Center, Radius);
	OutResults.Reserve(CellCoords.Num());
	for (const FIntVector& Coords : CellCoords)
	{
		const FInstancedStruct* CellData = Layer->GetCellData(Coords);
		if (CellData)
		{
			FArcSpatialDataQueryResult& Entry = OutResults.AddDefaulted_GetRef();
			Entry.GridCoords = Coords;
			Entry.WorldCenter = Layer->GridToWorld(Coords);
			Entry.Data = *CellData;
		}
	}
}

void UArcSpatialWorldDataSubsystem::QueryDataInBox(FGameplayTag LayerTag, FBox Box, TArray<FArcSpatialDataQueryResult>& OutResults) const
{
	OutResults.Reset();
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return;
	}

	const TArray<FIntVector> CellCoords = Layer->QueryCellsInBox(Box);
	OutResults.Reserve(CellCoords.Num());
	for (const FIntVector& Coords : CellCoords)
	{
		const FInstancedStruct* CellData = Layer->GetCellData(Coords);
		if (CellData)
		{
			FArcSpatialDataQueryResult& Entry = OutResults.AddDefaulted_GetRef();
			Entry.GridCoords = Coords;
			Entry.WorldCenter = Layer->GridToWorld(Coords);
			Entry.Data = *CellData;
		}
	}
}

void UArcSpatialWorldDataSubsystem::FindCellsMeetingPredicate(FGameplayTag LayerTag, TFunctionRef<bool(const FIntVector&, const FInstancedStruct&)> Predicate, TArray<FArcSpatialDataQueryResult>& OutResults) const
{
	OutResults.Reset();
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return;
	}

	for (const auto& Pair : Layer->Cells)
	{
		if (Predicate(Pair.Key, Pair.Value))
		{
			FArcSpatialDataQueryResult& Entry = OutResults.AddDefaulted_GetRef();
			Entry.GridCoords = Pair.Key;
			Entry.WorldCenter = Layer->GridToWorld(Pair.Key);
			Entry.Data = Pair.Value;
		}
	}
}

// -- Bulk / utility ---------------------------------------------------------

void UArcSpatialWorldDataSubsystem::ClearLayer(FGameplayTag LayerTag)
{
	FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (Layer)
	{
		Layer->Cells.Empty();
	}
}

int32 UArcSpatialWorldDataSubsystem::GetOccupiedCellCount(FGameplayTag LayerTag) const
{
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	return Layer ? Layer->Cells.Num() : 0;
}

// -- Debug ------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
void UArcSpatialWorldDataSubsystem::DebugDraw() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 ColorIndex = 0;
	static const FColor LayerColors[] = { FColor::Red, FColor::Green, FColor::Blue, FColor::Yellow, FColor::Cyan, FColor::Magenta, FColor::Orange };
	constexpr int32 NumColors = UE_ARRAY_COUNT(LayerColors);

	for (const auto& Pair : Layers)
	{
		const FArcSpatialDataLayer& Layer = Pair.Value;
		const FColor Color = LayerColors[ColorIndex % NumColors];
		const float HalfSize = Layer.Config.CellSize * 0.5f;
		const FVector Extent(HalfSize, HalfSize, Layer.Config.bIs2D ? 10.f : HalfSize);

		for (const auto& CellPair : Layer.Cells)
		{
			const FVector Center = Layer.GridToWorld(CellPair.Key);
			DrawDebugBox(World, Center, Extent, Color, false, -1.f, 0, 2.f);
		}

		++ColorIndex;
	}
}
#endif
