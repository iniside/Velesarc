// Copyright Lukasz Baran. All Rights Reserved.

#include "Conversion/ArcBlueprintToEntityConfig.h"

#include "Conversion/ArcBlueprintMeshFlatten.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "MassEntityTraitBase.h"
#include "Mass/EntityFragments.h"
#include "MassAssortedFragmentsTrait.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "ArcBlueprintToEntityConfig"

namespace ArcBlueprintToEntityConfigPrivate
{
	FString MakeAssetName(const UBlueprint& Blueprint)
	{
		return FString::Printf(TEXT("EC_%s"), *Blueprint.GetName());
	}

	FString MakePackageName(const UBlueprint& Blueprint, const FString& AssetName)
	{
		const FString BlueprintPackageName = Blueprint.GetOutermost()->GetName();
		return FPackageName::GetLongPackagePath(BlueprintPackageName) / AssetName;
	}

	void ConvertEntries(
		TConstArrayView<FArcFlattenedMeshEntry> InEntries,
		TArray<FArcUnifiedStaticMeshEntry>& OutEntries)
	{
		OutEntries.Reset(InEntries.Num());
		for (const FArcFlattenedMeshEntry& Flattened : InEntries)
		{
			FArcUnifiedStaticMeshEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.StaticMesh = TSoftObjectPtr<UStaticMesh>(Flattened.Mesh);
			Entry.RelativeTransform = Flattened.RelativeTransform;
			Entry.MaterialOverrides.Reserve(Flattened.MaterialOverrides.Num());
			for (const TObjectPtr<UMaterialInterface>& Material : Flattened.MaterialOverrides)
			{
				Entry.MaterialOverrides.Add(TSoftObjectPtr<UMaterialInterface>(Material.Get()));
			}
		}
	}

	UArcMassEntityConfigAsset* ResolveTarget(
		const FString& PackageName,
		const FString& AssetName,
		bool& bOutCreated,
		TArray<FText>& OutProblems)
	{
		bOutCreated = false;

		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);

		UObject* Existing = FindObject<UObject>(nullptr, *ObjectPath);
		if (Existing == nullptr && bPackageExists)
		{
			Existing = LoadObject<UObject>(nullptr, *ObjectPath);
		}

		if (Existing != nullptr)
		{
			UArcMassEntityConfigAsset* const ExistingConfig = Cast<UArcMassEntityConfigAsset>(Existing);
			if (ExistingConfig == nullptr)
			{
				OutProblems.Add(FText::Format(
					LOCTEXT("ExistingAssetWrongClass", "'{0}' already exists and is a {1}, not an Arc Mass Entity Config."),
					FText::FromString(ObjectPath),
					FText::FromString(Existing->GetClass()->GetName())));
			}
			return ExistingConfig;
		}

		if (bPackageExists)
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("PackageWithoutAsset", "Package '{0}' exists but carries no asset named '{1}'."),
				FText::FromString(PackageName),
				FText::FromString(AssetName)));
			return nullptr;
		}

		UPackage* const Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("PackageCreationFailed", "Could not create package '{0}'."),
				FText::FromString(PackageName)));
			return nullptr;
		}
		Package->FullyLoad();

		UArcMassEntityConfigAsset* const NewConfig = NewObject<UArcMassEntityConfigAsset>(
			Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
		if (NewConfig == nullptr)
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("AssetCreationFailed", "Could not create asset '{0}'."),
				FText::FromString(AssetName)));
			return nullptr;
		}

		bOutCreated = true;
		return NewConfig;
	}

	bool IsTraitOwnedElsewhere(
		const UArcMassEntityConfigAsset& Target,
		TSubclassOf<UMassEntityTraitBase> TraitClass,
		const FString& AssetName,
		TArray<FText>& OutProblems)
	{
		// FindTrait ascends the Parent config chain without mutating anything
		// (MassEntityConfigAsset.cpp:36-63) using the exact match AddTrait resolves with (:309).
		const UMassEntityTraitBase* const TraitInstance = Target.FindTrait(TraitClass, /*bExactMatch=*/true);
		if (TraitInstance == nullptr)
		{
			return false;
		}

		const UMassEntityConfigAsset* const OwningAsset = TraitInstance->GetTypedOuter<UMassEntityConfigAsset>();
		if (OwningAsset == &Target)
		{
			return false;
		}

		OutProblems.Add(FText::Format(
			LOCTEXT("TraitOwnedElsewhere", "The trait '{0}' reached from '{1}' is carried by '{2}'; overwriting it would repoint every config deriving from that parent."),
			FText::FromString(TraitClass->GetName()),
			FText::FromString(AssetName),
			FText::FromString(GetNameSafe(OwningAsset))));
		return true;
	}

	bool EnsureTransformFragment(UMassAssortedFragmentsTrait& Trait, TArray<FText>& OutProblems)
	{
		// The trait's Fragments list is protected in a MinimalAPI engine class
		// (MassAssortedFragmentsTrait.h:22), so reflection is the only writer available here.
		FArrayProperty* const FragmentsProperty = CastField<FArrayProperty>(
			UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Fragments")));
		FStructProperty* const InnerProperty =
			FragmentsProperty != nullptr ? CastField<FStructProperty>(FragmentsProperty->Inner) : nullptr;
		if (InnerProperty == nullptr || InnerProperty->Struct != FInstancedStruct::StaticStruct())
		{
			OutProblems.Add(LOCTEXT("AssortedFragmentsLayoutChanged", "The Assorted Fragments trait no longer exposes a Fragments array of instanced structs."));
			return false;
		}

		TArray<FInstancedStruct>& Fragments =
			*FragmentsProperty->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(&Trait);
		for (const FInstancedStruct& Fragment : Fragments)
		{
			if (Fragment.GetScriptStruct() == FTransformFragment::StaticStruct())
			{
				return true;
			}
		}

		Trait.Modify();
		Fragments.Add(FInstancedStruct::Make<FTransformFragment>());
		return true;
	}
}

