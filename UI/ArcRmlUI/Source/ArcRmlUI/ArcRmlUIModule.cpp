// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlUIModule.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUISettings.h"
#include "Compile/ArcRmlDocumentCompiler.h"
#include "HAL/PlatformFileManager.h"
#include "IImageWrapperModule.h"
#include "Interfaces/ArcRmlFileInterface.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Interfaces/ArcRmlSystemInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Thread/ArcRmlUIThread.h"

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/StyleSheetBinary.h"

DEFINE_LOG_CATEGORY(LogArcRmlUI);

FArcRmlUIModule& FArcRmlUIModule::Get()
{
	return FModuleManager::LoadModuleChecked<FArcRmlUIModule>(TEXT("ArcRmlUI"));
}

void FArcRmlUIModule::StartupModule()
{
	ContentRoots = ArcRmlPaths::BuildContentRoots();
	ContentPathMountedHandle = FPackageName::OnContentPathMounted().AddRaw(this, &FArcRmlUIModule::HandleContentPathMounted);
	ContentPathDismountedHandle = FPackageName::OnContentPathDismounted().AddRaw(this, &FArcRmlUIModule::HandleContentPathDismounted);
}

void FArcRmlUIModule::ShutdownModule()
{
	FPackageName::OnContentPathMounted().Remove(ContentPathMountedHandle);
	FPackageName::OnContentPathDismounted().Remove(ContentPathDismountedHandle);
	if (UIThread.IsValid())
	{
		UIThread->Shutdown();
		UIThread.Reset();
	}
}

TSharedPtr<FArcRmlUIThread> FArcRmlUIModule::GetOrStartUIThread()
{
	check(IsInGameThread());
	if (UIThread.IsValid())
	{
		return UIThread;
	}

	if (IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		return nullptr;
	}

	FArcRmlUIThreadDependencies Dependencies;
	Dependencies.ImageWrapperModule = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	Dependencies.PlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
	Dependencies.ContentRoots = ContentRoots;

	const UArcRmlUISettings* Settings = GetDefault<UArcRmlUISettings>();
	for (const TPair<FName, FString>& GenericFamily : Settings->GenericFontFamilies)
	{
		Dependencies.GenericFontFamilies.Add(GenericFamily.Key.ToString(), GenericFamily.Value);
	}
	Dependencies.FallbackFontFamily = Settings->FallbackFontFamily;

	UIThread = MakeShared<FArcRmlUIThread>(MoveTemp(Dependencies));
	UIThread->Start();
	return UIThread;
}

TSharedPtr<FArcRmlUIThread> FArcRmlUIModule::GetUIThread() const
{
	check(IsInGameThread());
	return UIThread;
}

const FArcRmlContentRoots& FArcRmlUIModule::GetContentRoots() const
{
	check(IsInGameThread() || IsInParallelGameThread());
	return ContentRoots;
}

FArcRmlContentRootChanged& FArcRmlUIModule::OnContentRootChanged()
{
	return ContentRootChanged;
}

void FArcRmlUIModule::HandleContentPathMounted(const FString& AssetPath, const FString& ContentPath)
{
	check(IsInGameThread());
	FArcRmlContentRoot Root;
	if (!ArcRmlPaths::MakeContentRoot(AssetPath, Root) || !ContentRoots.Add(Root))
	{
		return;
	}
	if (UIThread.IsValid())
	{
		UIThread->UpdateContentRoot(Root, true);
	}
	ContentRootChanged.Broadcast(Root, true);
}

void FArcRmlUIModule::HandleContentPathDismounted(const FString& AssetPath, const FString& ContentPath)
{
	check(IsInGameThread());
	FString MountName = AssetPath;
	MountName.TrimCharInline(TEXT('/'), nullptr);
	const FArcRmlContentRoot* Found = ContentRoots.Find(MountName);
	if (!Found)
	{
		return;
	}
	const FArcRmlContentRoot Root = *Found;
	ContentRoots.Remove(MountName);
	if (UIThread.IsValid())
	{
		UIThread->UpdateContentRoot(Root, false);
	}
	ContentRootChanged.Broadcast(Root, false);
}

#if WITH_EDITOR
FArcRmlDocumentCachesCleared& FArcRmlUIModule::OnDocumentCachesCleared()
{
	return DocumentCachesCleared;
}

bool FArcRmlUIModule::CompileAll(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& OutReport)
{
	check(IsInGameThread());
	if (UIThread.IsValid())
	{
		if (!UIThread->CompileAllAndWait(ExcludedPrefixes, OutReport))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: the UI thread stopped before it compiled the documents");
			return false;
		}
		SetCookRegistryHash(OutReport.RegistryHash);
		return true;
	}
	// A commandlet never starts the UI thread, so the game thread owns RmlUi for the length of this compile.
	const bool bRan = RunRmlSession([ExcludedPrefixes, &OutReport](FArcRmlFileInterface& FileInterface, const FArcRmlContentRoots& Roots)
	{
		FArcRmlDocumentCompiler Compiler(FileInterface, Roots);
		Compiler.CompileAll(ExcludedPrefixes, OutReport);
	});
	if (bRan)
	{
		SetCookRegistryHash(OutReport.RegistryHash);
	}
	return bRan;
}

bool FArcRmlUIModule::CaptureRegistryHash()
{
	check(IsInGameThread());
	uint64 RegistryHash = 0;
	if (!RunRmlSession([&RegistryHash](FArcRmlFileInterface& FileInterface, const FArcRmlContentRoots& Roots)
	{
		RegistryHash = Rml::GetCompiledRegistryHash();
	}))
	{
		return false;
	}
	SetCookRegistryHash(RegistryHash);
	return true;
}

bool FArcRmlUIModule::GetCookRegistryHash(uint64& OutRegistryHash) const
{
	if (!bHasCookRegistryHash.load(std::memory_order_acquire))
	{
		return false;
	}
	OutRegistryHash = CookRegistryHash.load(std::memory_order_relaxed);
	return true;
}

void FArcRmlUIModule::SetCookRegistryHash(uint64 RegistryHash)
{
	CookRegistryHash.store(RegistryHash, std::memory_order_relaxed);
	bHasCookRegistryHash.store(true, std::memory_order_release);
}

bool FArcRmlUIModule::RunRmlSession(TFunctionRef<void(FArcRmlFileInterface&, const FArcRmlContentRoots&)> Body)
{
	check(IsInGameThread());
	if (UIThread.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: refusing a game-thread RmlUi session while the UI thread owns RmlUi");
		return false;
	}

	const FArcRmlContentRoots Roots = ContentRoots;
	FArcRmlSystemInterface SystemInterface;
	FArcRmlFileInterface FileInterface(FPlatformFileManager::Get().GetPlatformFile(), Roots);
	Rml::SetSystemInterface(&SystemInterface);
	Rml::SetFileInterface(&FileInterface);
	if (!Rml::Initialise())
	{
		Rml::SetFileInterface(nullptr);
		Rml::SetSystemInterface(nullptr);
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: Rml::Initialise failed for the game-thread RmlUi session");
		return false;
	}

	Body(FileInterface, Roots);

	Rml::Shutdown();
	return true;
}
#endif

IMPLEMENT_MODULE(FArcRmlUIModule, ArcRmlUI)
