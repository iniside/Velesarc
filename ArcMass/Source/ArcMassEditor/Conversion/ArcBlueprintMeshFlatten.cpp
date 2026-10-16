// Copyright Lukasz Baran. All Rights Reserved.

#include "Conversion/ArcBlueprintMeshFlatten.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "ArcBlueprintMeshFlatten"

namespace ArcBlueprintMeshFlattenPrivate
{
	struct FClassScope
	{
		UBlueprintGeneratedClass* LeafGeneratedClass = nullptr;
		const USceneComponent* NativeRootComponent = nullptr;
		TArray<USCS_Node*> AllNodes;
		TMap<const USCS_Node*, USCS_Node*> ParentByChild;
		TMap<const USceneComponent*, USCS_Node*> NodeByTemplate;
		TMap<FName, USCS_Node*> NodeByVariableName;
	};

	struct FChainDiagnostics
	{
		FChainDiagnostics(const FName InComponentName, TArray<FText>& InProblems)
			: ComponentName(InComponentName)
			, Problems(InProblems)
		{
		}

		FName ComponentName;
		TArray<FText>& Problems;
		bool bShearReported = false;
		bool bAbsoluteReported = false;
	};

	FTransform LocalTransform(const USceneComponent& Component)
	{
		return FTransform(
			Component.GetRelativeRotation(),
			Component.GetRelativeLocation(),
			Component.GetRelativeScale3D());
	}

	bool IsNonUniformScale(const FVector& Scale)
	{
		return !FMath::IsNearlyEqual(Scale.X, Scale.Y) || !FMath::IsNearlyEqual(Scale.X, Scale.Z);
	}

	USceneComponent* ActualSceneTemplate(const USCS_Node& Node, UBlueprintGeneratedClass* LeafGeneratedClass)
	{
		if (LeafGeneratedClass != nullptr)
		{
			return Cast<USceneComponent>(Node.GetActualComponentTemplate(LeafGeneratedClass));
		}
		return Cast<USceneComponent>(Node.ComponentTemplate);
	}

	const USceneComponent* EffectiveRootComponent(const AActor& DefaultActor)
	{
		const USceneComponent* const DeclaredRoot = DefaultActor.GetRootComponent();
		if (DeclaredRoot != nullptr)
		{
			return DeclaredRoot;
		}

		for (UActorComponent* const Component : DefaultActor.GetComponents())
		{
			const USceneComponent* const SceneComponent = Cast<USceneComponent>(Component);
			if (SceneComponent != nullptr
				&& SceneComponent->GetAttachParent() == nullptr
				&& SceneComponent->CreationMethod == EComponentCreationMethod::Native)
			{
				return SceneComponent;
			}
		}

		return nullptr;
	}

