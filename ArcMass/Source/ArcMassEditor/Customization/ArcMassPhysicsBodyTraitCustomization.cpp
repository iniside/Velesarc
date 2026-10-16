// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyTraitCustomization.h"

#include "ArcBodyInstanceExpander.h"
#include "ArcMass/Physics/ArcCollisionGeometry.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "MassEntityConfigAsset.h"
#include "Misc/MessageDialog.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ReferenceSkeleton.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcMassPhysicsBodyTraitCustomization"

namespace ArcMassPhysicsBodyTraitExtractInternal
{
	// Per-axis primitive scale yields ill-defined extents under non-uniform scale once
	// rotation is involved. Treat scale as uniform if the three components are within
	// this tolerance; otherwise refuse to bake that entry and warn.
	constexpr double UniformScaleTolerance = 1.0e-3;

	static bool IsApproximatelyUniform(const FVector& Scale)
	{
		const double Max = FMath::Max3(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), FMath::Abs(Scale.Z));
		const double Min = FMath::Min3(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), FMath::Abs(Scale.Z));
		return (Max - Min) <= UniformScaleTolerance;
	}

	static const UMassEntityConfigAsset* GetOwningConfig(const UArcMassPhysicsBodyTrait* Trait)
	{
		return Trait ? Trait->GetTypedOuter<UMassEntityConfigAsset>() : nullptr;
	}

	static const UArcUnifiedMeshVisualizationTrait* FindUnifiedMeshTrait(const UMassEntityConfigAsset* Config)
	{
		if (Config == nullptr)
		{
			return nullptr;
		}
		return Cast<UArcUnifiedMeshVisualizationTrait>(Config->FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	}

	static bool MeshHasExtractableCollision(const UStaticMesh* Mesh)
	{
		if (Mesh == nullptr)
		{
			return false;
		}
		const UBodySetup* BodySetup = Mesh->GetBodySetup();
		if (BodySetup == nullptr)
		{
			return false;
		}
		const FKAggregateGeom& Geom = BodySetup->AggGeom;
		return Geom.BoxElems.Num() > 0 || Geom.SphereElems.Num() > 0 || Geom.SphylElems.Num() > 0 || Geom.ConvexElems.Num() > 0;
	}

	// Bakes Transform into a freshly-appended set of primitives. Box/Sphyl/Sphere store
	// position+rotation explicitly; ConvexElem stores its own Transform which we combine
	// with Transform and then bake into VertexData. Returns true on success.
	static bool AppendAggGeomTransformed(const FKAggregateGeom& Src, const FTransform& Transform, FKAggregateGeom& Out, FString& OutWarnReason)
	{
		const FVector Scale = Transform.GetScale3D();
		const bool bUniformScale = IsApproximatelyUniform(Scale);
		if (!bUniformScale)
		{
			OutWarnReason = FString::Printf(TEXT("non-uniform transform scale (%.3f, %.3f, %.3f)"), Scale.X, Scale.Y, Scale.Z);
			return false;
		}
		const double UniformScale = FMath::Abs(Scale.X);

		const FTransform& RelativeTransform = Transform;

		// Boxes
		for (const FKBoxElem& SrcBox : Src.BoxElems)
		{
			FKBoxElem Baked = SrcBox;
			const FTransform LocalTM(SrcBox.Rotation, SrcBox.Center, FVector::OneVector);
			const FTransform Combined = LocalTM * RelativeTransform;
			Baked.Center = Combined.GetLocation();
			Baked.Rotation = Combined.Rotator();
			Baked.X = SrcBox.X * UniformScale;
			Baked.Y = SrcBox.Y * UniformScale;
			Baked.Z = SrcBox.Z * UniformScale;
			Out.BoxElems.Add(MoveTemp(Baked));
		}

		// Spheres
		for (const FKSphereElem& SrcSphere : Src.SphereElems)
		{
			FKSphereElem Baked = SrcSphere;
			Baked.Center = RelativeTransform.TransformPosition(SrcSphere.Center);
			Baked.Radius = SrcSphere.Radius * UniformScale;
			Out.SphereElems.Add(MoveTemp(Baked));
		}

		// Capsules (Sphyls)
		for (const FKSphylElem& SrcSphyl : Src.SphylElems)
		{
			FKSphylElem Baked = SrcSphyl;
			const FTransform LocalTM(SrcSphyl.Rotation, SrcSphyl.Center, FVector::OneVector);
			const FTransform Combined = LocalTM * RelativeTransform;
			Baked.Center = Combined.GetLocation();
			Baked.Rotation = Combined.Rotator();
			Baked.Radius = SrcSphyl.Radius * UniformScale;
			Baked.Length = SrcSphyl.Length * UniformScale;
			Out.SphylElems.Add(MoveTemp(Baked));
		}

		// Convexes — combine existing per-elem transform with RelativeTransform, then
		// BakeTransformToVerts to apply it to VertexData and reset Transform to identity.
		for (const FKConvexElem& SrcConvex : Src.ConvexElems)
		{
			FKConvexElem Baked = SrcConvex;
			Baked.SetTransform(SrcConvex.GetTransform() * RelativeTransform);
			Baked.BakeTransformToVerts();
			Baked.UpdateElemBox();
			Out.ConvexElems.Add(MoveTemp(Baked));
		}

		return true;
	}

	static bool AppendMeshAggGeomTransformed(const UStaticMesh* Mesh, const FTransform& RelativeTransform, FKAggregateGeom& Out, FString& OutWarnReason)
	{
		if (!MeshHasExtractableCollision(Mesh))
		{
			OutWarnReason = TEXT("no simple collision primitives");
			return false;
		}
		return AppendAggGeomTransformed(Mesh->GetBodySetup()->AggGeom, RelativeTransform, Out, OutWarnReason);
	}

	/** Component-space ref-pose transform of a bone: compose the local ref pose up the parent chain. */
	static FTransform GetComponentSpaceRefBoneTransform(const FReferenceSkeleton& RefSkel, int32 BoneIndex)
	{
		FTransform Result = FTransform::Identity;
		const TArray<FTransform>& RefPose = RefSkel.GetRefBonePose();
		int32 Idx = BoneIndex;
		while (Idx != INDEX_NONE)
		{
			Result = Result * RefPose[Idx];
			Idx = RefSkel.GetParentIndex(Idx);
		}
		return Result;
	}

	static bool SkinnedAssetHasExtractableCollision(const USkinnedAsset* SkinnedAsset)
	{
		const UPhysicsAsset* PhysicsAsset = SkinnedAsset ? SkinnedAsset->GetPhysicsAsset() : nullptr;
		if (PhysicsAsset == nullptr)
		{
			return false;
		}
		for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (BodySetup == nullptr)
			{
				continue;
			}
			const FKAggregateGeom& Geom = BodySetup->AggGeom;
			if (Geom.BoxElems.Num() > 0 || Geom.SphereElems.Num() > 0 || Geom.SphylElems.Num() > 0 || Geom.ConvexElems.Num() > 0)
			{
				return true;
			}
		}
		return false;
	}

	// Bakes every PhysicsAsset body of the skinned asset into Out: per body, the
	// ref-pose component-space bone transform composed with RelativeTransform.
	// Returns true if at least one body was appended.
	static bool AppendSkinnedPhysicsAssetAggGeom(const USkinnedAsset* SkinnedAsset, const FTransform& RelativeTransform, FKAggregateGeom& Out, FString& OutWarnReason)
	{
		const UPhysicsAsset* PhysicsAsset = SkinnedAsset ? SkinnedAsset->GetPhysicsAsset() : nullptr;
		if (PhysicsAsset == nullptr)
		{
			OutWarnReason = TEXT("skinned asset has no PhysicsAsset");
			return false;
		}

		const FReferenceSkeleton& RefSkel = SkinnedAsset->GetRefSkeleton();
		bool bAppendedAny = false;
		for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (BodySetup == nullptr)
			{
				continue;
			}
			const int32 BoneIndex = RefSkel.FindBoneIndex(BodySetup->BoneName);
			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}

			const FTransform BoneRefPoseTM = GetComponentSpaceRefBoneTransform(RefSkel, BoneIndex);
			FString BodyWarnReason;
			if (AppendAggGeomTransformed(BodySetup->AggGeom, BoneRefPoseTM * RelativeTransform, Out, BodyWarnReason))
			{
				bAppendedAny = true;
			}
		}

		if (!bAppendedAny)
		{
			OutWarnReason = TEXT("no PhysicsAsset body produced primitives (empty bodies, unknown bones, or non-uniform scale)");
		}
		return bAppendedAny;
	}

	static bool CanExtract(const UArcMassPhysicsBodyTrait* Trait, FText* OutDisabledReason = nullptr)
	{
		if (Trait == nullptr)
		{
			if (OutDisabledReason) *OutDisabledReason = LOCTEXT("Disabled_NoTrait", "Single-trait selection required");
			return false;
		}

		const UMassEntityConfigAsset* Config = GetOwningConfig(Trait);
		if (Config == nullptr)
		{
			if (OutDisabledReason) *OutDisabledReason = LOCTEXT("Disabled_NoConfig", "Trait is not inside a UMassEntityConfigAsset");
			return false;
		}

		if (const UArcUnifiedMeshVisualizationTrait* Unified = FindUnifiedMeshTrait(Config))
		{
			for (const FArcUnifiedMeshEntry& Entry : Unified->GetMeshEntries())
			{
				if (!Entry.IsValidEntry())
				{
					continue;
				}
				if (Entry.IsSkinnedEntry()
					? SkinnedAssetHasExtractableCollision(Entry.SkinnedAsset.LoadSynchronous())
					: MeshHasExtractableCollision(Entry.StaticMesh.LoadSynchronous()))
				{
					return true;
				}
			}
			if (OutDisabledReason) *OutDisabledReason = LOCTEXT("Disabled_UnifiedNone", "No unified entry has extractable collision (mesh simple collision or PhysicsAsset bodies)");
			return false;
		}

		if (OutDisabledReason) *OutDisabledReason = LOCTEXT("Disabled_NoSibling", "No sibling mesh trait found in this config");
		return false;
	}

	// Returns the number of primitives appended into Out. Out is cleared on entry.
	static int32 PerformExtract(const UArcMassPhysicsBodyTrait* Trait, FKAggregateGeom& Out)
	{
		Out = FKAggregateGeom{};

		const UMassEntityConfigAsset* Config = GetOwningConfig(Trait);
		if (Config == nullptr)
		{
			return 0;
		}

		if (const UArcUnifiedMeshVisualizationTrait* Unified = FindUnifiedMeshTrait(Config))
		{
			const TArray<FArcUnifiedMeshEntry>& Entries = Unified->GetMeshEntries();
			for (int32 Idx = 0; Idx < Entries.Num(); ++Idx)
			{
				const FArcUnifiedMeshEntry& Entry = Entries[Idx];
				if (!Entry.IsValidEntry())
				{
					continue;
				}

				FString WarnReason;
				const bool bAppended = Entry.IsSkinnedEntry()
					? AppendSkinnedPhysicsAssetAggGeom(Entry.SkinnedAsset.LoadSynchronous(), Entry.RelativeTransform, Out, WarnReason)
					: AppendMeshAggGeomTransformed(Entry.StaticMesh.LoadSynchronous(), Entry.RelativeTransform, Out, WarnReason);
				if (!bAppended)
				{
					UE_LOGF(LogMass, Warning,"ExtractCollisionFromMesh: skipped unified entry %d on '%ls' — %ls",
						Idx,
						*Unified->GetName(),
						*WarnReason);
				}
			}
		}

		return Out.BoxElems.Num() + Out.SphereElems.Num() + Out.SphylElems.Num() + Out.ConvexElems.Num();
	}
} // namespace ArcMassPhysicsBodyTraitExtractInternal

void FArcMassPhysicsBodyTraitCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> BodyTemplateHandle = DetailBuilder.GetProperty(TEXT("BodyTemplate"));
	DetailBuilder.HideProperty(BodyTemplateHandle);

	IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics");

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	TWeakObjectPtr<UArcMassPhysicsBodyTrait> WeakTrait;
	if (SelectedObjects.Num() == 1)
	{
		WeakTrait = Cast<UArcMassPhysicsBodyTrait>(SelectedObjects[0].Get());
	}

	IDetailLayoutBuilder* DetailBuilderPtr = &DetailBuilder;

	PhysicsCategory.AddCustomRow(LOCTEXT("ExtractCollisionFilter", "Extract Collision"))
		.WholeRowContent()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("ExtractFromMesh", "Extract Collision From Sibling Mesh"))
			.ToolTipText_Lambda([WeakTrait]()
			{
				FText Reason;
				if (ArcMassPhysicsBodyTraitExtractInternal::CanExtract(WeakTrait.Get(), &Reason))
				{
					return LOCTEXT("ExtractFromMesh_TooltipEnabled",
						"Replace this trait's CollisionGeometry with the simple-collision primitives from the unified mesh trait found in the owning UMassEntityConfigAsset.");
				}
				return Reason;
			})
			.IsEnabled_Lambda([WeakTrait]()
			{
				return ArcMassPhysicsBodyTraitExtractInternal::CanExtract(WeakTrait.Get());
			})
			.OnClicked_Lambda([WeakTrait, DetailBuilderPtr]() -> FReply
			{
				UArcMassPhysicsBodyTrait* Trait = WeakTrait.Get();
				if (Trait == nullptr)
				{
					return FReply::Handled();
				}

				if (Trait->HasCollisionGeometry())
				{
					const EAppReturnType::Type Choice = FMessageDialog::Open(
						EAppMsgType::OkCancel,
						LOCTEXT("ReplaceConfirm", "This will replace the trait's existing collision geometry with primitives extracted from the sibling mesh trait. Continue?"),
						LOCTEXT("ReplaceConfirmTitle", "Replace Collision Geometry"));
					if (Choice != EAppReturnType::Ok)
					{
						return FReply::Handled();
					}
				}

				FScopedTransaction Transaction(LOCTEXT("ExtractCollisionTransaction", "Extract Collision From Mesh"));
				Trait->Modify();

				FKAggregateGeom Extracted;
				const int32 PrimitiveCount = ArcMassPhysicsBodyTraitExtractInternal::PerformExtract(Trait, Extracted);
				if (PrimitiveCount <= 0)
				{
					Transaction.Cancel();
					FMessageDialog::Open(
						EAppMsgType::Ok,
						LOCTEXT("ExtractFailed", "Extraction produced no primitives — see Output Log for per-entry reasons."),
						LOCTEXT("ExtractFailedTitle", "Extract Collision From Mesh"));
					return FReply::Handled();
				}

				Trait->GetMutableCollisionGeometry().AggGeom = MoveTemp(Extracted);

				FProperty* Prop = FindFProperty<FProperty>(UArcMassPhysicsBodyTrait::StaticClass(), TEXT("CollisionGeometry"));
				FPropertyChangedEvent ChangeEvent(Prop, EPropertyChangeType::ValueSet);
				Trait->PostEditChangeProperty(ChangeEvent);

				if (DetailBuilderPtr)
				{
					DetailBuilderPtr->ForceRefreshDetails();
				}

				UE_LOGF(LogMass, Log,"ExtractCollisionFromMesh: populated '%ls' with %d primitive(s)", *Trait->GetName(), PrimitiveCount);
				return FReply::Handled();
			})
		];

	PhysicsCategory.AddCustomBuilder(MakeShared<FArcBodyInstanceNodeBuilder>(BodyTemplateHandle));
}

#undef LOCTEXT_NAMESPACE
