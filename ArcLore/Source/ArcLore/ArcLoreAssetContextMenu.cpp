// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreAssetContextMenu.h"

#include "ArcLoreSourceControlProvider.h"
#include "ArcLoreSourceControlOperations.h"
#include "ArcLoreSourceControlState.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

namespace ArcLoreAssetContextMenuInternal
{
	/** Completion handler for the Lock (FCheckOut) context-menu dispatch.
	 *  Bound via CreateStatic — no captured refs, no dangling. On failure pops
	 *  the same dialog the panel uses (panel.cpp:2703-2708); refresh rides the
	 *  existing OnSourceControlStateChanged broadcast from the worker's UpdateStates. */
	static void OnLockOpComplete(const FSourceControlOperationRef& Op, ECommandResult::Type Result)
	{
		if (Result != ECommandResult::Succeeded)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				LOCTEXT("LockFailed", "Could not lock the selected file(s) — one or more are held by another user. See the Output Log for details."));
		}
	}

	/** Completion handler for the Unlock (FLoreReleaseLock) context-menu dispatch.
	 *  Bound via CreateStatic. Unlock failure is logged by the worker; no modal needed. */
	static void OnUnlockOpComplete(const FSourceControlOperationRef& Op, ECommandResult::Type Result)
	{
		// Worker already logs the lore error; broadcast refresh rides UpdateStates.
	}
}