	/** Inherited components live in the ancestors' construction scripts, so the whole class chain is walked; the leaf's own node list is only its added components. */
	void BuildClassScope(const UClass& ActorClass, FClassScope& OutScope)
	{
		OutScope.LeafGeneratedClass = Cast<UBlueprintGeneratedClass>(const_cast<UClass*>(&ActorClass));

		const AActor* const DefaultActor = Cast<AActor>(ActorClass.GetDefaultObject());
		OutScope.NativeRootComponent = (DefaultActor != nullptr) ? EffectiveRootComponent(*DefaultActor) : nullptr;

		for (UClass* Current = const_cast<UClass*>(&ActorClass); Current != nullptr; Current = Current->GetSuperClass())
		{
			UBlueprintGeneratedClass* const GeneratedClass = Cast<UBlueprintGeneratedClass>(Current);
			if (GeneratedClass == nullptr || GeneratedClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}

			for (USCS_Node* const Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node == nullptr)
				{
					continue;
				}

				OutScope.AllNodes.Add(Node);

				const USceneComponent* const Template = ActualSceneTemplate(*Node, OutScope.LeafGeneratedClass);
				if (Template != nullptr)
				{
					OutScope.NodeByTemplate.Add(Template, Node);
				}

				if (!Node->GetVariableName().IsNone())
				{
					OutScope.NodeByVariableName.Add(Node->GetVariableName(), Node);
				}

				for (USCS_Node* const Child : Node->GetChildNodes())
				{
					if (Child != nullptr)
					{
						OutScope.ParentByChild.Add(Child, Node);
					}
				}
			}
		}
	}

	void ResolveParent(
		const FClassScope& Scope,
		const USCS_Node& Node,
		USCS_Node*& OutParentNode,
		USceneComponent*& OutParentTemplate)
	{
		OutParentNode = nullptr;
		OutParentTemplate = nullptr;

		USCS_Node* const* const ParentNode = Scope.ParentByChild.Find(&Node);
		if (ParentNode != nullptr)
		{
			OutParentNode = *ParentNode;
			OutParentTemplate = ActualSceneTemplate(**ParentNode, Scope.LeafGeneratedClass);
			return;
		}

		if (Node.ParentComponentOrVariableName.IsNone())
		{
			return;
		}

		OutParentTemplate = (Scope.LeafGeneratedClass != nullptr)
			? Node.GetParentComponentTemplate(Scope.LeafGeneratedClass)
			: nullptr;
		if (OutParentTemplate != nullptr)
		{
			USCS_Node* const* const TemplateNode = Scope.NodeByTemplate.Find(OutParentTemplate);
			OutParentNode = (TemplateNode != nullptr) ? *TemplateNode : nullptr;
			return;
		}

		if (!Node.bIsParentComponentNative)
		{
			USCS_Node* const* const NamedNode = Scope.NodeByVariableName.Find(Node.ParentComponentOrVariableName);
			if (NamedNode != nullptr)
			{
				OutParentNode = *NamedNode;
				OutParentTemplate = ActualSceneTemplate(**NamedNode, Scope.LeafGeneratedClass);
			}
		}
	}

	/** An unresolvable socket yields an identity link because the engine's SetupAttachment (SCS_Node.cpp:168) leaves the component at the parent's origin and still renders it. */
	FTransform SocketLink(
		const USceneComponent* ParentTemplate,
		const FName AttachToName,
		FChainDiagnostics& Diagnostics)
	{
		if (AttachToName.IsNone())
		{
			return FTransform::Identity;
		}

		const UStaticMeshComponent* const ParentMeshComponent = Cast<UStaticMeshComponent>(ParentTemplate);
		const UStaticMesh* const ParentMesh = (ParentMeshComponent != nullptr) ? ParentMeshComponent->GetStaticMesh() : nullptr;
		const UStaticMeshSocket* const Socket = (ParentMesh != nullptr) ? ParentMesh->FindSocket(AttachToName) : nullptr;
		if (Socket == nullptr)
		{
			Diagnostics.Problems.Add(FText::Format(
				LOCTEXT("UnresolvedSocket", "'{0}' attaches to socket '{1}', which its parent does not provide; it is placed at the parent's origin."),
				FText::FromName(Diagnostics.ComponentName),
				FText::FromName(AttachToName)));
			return FTransform::Identity;
		}

		return FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
	}

	void NoteShear(const FTransform& Accumulated, const FVector& AncestorScale, FChainDiagnostics& Diagnostics)
	{
		if (Diagnostics.bShearReported || !IsNonUniformScale(AncestorScale) || Accumulated.GetRotation().IsIdentity(UE_KINDA_SMALL_NUMBER))
		{
			return;
		}

		Diagnostics.bShearReported = true;
		Diagnostics.Problems.Add(FText::Format(
			LOCTEXT("ShearApproximated", "'{0}' sits under a rotated, non-uniformly scaled parent; the flattened transform approximates the shear."),
			FText::FromName(Diagnostics.ComponentName)));
	}

	void NoteAbsoluteChannels(const USceneComponent& Component, FChainDiagnostics& Diagnostics)
	{
		if (Diagnostics.bAbsoluteReported)
		{
			return;
		}

		TArray<FString> ChannelNames;
		if (Component.IsUsingAbsoluteLocation())
		{
			ChannelNames.Add(TEXT("location"));
		}
		if (Component.IsUsingAbsoluteRotation())
		{
			ChannelNames.Add(TEXT("rotation"));
		}
		if (Component.IsUsingAbsoluteScale())
		{
			ChannelNames.Add(TEXT("scale"));
		}
		if (ChannelNames.IsEmpty())
		{
			return;
		}

		Diagnostics.bAbsoluteReported = true;

		const FText Channels = FText::FromString(FString::Join(ChannelNames, TEXT(", ")));
		if (Component.GetFName() == Diagnostics.ComponentName)
		{
			Diagnostics.Problems.Add(FText::Format(
				LOCTEXT("AbsoluteChannelSelf", "'{0}' uses world-absolute {1}; the flattened transform is relative to the actor root and carries no world frame, so that channel is composed as relative and the mesh may sit in the wrong place."),
				FText::FromName(Diagnostics.ComponentName),
				Channels));
			return;
		}

		Diagnostics.Problems.Add(FText::Format(
			LOCTEXT("AbsoluteChannelAncestor", "'{0}' sits under '{1}', which uses world-absolute {2}; the flattened transform is relative to the actor root and carries no world frame, so that channel is composed as relative and the mesh may sit in the wrong place."),
			FText::FromName(Diagnostics.ComponentName),
			FText::FromName(Component.GetFName()),
			Channels));
	}

	void NoteMissingTemplate(const USCS_Node& Node, FChainDiagnostics& Diagnostics)
	{
		Diagnostics.Problems.Add(FText::Format(
			LOCTEXT("MissingNodeTemplate", "'{0}' is placed through '{1}', whose component template is missing; that node's offset is left out of the flattened transform."),
			FText::FromName(Diagnostics.ComponentName),
			FText::FromName(Node.GetVariableName())));
	}

	void NoteTruncatedChain(const USCS_Node& Node, FChainDiagnostics& Diagnostics)
	{
		Diagnostics.Problems.Add(FText::Format(
			LOCTEXT("TruncatedChain", "'{0}' is placed through '{1}', whose parent '{2}' does not resolve in this class chain; the flattened transform stops there and omits the offsets above it."),
			FText::FromName(Diagnostics.ComponentName),
			FText::FromName(Node.GetVariableName()),
			FText::FromName(Node.ParentComponentOrVariableName)));
	}

	void NoteUnattachedComponent(const USceneComponent& Component, FChainDiagnostics& Diagnostics)
	{
		Diagnostics.Problems.Add(FText::Format(
			LOCTEXT("UnattachedNativeComponent", "'{0}' is placed through '{1}', which is attached to nothing and is not the actor's root; it renders at its own transform regardless of where the actor is, which a transform relative to the actor cannot express."),
			FText::FromName(Diagnostics.ComponentName),
			FText::FromName(Component.GetFName())));
	}

	FTransform ComposeNativeChain(
		const USceneComponent& Component,
		const USceneComponent* ActorRootComponent,
		const bool bFirstIsAncestor,
		const FTransform& AccumulatedBelow,
		FChainDiagnostics& Diagnostics)
	{
		FTransform Result = FTransform::Identity;
		TSet<const USceneComponent*> Visited;

		for (const USceneComponent* Current = &Component; Current != nullptr && !Visited.Contains(Current); Current = Current->GetAttachParent())
		{
			Visited.Add(Current);

			if (bFirstIsAncestor || Current != &Component)
			{
				NoteShear(AccumulatedBelow * Result, Current->GetRelativeScale3D(), Diagnostics);
			}

			if (Current->GetAttachParent() == nullptr)
			{
				if (Current != ActorRootComponent)
				{
					NoteUnattachedComponent(*Current, Diagnostics);
				}

				Result = Result * LocalTransform(*Current);
				break;
			}

			NoteAbsoluteChannels(*Current, Diagnostics);

			const FTransform Below = Result * LocalTransform(*Current);
			const FTransform Socket =
				SocketLink(Current->GetAttachParent(), Current->GetAttachSocketName(), Diagnostics);
			NoteShear(AccumulatedBelow * Below, Socket.GetScale3D(), Diagnostics);
			Result = Below * Socket;
		}

		return Result;
	}

	FTransform ComposeNodeChain(
		const FClassScope& Scope,
		const USCS_Node& Node,
		FChainDiagnostics& Diagnostics)
	{
		FTransform Result = FTransform::Identity;
		TSet<const USCS_Node*> Visited;

		for (const USCS_Node* Current = &Node; Current != nullptr && !Visited.Contains(Current);)
		{
			Visited.Add(Current);

			USCS_Node* ParentNode = nullptr;
			USceneComponent* ParentTemplate = nullptr;
			ResolveParent(Scope, *Current, ParentNode, ParentTemplate);

			const bool bIsRoot = ParentNode == nullptr
				&& ParentTemplate == nullptr
				&& Current->ParentComponentOrVariableName.IsNone();

			const USceneComponent* const Template = ActualSceneTemplate(*Current, Scope.LeafGeneratedClass);
			if (Template == nullptr)
			{
				if (!bIsRoot)
				{
					NoteMissingTemplate(*Current, Diagnostics);
				}
			}
			else
			{
				if (Current != &Node)
				{
					NoteShear(Result, Template->GetRelativeScale3D(), Diagnostics);
				}

				if (bIsRoot)
				{
					Result = Result * LocalTransform(*Template);
				}
				else
				{
					NoteAbsoluteChannels(*Template, Diagnostics);

					const FTransform Below = Result * LocalTransform(*Template);
					const FTransform Socket = SocketLink(ParentTemplate, Current->AttachToName, Diagnostics);
					NoteShear(Below, Socket.GetScale3D(), Diagnostics);
					Result = Below * Socket;
				}
			}

			if (ParentNode != nullptr)
			{
				Current = ParentNode;
				continue;
			}

			if (ParentTemplate != nullptr)
			{
				Result = Result * ComposeNativeChain(*ParentTemplate, Scope.NativeRootComponent, true, Result, Diagnostics);
				break;
			}

			if (!bIsRoot)
			{
				NoteTruncatedChain(*Current, Diagnostics);
				break;
			}

			/** A parentless SCS node is attached to a root inherited from a base class rather than becoming the root (SimpleConstructionScript.cpp:743), so that root's transform still applies above it. */
			if (Scope.NativeRootComponent != nullptr && Template != Scope.NativeRootComponent)
			{
				Result = Result * ComposeNativeChain(*Scope.NativeRootComponent, Scope.NativeRootComponent, true, Result, Diagnostics);
			}
			break;
		}

		return Result;
	}

	void CollectMaterialOverrides(
		const UMeshComponent& Component,
		const FName ComponentName,
		TArray<TObjectPtr<UMaterialInterface>>& OutMaterials,
		TArray<FText>& OutProblems)
	{
		bool bSawNull = false;
		bool bSparseReported = false;

		for (const TObjectPtr<UMaterialInterface>& Material : Component.OverrideMaterials)
		{
			if (Material == nullptr)
			{
				bSawNull = true;
				continue;
			}

			if (bSawNull && !bSparseReported)
			{
				bSparseReported = true;
				OutProblems.Add(FText::Format(
					LOCTEXT("SparseMaterialOverrides", "'{0}' overrides material slots with gaps; the overrides are compacted and no longer match their original slot indices."),
					FText::FromName(ComponentName)));
			}

			OutMaterials.Add(Material);
		}
	}

	void NoteRenderingDifferences(
		const UPrimitiveComponent& Component,
		const FName ComponentName,
		TArray<FText>& OutProblems)
	{
		static const FName RenderingPropertyNames[] = {
			TEXT("CastShadow"),
			TEXT("bCastShadowAsTwoSided"),
			TEXT("bCastHiddenShadow"),
			TEXT("bCastFarShadow"),
			TEXT("bCastInsetShadow"),
			TEXT("bCastContactShadow"),
			TEXT("bHiddenInGame"),
			TEXT("bReceivesDecals"),
			TEXT("bNeverDistanceCull"),
			TEXT("bRenderCustomDepth"),
			TEXT("CustomDepthStencilValue"),
			TEXT("CustomDepthStencilWriteMask"),
			TEXT("TranslucencySortPriority"),
			TEXT("bAffectDynamicIndirectLighting"),
			TEXT("bAffectIndirectLightingWhileHidden"),
			TEXT("LightingChannels"),
			TEXT("bVisibleInReflections"),
			TEXT("bVisibleInRayTracing"),
			TEXT("bVisibleInReflectionCaptures"),
			TEXT("MinDrawDistance"),
			TEXT("LDMaxDrawDistance"),
		};

		const UPrimitiveComponent* const ClassDefault =
			Cast<UPrimitiveComponent>(Component.GetClass()->GetDefaultObject());
		if (ClassDefault == nullptr)
		{
			return;
		}

		TArray<FString> DifferingNames;
		for (const FName& PropertyName : RenderingPropertyNames)
		{
			const FProperty* const Property = FindFProperty<FProperty>(Component.GetClass(), PropertyName);
			if (Property != nullptr && !Property->Identical_InContainer(&Component, ClassDefault))
			{
				DifferingNames.Add(PropertyName.ToString());
			}
		}

		if (DifferingNames.IsEmpty())
		{
			return;
		}

		OutProblems.Add(FText::Format(
			LOCTEXT("PerComponentRenderingLost", "'{0}' changes rendering settings ({1}); rendering settings are authored once per trait, so these are dropped."),
			FText::FromName(ComponentName),
			FText::FromString(FString::Join(DifferingNames, TEXT(", ")))));
	}

	bool IsPlainStaticMeshDraw(const UStaticMeshComponent& Template, TArray<FText>& OutProblems)
	{
		if (Template.IsA<UInstancedStaticMeshComponent>())
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("InstancedComponentSkipped", "'{0}' draws its mesh once per instance; a flat trait list carries a single transform per mesh, so it contributes no mesh and its instances are dropped."),
				FText::FromName(Template.GetFName())));
			return false;
		}

		if (Template.IsA<USplineMeshComponent>())
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("SplineMeshComponentSkipped", "'{0}' deforms its mesh along a spline; a flat trait list carries only the undeformed mesh and a transform, so it contributes no mesh."),
				FText::FromName(Template.GetFName())));
			return false;
		}

		return true;
	}

	bool BuildEntry(
		const UStaticMeshComponent& Template,
		const FTransform& RelativeTransform,
		TArray<FArcFlattenedMeshEntry>& OutEntries,
		TArray<FText>& OutProblems)
	{
		UStaticMesh* const Mesh = Template.GetStaticMesh();
		if (Mesh == nullptr)
		{
			return false;
		}

		const FName ComponentName = Template.GetFName();
		if (!IsPlainStaticMeshDraw(Template, OutProblems))
		{
			return false;
		}

		if (!Template.GetVisibleFlag())
		{
			OutProblems.Add(FText::Format(
				LOCTEXT("HiddenComponentConverted", "'{0}' is hidden in the Blueprint; its mesh was converted anyway and will be visible on the entity."),
				FText::FromName(ComponentName)));
		}

		FArcFlattenedMeshEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.Mesh = Mesh;
		Entry.RelativeTransform = RelativeTransform;
		CollectMaterialOverrides(Template, ComponentName, Entry.MaterialOverrides, OutProblems);
		NoteRenderingDifferences(Template, ComponentName, OutProblems);
		return true;
	}

	void CollectNativeEntries(
		const UClass& ActorClass,
		TArray<FArcFlattenedMeshEntry>& OutEntries,
		TArray<FText>& OutProblems)
	{
		const AActor* const DefaultActor = Cast<AActor>(ActorClass.GetDefaultObject());
		if (DefaultActor == nullptr)
		{
			return;
		}

		const USceneComponent* const ActorRootComponent = EffectiveRootComponent(*DefaultActor);

		for (const UActorComponent* const Component : DefaultActor->GetComponents())
		{
			const UStaticMeshComponent* const MeshComponent = Cast<UStaticMeshComponent>(Component);
			if (MeshComponent == nullptr || MeshComponent->GetStaticMesh() == nullptr)
			{
				continue;
			}

			FChainDiagnostics Diagnostics(MeshComponent->GetFName(), OutProblems);
			const FTransform RelativeTransform =
				ComposeNativeChain(*MeshComponent, ActorRootComponent, false, FTransform::Identity, Diagnostics);

			BuildEntry(*MeshComponent, RelativeTransform, OutEntries, OutProblems);
		}
	}
}

