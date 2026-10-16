// Copyright Lukasz Baran. All Rights Reserved.

#include "Catalog/ArcTownBuildingVisual.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SceneComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "ArcTownBuildingVisual"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownBuildingVisual, Log, All);

namespace ArcTownBuildingVisualPrivate
{
	/**
	 * The door convention is the asset's, not the actor's: the same art faces the same way whether it
	 * is a raw mesh or wrapped in a Blueprint, and every plot is measured against -Y.
	 */
	const FVector2D FallbackForwardXY(0.0, -1.0);

	FVector2D PlanarForward(const FQuat& Rotation, const FVector2D& Fallback)
	{
		const FVector Forward = Rotation.GetForwardVector();
		const FVector2D Planar(Forward.X, Forward.Y);
		return Planar.IsNearlyZero() ? Fallback : Planar.GetSafeNormal();
	}

	const USceneComponent* FindNativeComponent(const UClass& Class, const FName ComponentName)
	{
		const AActor* const DefaultActor = Cast<AActor>(Class.GetDefaultObject());
		if (DefaultActor == nullptr)
		{
			return nullptr;
		}

		for (const UActorComponent* const Component : DefaultActor->GetComponents())
		{
			const USceneComponent* const SceneComponent = Cast<USceneComponent>(Component);
			if (SceneComponent != nullptr && SceneComponent->GetFName() == ComponentName)
			{
				return SceneComponent;
			}
		}
		return nullptr;
	}

	/** Composed from relative rotations: these templates are never registered, so ComponentToWorld — and with it GetForwardVector() — is never updated. */
	FQuat ComposeNativeRotation(const USceneComponent& Component)
	{
		FQuat Rotation = Component.GetRelativeRotation().Quaternion();
		for (const USceneComponent* Parent = Component.GetAttachParent(); Parent != nullptr; Parent = Parent->GetAttachParent())
		{
			Rotation = Parent->GetRelativeRotation().Quaternion() * Rotation;
		}
		return Rotation;
	}