void ArcLoreAssetContextMenu::Register()
{
	// Extend the asset "Revision Control" SUB-menu (added by the engine as
	// "SourceControlSubMenu" on ContentBrowser.AssetContextMenu), so Lock/Unlock sit
	// alongside the stock SC actions rather than at the top level. UToolMenus merges
	// name-registered sections into the delegate-populated submenu when it is generated.
	UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SourceControlSubMenu");

	// No section header label — the entries group quietly inside the Revision Control submenu.
	FToolMenuSection& Section = AssetContextMenu->FindOrAddSection("ArcLoreLocks");

	Section.AddDynamicEntry(
		"ArcLoreLockUnlock",
		FNewToolMenuSectionDelegate::CreateStatic([](FToolMenuSection& InSection)
		{
			// --- Context + early-out ---
			const UContentBrowserAssetContextMenuContext* Context =
				UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection);
			if (Context == nullptr || Context->SelectedAssets.IsEmpty())
			{
				return;
			}

			// --- Provider gate (instance-consistent: read auth off the ACTIVE provider) ---
			if (!ISourceControlModule::Get().IsEnabled())
			{
				return;
			}
			ISourceControlProvider& P = ISourceControlModule::Get().GetProvider();
			if (P.GetName() != FName("Lore"))
			{
				return;
			}
			FArcLoreSourceControlProvider& Lore = static_cast<FArcLoreSourceControlProvider&>(P);
			if (!Lore.IsAuthResolved())
			{
				return;
			}

			UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] menu gate: provider='%ls' authResolved=%d userId='%ls' selected=%d", *P.GetName().ToString(), Lore.IsAuthResolved() ? 1 : 0, *Lore.GetUserId(), Context->SelectedAssets.Num());

			// --- Build absolute file paths ---
			TArray<FString> PackageNames;
			PackageNames.Reserve(Context->SelectedAssets.Num());
			for (int32 i = 0; i < Context->SelectedAssets.Num(); ++i)
			{
				PackageNames.Add(Context->SelectedAssets[i].PackageName.ToString());
			}

			TArray<FString> Files = USourceControlHelpers::PackageFilenames(PackageNames);
			// Drop empty entries (non-content-root assets that couldn't be mapped)
			for (int32 i = Files.Num() - 1; i >= 0; --i)
			{
				if (Files[i].IsEmpty())
				{
					Files.RemoveAt(i, 1, EAllowShrinking::No);
				}
			}
			if (Files.IsEmpty())
			{
				return;
			}

			// --- Query cached state ---
			TArray<FSourceControlStateRef> States;
			Lore.GetState(Files, States, EStateCacheUsage::Use);

			// --- Classify files ---
			// LockableFiles: CanCheckout() AND NOT locked by another (IsCheckedOutOther)
			//   — do NOT require IsSourceControlled(); cold Unknown assets still show Lock;
			//   acquire fails server-side for non-repo files and the dialog surfaces it.
			// UnlockableFiles: IsCheckedOut() == locked by me.
			// OtherOwners: distinct owner ids from IsCheckedOutOther.
			TArray<FString> LockableFiles;
			TArray<FString> UnlockableFiles;
			TArray<FString> OtherOwners;

			for (int32 i = 0; i < Files.Num(); ++i)
			{
				const FSourceControlStateRef& State = States[i];

				const FArcLoreSourceControlState& ArcState = static_cast<const FArcLoreSourceControlState&>(State.Get());

				FString DiagWho;
				const bool bDiagOther = States[i]->IsCheckedOutOther(&DiagWho);
				UE_LOGF(LogSourceControl, Verbose,"ArcLore: [lockdiag] menu file='%ls' controlled=%d canLock=%d lockedByMe=%d byOther=%d otherOwner='%ls'", *Files[i], States[i]->IsSourceControlled() ? 1 : 0, ArcState.CanLock() ? 1 : 0, ArcState.IsLockedByMe() ? 1 : 0, bDiagOther ? 1 : 0, *DiagWho);

				FString Who;
				if (State->IsCheckedOutOther(&Who))
				{
					if (!Who.IsEmpty() && !OtherOwners.Contains(Who))
					{
						OtherOwners.Add(Who);
					}
				}
				else if (ArcState.IsLockedByMe())
				{
					UnlockableFiles.Add(Files[i]);
				}
				else if (ArcState.CanLock())
				{
					LockableFiles.Add(Files[i]);
				}
			}

			// --- Add "Lock" entry ---
			if (!LockableFiles.IsEmpty())
			{
				FToolUIAction LockAction;
				LockAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
					[LockableFiles](const FToolMenuContext& /*MenuContext*/)
					{
						ISourceControlModule::Get().GetProvider().Execute(
							ISourceControlOperation::Create<FCheckOut>(),
							nullptr,
							LockableFiles,
							EConcurrency::Asynchronous,
							FSourceControlOperationComplete::CreateStatic(
								&ArcLoreAssetContextMenuInternal::OnLockOpComplete));
					});

				InSection.AddMenuEntry(
					"ArcLoreLock",
					LOCTEXT("ArcLoreLockLabel", "Lock"),
					LOCTEXT("ArcLoreLockTooltip", "Acquire an exclusive Lore lock on the selected asset(s)."),
					FSlateIcon(),
					LockAction);
			}

			// --- Add "Unlock" entry ---
			if (!UnlockableFiles.IsEmpty())
			{
				FToolUIAction UnlockAction;
				UnlockAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
					[UnlockableFiles](const FToolMenuContext& /*MenuContext*/)
					{
						ISourceControlModule::Get().GetProvider().Execute(
							ISourceControlOperation::Create<FLoreReleaseLock>(),
							nullptr,
							UnlockableFiles,
							EConcurrency::Asynchronous,
							FSourceControlOperationComplete::CreateStatic(
								&ArcLoreAssetContextMenuInternal::OnUnlockOpComplete));
					});

				InSection.AddMenuEntry(
					"ArcLoreUnlock",
					LOCTEXT("ArcLoreUnlockLabel", "Unlock"),
					LOCTEXT("ArcLoreUnlockTooltip", "Release the Lore lock on the selected asset(s)."),
					FSlateIcon(),
					UnlockAction);
			}

			// --- Add disabled "Locked by <owner>" info entry ---
			if (!OtherOwners.IsEmpty())
			{
				FString OwnerList;
				for (int32 i = 0; i < OtherOwners.Num(); ++i)
				{
					if (i > 0)
					{
						OwnerList += TEXT(", ");
					}
					OwnerList += OtherOwners[i];
				}

				FToolUIAction DisabledAction;
				DisabledAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda(
					[](const FToolMenuContext& /*MenuContext*/)
					{
						return false;
					});

				InSection.AddMenuEntry(
					"ArcLoreLockedByOther",
					FText::Format(LOCTEXT("ArcLoreLockedByOtherLabel", "Locked by {0}"), FText::FromString(OwnerList)),
					LOCTEXT("ArcLoreLockedByOtherTooltip", "One or more selected assets are locked by another user."),
					FSlateIcon(),
					DisabledAction);
			}
		}));
}

#undef LOCTEXT_NAMESPACE
