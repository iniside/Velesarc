// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Modules/ModuleInterface.h"

#include <atomic>

class FArcRmlFileInterface;
class FArcRmlUIThread;
struct FArcRmlCompileReport;

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcRmlContentRootChanged, const FArcRmlContentRoot& /*Root*/, bool /*bMounted*/);
#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE(FArcRmlDocumentCachesCleared);
#endif

class ARCRMLUI_API FArcRmlUIModule : public IModuleInterface
{
public:
	static FArcRmlUIModule& Get();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FArcRmlUIThread> GetOrStartUIThread();
	TSharedPtr<FArcRmlUIThread> GetUIThread() const;
	const FArcRmlContentRoots& GetContentRoots() const;
	FArcRmlContentRootChanged& OnContentRootChanged();

#if WITH_EDITOR
	FArcRmlDocumentCachesCleared& OnDocumentCachesCleared();
	bool CompileAll(TConstArrayView<FString> ExcludedPrefixes, FArcRmlCompileReport& OutReport);
	bool CaptureRegistryHash();
	bool GetCookRegistryHash(uint64& OutRegistryHash) const;
#endif

private:
	void HandleContentPathMounted(const FString& AssetPath, const FString& ContentPath);
	void HandleContentPathDismounted(const FString& AssetPath, const FString& ContentPath);
#if WITH_EDITOR
	void SetCookRegistryHash(uint64 RegistryHash);
	bool RunRmlSession(TFunctionRef<void(FArcRmlFileInterface&, const FArcRmlContentRoots&)> Body);
#endif

	TSharedPtr<FArcRmlUIThread> UIThread;
	FArcRmlContentRoots ContentRoots;
	FDelegateHandle ContentPathMountedHandle;
	FDelegateHandle ContentPathDismountedHandle;
	FArcRmlContentRootChanged ContentRootChanged;
#if WITH_EDITOR
	FArcRmlDocumentCachesCleared DocumentCachesCleared;
	std::atomic<uint64> CookRegistryHash{ 0 };
	std::atomic<bool> bHasCookRegistryHash{ false };
#endif
};