bool FlattenBlueprintStaticMeshes(
	const UClass& ActorClass,
	TArray<FArcFlattenedMeshEntry>& OutEntries,
	TArray<FText>& OutProblems)
{
	using namespace ArcBlueprintMeshFlattenPrivate;

	if (!ActorClass.IsChildOf(AActor::StaticClass()))
	{
		OutProblems.Add(FText::Format(
			LOCTEXT("NotAnActorClass", "'{0}' is not an Actor class."),
			FText::FromString(ActorClass.GetName())));
		return false;
	}

	CollectNativeEntries(ActorClass, OutEntries, OutProblems);

	FClassScope Scope;
	BuildClassScope(ActorClass, Scope);

	for (const USCS_Node* const Node : Scope.AllNodes)
	{
		const UStaticMeshComponent* const Template =
			Cast<UStaticMeshComponent>(ActualSceneTemplate(*Node, Scope.LeafGeneratedClass));
		if (Template == nullptr || Template->GetStaticMesh() == nullptr)
		{
			continue;
		}

		FChainDiagnostics Diagnostics(Template->GetFName(), OutProblems);
		const FTransform RelativeTransform = ComposeNodeChain(Scope, *Node, Diagnostics);
		BuildEntry(*Template, RelativeTransform, OutEntries, OutProblems);
	}

	if (OutEntries.IsEmpty())
	{
		OutProblems.Add(FText::Format(
			LOCTEXT("NoMeshes", "'{0}' has no static mesh component."),
			FText::FromString(ActorClass.GetName())));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