	const USCS_Node* FindConstructionScriptNode(const UClass& Class, const FName VariableName)
	{
		for (const UClass* Current = &Class; Current != nullptr; Current = Current->GetSuperClass())
		{
			const UBlueprintGeneratedClass* const GeneratedClass = Cast<UBlueprintGeneratedClass>(Current);
			if (GeneratedClass == nullptr || GeneratedClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}

			for (const USCS_Node* const Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node != nullptr && Node->GetVariableName() == VariableName)
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	void CollectNodeParents(const UClass& Class, TMap<const USCS_Node*, const USCS_Node*>& OutParentByChild)
	{
		for (const UClass* Current = &Class; Current != nullptr; Current = Current->GetSuperClass())
		{
			const UBlueprintGeneratedClass* const GeneratedClass = Cast<UBlueprintGeneratedClass>(Current);
			if (GeneratedClass == nullptr || GeneratedClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}

			for (const USCS_Node* const Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node == nullptr)
				{
					continue;
				}

				for (const USCS_Node* const Child : Node->GetChildNodes())
				{
					if (Child != nullptr)
					{
						OutParentByChild.Add(Child, Node);
					}
				}
			}
		}
	}

	bool IsEntityConfigObject(const UObject& Object)
	{
		for (const UClass* Class = Object.GetClass(); Class != nullptr; Class = Class->GetSuperClass())
		{
			if (Class->GetPathName() == TEXT("/Script/ArcMass.ArcMassEntityConfigAsset"))
			{
				return true;
			}
		}
		return false;
	}

	FQuat ComposeConstructionScriptRotation(const UClass& Class, const USCS_Node& Node)
	{
		TMap<const USCS_Node*, const USCS_Node*> ParentByChild;
		CollectNodeParents(Class, ParentByChild);

		FQuat Rotation = FQuat::Identity;
		TSet<const USCS_Node*> Visited;

		for (const USCS_Node* Current = &Node; Current != nullptr && !Visited.Contains(Current);)
		{
			Visited.Add(Current);

			const USceneComponent* const Template = Cast<USceneComponent>(Current->ComponentTemplate);
			if (Template != nullptr)
			{
				Rotation = Template->GetRelativeRotation().Quaternion() * Rotation;
			}

			const USCS_Node* const* const Parent = ParentByChild.Find(Current);
			if (Parent != nullptr)
			{
				Current = *Parent;
				continue;
			}

			if (Current->ParentComponentOrVariableName.IsNone())
			{
				break;
			}

			if (Current->bIsParentComponentNative)
			{
				const USceneComponent* const NativeParent =
					FindNativeComponent(Class, Current->ParentComponentOrVariableName);
				if (NativeParent != nullptr)
				{
					Rotation = ComposeNativeRotation(*NativeParent) * Rotation;
				}
				break;
			}

			Current = FindConstructionScriptNode(Class, Current->ParentComponentOrVariableName);
		}

		return Rotation;
	}
}

bool FArcTownBuildingVisual::IsValid(FText& OutMessage) const
{
	OutMessage = LOCTEXT("VisualAbstractBase", "Visual must name a concrete representation; the abstract base builds nothing.");
	return false;
}

FSoftObjectPath FArcTownBuildingVisual::GetAssetPath() const
{
	return FSoftObjectPath();
}

FVector2D FArcTownBuildingVisual::ResolveDoorForwardXY(const FName /*DoorSocketOverride*/) const
{
	return ArcTownBuildingVisualPrivate::FallbackForwardXY;
}

bool FArcTownBuildingVisual_StaticMesh::IsValid(FText& OutMessage) const
{
	OutMessage = FText::GetEmpty();

	if (Mesh.IsNull())
	{
		OutMessage = LOCTEXT("VisualMeshEmpty", "Mesh must not be empty.");
		return false;
	}

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	const FSoftObjectPath ResolvedPath = AssetRegistry.GetRedirectedObjectPath(Mesh.ToSoftObjectPath());

	FAssetData MeshAsset;
	const UE::AssetRegistry::EExists ExistsResult = AssetRegistry.TryGetAssetByObjectPath(ResolvedPath, MeshAsset);
	if (ExistsResult == UE::AssetRegistry::EExists::DoesNotExist)
	{
		OutMessage = FText::Format(
			LOCTEXT("VisualMeshMissing", "Mesh ({0}) does not resolve to an asset in the registry."),
			FText::FromString(Mesh.ToString()));
		return false;
	}

	if (ExistsResult == UE::AssetRegistry::EExists::Exists)
	{
		bool bNaniteEnabled = false;
		if (MeshAsset.GetTagValue(TEXT("NaniteEnabled"), bNaniteEnabled) && !bNaniteEnabled)
		{
			OutMessage = FText::Format(
				LOCTEXT("VisualMeshNoNanite", "Mesh ({0}) does not have Nanite enabled. Instanced static mesh components select LOD per component from the combined instance bounds, not per instance, so one nearby instance holds the whole batch at LOD0."),
				FText::FromString(MeshAsset.AssetName.ToString()));
		}
	}

	return true;
}

FSoftObjectPath FArcTownBuildingVisual_StaticMesh::GetAssetPath() const
{
	return Mesh.ToSoftObjectPath();
}

FVector2D FArcTownBuildingVisual_StaticMesh::ResolveDoorForwardXY(const FName DoorSocketOverride) const
{
	using namespace ArcTownBuildingVisualPrivate;

	const UStaticMesh* const LoadedMesh = Mesh.LoadSynchronous();
	if (LoadedMesh == nullptr || DoorSocketOverride.IsNone())
	{
		return FallbackForwardXY;
	}

	const UStaticMeshSocket* const Socket = LoadedMesh->FindSocket(DoorSocketOverride);
	if (Socket != nullptr)
	{
		const FVector SocketForward = FRotationMatrix(Socket->RelativeRotation).GetUnitAxis(EAxis::X);
		const FVector2D Planar(SocketForward.X, SocketForward.Y);
		if (!Planar.IsNearlyZero())
		{
			return Planar.GetSafeNormal();
		}
	}

	UE_LOGF(LogArcTownBuildingVisual, Warning,"Building mesh '%ls' has no usable door socket '%ls'; its door is taken to face -Y.",
		*LoadedMesh->GetName(),
		*DoorSocketOverride.ToString());
	return FallbackForwardXY;
}

bool FArcTownBuildingVisual_Actor::IsValid(FText& OutMessage) const
{
	OutMessage = FText::GetEmpty();

	if (ActorClass.IsNull())
	{
		OutMessage = LOCTEXT("VisualActorClassEmpty", "ActorClass must not be empty.");
		return false;
	}

	if (ActorClass.LoadSynchronous() == nullptr)
	{
		OutMessage = FText::Format(
			LOCTEXT("VisualActorClassMissing", "ActorClass ({0}) does not resolve to a class that loads."),
			FText::FromString(ActorClass.ToString()));
		return false;
	}

	return true;
}

FSoftObjectPath FArcTownBuildingVisual_Actor::GetAssetPath() const
{
	return ActorClass.ToSoftObjectPath();
}

FVector2D FArcTownBuildingVisual_Actor::ResolveDoorForwardXY(const FName DoorSocketOverride) const
{
	using namespace ArcTownBuildingVisualPrivate;

	UClass* const LoadedClass = ActorClass.LoadSynchronous();
	if (LoadedClass == nullptr || DoorSocketOverride.IsNone())
	{
		return FallbackForwardXY;
	}

	const USCS_Node* const Node = FindConstructionScriptNode(*LoadedClass, DoorSocketOverride);
	if (Node != nullptr)
	{
		return PlanarForward(ComposeConstructionScriptRotation(*LoadedClass, *Node), FallbackForwardXY);
	}

	const USceneComponent* const NativeComponent = FindNativeComponent(*LoadedClass, DoorSocketOverride);
	if (NativeComponent != nullptr)
	{
		return PlanarForward(ComposeNativeRotation(*NativeComponent), FallbackForwardXY);
	}

	UE_LOGF(LogArcTownBuildingVisual, Warning,"Building class '%ls' has no component named '%ls'; its door is taken to face -Y.",
		*LoadedClass->GetName(),
		*DoorSocketOverride.ToString());
	return FallbackForwardXY;
}

bool FArcTownBuildingVisual_MassEntity::IsValid(FText& OutMessage) const
{
	OutMessage = FText::GetEmpty();

	if (EntityConfig.IsNull())
	{
		OutMessage = LOCTEXT("VisualEntityConfigEmpty", "EntityConfig must not be empty.");
		return false;
	}

	const UObject* const LoadedConfig = EntityConfig.TryLoad();
	if (LoadedConfig == nullptr || !ArcTownBuildingVisualPrivate::IsEntityConfigObject(*LoadedConfig))
	{
		OutMessage = FText::Format(
			LOCTEXT("VisualEntityConfigMissing", "EntityConfig ({0}) does not resolve to a Mass entity config asset that loads."),
			FText::FromString(EntityConfig.ToString()));
		return false;
	}

	return true;
}

FSoftObjectPath FArcTownBuildingVisual_MassEntity::GetAssetPath() const
{
	return EntityConfig;
}

#undef LOCTEXT_NAMESPACE
