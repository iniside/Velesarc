// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRenderModule.h"

#include "ArcMassRenderLogs.h"

DEFINE_LOG_CATEGORY(LogArcMassVisDiag);

#if WITH_EDITOR
#include "Engine/World.h"
#include "Mesh/ArcRenderStateHelper.h"
#include "Components/ComponentInterfaces.h"
#include "MassEntitySubsystem.h"
#include "Mesh/ArcRenderStateHelperRegistry.h"

namespace ArcMassRenderModule_Private
{
	static void OnWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
	{
		if (World)
		{
			UE::ArcMassRender::StripWorldRenderStateProxies(*World);
		}
	}
} // namespace ArcMassRenderModule_Private

void FArcMassRenderModule::StartupModule()
{
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&ArcMassRenderModule_Private::OnWorldCleanup);

	IPrimitiveComponent::AddImplementer(TComponentInterfaceImplementation<IPrimitiveComponent>(
		UMassEntitySubsystem::StaticClass(),
		[](UObject* Object) -> TArray<IPrimitiveComponent*>
		{
			TArray<IPrimitiveComponent*> Result;
			const UMassEntitySubsystem* Subsystem = CastChecked<UMassEntitySubsystem>(Object);
			UWorld* World = Subsystem->GetWorld();
			if (IsValid(World))
			{
				ArcRenderStateHelperRegistry::ResolveForWorld(World, Result);
			}
			return Result;
		}));

	ArcRenderStateHelperRegistry::StartMaterialInvalidationWatcher();
}

void FArcMassRenderModule::ShutdownModule()
{
	ArcRenderStateHelperRegistry::StopMaterialInvalidationWatcher();
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
	IPrimitiveComponent::RemoveImplementer(UMassEntitySubsystem::StaticClass());
}
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FArcMassRenderModule, ArcMassRender)
