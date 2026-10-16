// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class IArcEditorEntityDrawContributor;
class IArcMassPreviewContributor;

enum class EArcPreviewContext : uint8
{
	None      = 0,
	Viewport  = 1 << 0,
	Thumbnail = 1 << 1,
};
ENUM_CLASS_FLAGS(EArcPreviewContext);

// Register and unregister only from a module's StartupModule/ShutdownModule, on the game thread.
class ARCMASSEDITOR_API FArcMassEditorVisualizationRegistry
{
public:
	using FPreviewFactory = TFunction<TUniquePtr<IArcMassPreviewContributor>()>;

	static FArcMassEditorVisualizationRegistry& Get();

	void RegisterPreviewContributor(FName Id, EArcPreviewContext Contexts, FPreviewFactory Factory);
	void UnregisterPreviewContributor(FName Id);
	void CreatePreviewContributors(EArcPreviewContext Context, TArray<TUniquePtr<IArcMassPreviewContributor>>& OutContributors) const;

	void NotifyPreviewViewportOpened();
	void NotifyPreviewViewportClosed();

	void RegisterDrawContributor(FName Id, TSharedRef<IArcEditorEntityDrawContributor> Contributor);
	void UnregisterDrawContributor(FName Id);
	void GetDrawContributors(TArray<TSharedPtr<IArcEditorEntityDrawContributor>>& OutContributors) const;

	FSimpleMulticastDelegate& OnDrawContributorsChanged();

private:
	struct FPreviewEntry
	{
		FName Id;
		EArcPreviewContext Contexts = EArcPreviewContext::None;
		FPreviewFactory Factory;
	};

	struct FDrawEntry
	{
		FName Id;
		TSharedPtr<IArcEditorEntityDrawContributor> Contributor;
	};

	TArray<FPreviewEntry> PreviewEntries;
	int32 OpenPreviewViewportCount = 0;

	TArray<FDrawEntry> DrawEntries;
	FSimpleMulticastDelegate DrawContributorsChangedDelegate;
};
