#include "ArcTargetingModule.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingSubsystem.h"

#if UE_DEBUG_RENDERING_ENABLED
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#endif

DEFINE_LOG_CATEGORY(LogArcTargeting);

#define LOCTEXT_NAMESPACE "FArcTargetingModule"

#if UE_DEBUG_RENDERING_ENABLED

namespace ArcTargetingModulePrivate
{
	UWorld* PickGameWorld(UWorld* PreferredWorld)
	{
		// Prefer the world the console command was invoked against.
		if (PreferredWorld != nullptr && PreferredWorld->IsGameWorld())
		{
			return PreferredWorld;
		}
		// Fall back to any PIE/Game world. Picks the first valid one.
		if (GEngine != nullptr)
		{
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::Game)
				{
					if (UWorld* World = WorldContext.World())
					{
						return World;
					}
				}
			}
		}
		return PreferredWorld;
	}

	void SeedContextFromPlayer(UWorld* World, FArcTargetingContext& OutContext)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		}
		if (Pawn != nullptr)
		{
			OutContext.SourceActor = Pawn;
			OutContext.InstigatorActor = Pawn;
		}

		if (PC != nullptr)
		{
			FVector CamLocation;
			FRotator CamRotation;
			PC->GetPlayerViewPoint(CamLocation, CamRotation);
			OutContext.SourceLocation = CamLocation;
			OutContext.SourceDirection = CamRotation.Vector();
		}
		else if (Pawn != nullptr)
		{
			OutContext.SourceLocation = Pawn->GetActorLocation();
			OutContext.SourceDirection = Pawn->GetActorForwardVector();
		}
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED

void FArcTargetingModule::StartupModule()
{
#if UE_DEBUG_RENDERING_ENABLED
	PreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("arc.Targeting.PreviewPreset"),
		TEXT("Load a UArcTargetingPreset by asset path, run it once against the local player, and persist the visualization. Usage: arc.Targeting.PreviewPreset /Game/Path/MyPreset.MyPreset"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[this](const TArray<FString>& Args, UWorld* InWorld)
			{
				if (Args.Num() < 1)
				{
					UE_LOGF(LogConsoleResponse, Warning,"arc.Targeting.PreviewPreset requires a preset asset path.");
					return;
				}

				UWorld* World = ArcTargetingModulePrivate::PickGameWorld(InWorld);
				if (World == nullptr)
				{
					UE_LOGF(LogConsoleResponse, Warning,"arc.Targeting.PreviewPreset: no valid game world.");
					return;
				}

				const FSoftObjectPath PresetPath(Args[0]);
				UArcTargetingPreset* Preset = Cast<UArcTargetingPreset>(PresetPath.TryLoad());
				if (Preset == nullptr)
				{
					UE_LOGF(LogConsoleResponse, Warning,"arc.Targeting.PreviewPreset: failed to load %ls as UArcTargetingPreset.", *Args[0]);
					return;
				}

				UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(World);
				if (Sub == nullptr)
				{
					UE_LOGF(LogConsoleResponse, Warning,"arc.Targeting.PreviewPreset: targeting subsystem unavailable in this world.");
					return;
				}

				if (PreviewHandle.IsValid() && PreviewWorld.IsValid())
				{
					if (UArcTargetingSubsystem* PrevSub = UArcTargetingSubsystem::Get(PreviewWorld.Get()))
					{
						PrevSub->Release(PreviewHandle);
					}
				}

				FArcTargetingContext SeedCtx;
				ArcTargetingModulePrivate::SeedContextFromPlayer(World, SeedCtx);

				PreviewHandle = Sub->MakeRequest(Preset, SeedCtx);
				PreviewWorld = World;
				Sub->SetDebugMode(PreviewHandle, EArcTargetingDebugMode::Persist);
				Sub->ExecuteSync(PreviewHandle);
			}),
		ECVF_Default);

	ClearCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("arc.Targeting.ClearPreview"),
		TEXT("Release the active arc.Targeting.PreviewPreset request, if any."),
		FConsoleCommandDelegate::CreateLambda(
			[this]()
			{
				if (PreviewHandle.IsValid() && PreviewWorld.IsValid())
				{
					if (UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(PreviewWorld.Get()))
					{
						Sub->Release(PreviewHandle);
					}
				}
				PreviewHandle.Reset();
				PreviewWorld.Reset();
			}),
		ECVF_Default);

	ListCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("arc.Targeting.ListPresets"),
		TEXT("Log all UArcTargetingPreset assets the asset registry knows about."),
		FConsoleCommandDelegate::CreateLambda(
			[]()
			{
				IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
				if (AssetRegistry == nullptr)
				{
					UE_LOGF(LogConsoleResponse, Warning,"arc.Targeting.ListPresets: asset registry unavailable.");
					return;
				}

				TArray<FAssetData> Found;
				AssetRegistry->GetAssetsByClass(UArcTargetingPreset::StaticClass()->GetClassPathName(), Found, /*bSearchSubClasses=*/true);
				UE_LOGF(LogConsoleResponse, Log,"arc.Targeting.ListPresets: %d presets", Found.Num());
				for (const FAssetData& Data : Found)
				{
					UE_LOGF(LogConsoleResponse, Log,"  %ls", *Data.GetSoftObjectPath().ToString());
				}
			}),
		ECVF_Default);
#endif // UE_DEBUG_RENDERING_ENABLED
}

void FArcTargetingModule::ShutdownModule()
{
#if UE_DEBUG_RENDERING_ENABLED
	if (PreviewHandle.IsValid() && PreviewWorld.IsValid())
	{
		if (UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(PreviewWorld.Get()))
		{
			Sub->Release(PreviewHandle);
		}
		PreviewHandle.Reset();
		PreviewWorld.Reset();
	}

	if (PreviewCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(PreviewCommand);
		PreviewCommand = nullptr;
	}
	if (ClearCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ClearCommand);
		ClearCommand = nullptr;
	}
	if (ListCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ListCommand);
		ListCommand = nullptr;
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcTargetingModule, ArcTargeting)