UArcMassEntityConfigAsset* ConvertBlueprintToEntityConfig(
	const UBlueprint& Blueprint,
	TArray<FText>& OutProblems)
{
	using namespace ArcBlueprintToEntityConfigPrivate;

	UClass* const GeneratedClass = Blueprint.GeneratedClass;
	if (GeneratedClass == nullptr || !GeneratedClass->IsChildOf(AActor::StaticClass()))
	{
		OutProblems.Add(FText::Format(
			LOCTEXT("NotAnActorBlueprint", "'{0}' does not generate an Actor class."),
			FText::FromString(Blueprint.GetName())));
		return nullptr;
	}

	TArray<FArcFlattenedMeshEntry> FlattenedEntries;
	if (!FlattenBlueprintStaticMeshes(*GeneratedClass, FlattenedEntries, OutProblems))
	{
		return nullptr;
	}

	const FString AssetName = MakeAssetName(Blueprint);
	const FString PackageName = MakePackageName(Blueprint, AssetName);

	bool bCreated = false;
	UArcMassEntityConfigAsset* const Target = ResolveTarget(PackageName, AssetName, bCreated, OutProblems);
	if (Target == nullptr)
	{
		return nullptr;
	}

	const UMassEntityTraitBase* const AnyVisualizationTrait =
		Target->FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass(), /*bExactMatch=*/false);
	if (AnyVisualizationTrait != nullptr
		&& AnyVisualizationTrait->GetClass() != UArcUnifiedMeshVisualizationTrait::StaticClass())
	{
		OutProblems.Add(FText::Format(
			LOCTEXT("SubclassTraitPresent", "'{0}' carries '{1}', a subclass of the unified mesh visualization trait; adding the base trait beside it would emit both mesh lists."),
			FText::FromString(AssetName),
			FText::FromString(AnyVisualizationTrait->GetClass()->GetName())));
		return nullptr;
	}

	const bool bMeshTraitOwnedElsewhere = IsTraitOwnedElsewhere(
		*Target, UArcUnifiedMeshVisualizationTrait::StaticClass(), AssetName, OutProblems);
	const bool bFragmentsTraitOwnedElsewhere = IsTraitOwnedElsewhere(
		*Target, UMassAssortedFragmentsTrait::StaticClass(), AssetName, OutProblems);
	if (bMeshTraitOwnedElsewhere || bFragmentsTraitOwnedElsewhere)
	{
		return nullptr;
	}

	FScopedTransaction Transaction(LOCTEXT("ConvertBlueprint", "Convert Blueprint to Mass Entity Config"));

	UArcUnifiedMeshVisualizationTrait* const MeshTrait = CastChecked<UArcUnifiedMeshVisualizationTrait>(
		Target->AddTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	UMassAssortedFragmentsTrait* const FragmentsTrait = CastChecked<UMassAssortedFragmentsTrait>(
		Target->AddTrait(UMassAssortedFragmentsTrait::StaticClass()));

	TArray<FArcUnifiedStaticMeshEntry> Entries;
	ConvertEntries(FlattenedEntries, Entries);

	Target->Modify();
	MeshTrait->Modify();
	MeshTrait->SetStaticMeshEntries(Entries);

	if (!EnsureTransformFragment(*FragmentsTrait, OutProblems))
	{
		Transaction.Cancel();
		return nullptr;
	}

	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Target);
	}
	Target->MarkPackageDirty();

	return Target;
}

#undef LOCTEXT_NAMESPACE
