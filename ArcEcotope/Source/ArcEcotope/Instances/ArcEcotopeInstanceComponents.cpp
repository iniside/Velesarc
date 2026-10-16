// Copyright Lukasz Baran. All Rights Reserved.

#include "Instances/ArcEcotopeInstanceComponents.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"

namespace ArcEcotopeInstanceComponentsPrivate
{
	class FCanonicalContentHash64
	{
	public:
		void AddUInt8(const uint8 Value)
		{
			Hash ^= Value;
			Hash *= Prime;
		}

		void AddUInt32(const uint32 Value)
		{
			for (int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddUInt64(const uint64 Value)
		{
			for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddInt32(const int32 Value)
		{
			AddUInt32(static_cast<uint32>(Value));
		}

		void AddFloat(float Value)
		{
			if (Value == 0.0f)
			{
				Value = 0.0f;
			}
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			AddUInt32(Bits);
		}

		void AddDouble(double Value)
		{
			if (Value == 0.0)
			{
				Value = 0.0;
			}
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			AddUInt64(Bits);
		}

		void AddString(const FString& Value)
		{
			const FTCHARToUTF8 Utf8(*Value);
			AddInt32(Utf8.Length());
			for (int32 Index = 0; Index < Utf8.Length(); ++Index)
			{
				AddUInt8(static_cast<uint8>(Utf8.Get()[Index]));
			}
		}

		void AddObject(const UObject* Object)
		{
			AddString(Object != nullptr ? FSoftObjectPath(Object).ToString() : FString());
		}

		void AddTransform(const FTransform& Transform)
		{
			const FVector Translation = Transform.GetTranslation();
			const FQuat Rotation = Transform.GetRotation();
			const FVector Scale = Transform.GetScale3D();
			AddDouble(Translation.X);
			AddDouble(Translation.Y);
			AddDouble(Translation.Z);
			AddDouble(Rotation.X);
			AddDouble(Rotation.Y);
			AddDouble(Rotation.Z);
			AddDouble(Rotation.W);
			AddDouble(Scale.X);
			AddDouble(Scale.Y);
			AddDouble(Scale.Z);
		}

		void AddTransform(const FTransform3f& Transform)
		{
			const FVector3f Translation = Transform.GetTranslation();
			const FQuat4f Rotation = Transform.GetRotation();
			const FVector3f Scale = Transform.GetScale3D();
			AddFloat(Translation.X);
			AddFloat(Translation.Y);
			AddFloat(Translation.Z);
			AddFloat(Rotation.X);
			AddFloat(Rotation.Y);
			AddFloat(Rotation.Z);
			AddFloat(Rotation.W);
			AddFloat(Scale.X);
			AddFloat(Scale.Y);
			AddFloat(Scale.Z);
		}

		uint64 Finalize() const
		{
			return Hash;
		}

	private:
		static constexpr uint64 OffsetBasis = 14695981039346656037ull;
		static constexpr uint64 Prime = 1099511628211ull;
		uint64 Hash = OffsetBasis;
	};

	void AddComponentContract(FCanonicalContentHash64& Hash, const USceneComponent& Component)
	{
		Hash.AddUInt8(static_cast<uint8>(Component.CreationMethod));
		Hash.AddString(Component.GetAttachParent() != nullptr
			? Component.GetAttachParent()->GetName()
			: FString());
		Hash.AddUInt8(static_cast<uint8>(Component.GetCollisionEnabled()));
		Hash.AddUInt8(Component.CanEverAffectNavigation() ? 1 : 0);
	}

	void AddMaterials(FCanonicalContentHash64& Hash, const UMeshComponent& Component)
	{
		const int32 MaterialCount = Component.GetNumMaterials();
		Hash.AddInt32(MaterialCount);
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			Hash.AddObject(Component.GetMaterial(MaterialIndex));
		}
	}

	struct FComponentContentEntry
	{
		uint8 Type = 0;
		FString GroupName;
		uint64 ContentHash = 0;
	};

	bool TryBuildContentEntries(
		TConstArrayView<TObjectPtr<UInstancedStaticMeshComponent>> StaticComponents,
		TConstArrayView<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedComponents,
		TArray<FComponentContentEntry>& OutEntries,
		FText& OutError)
	{
		OutEntries.Reset();
		OutEntries.Reserve(StaticComponents.Num() + SkinnedComponents.Num());

		for (const UInstancedStaticMeshComponent* Component : StaticComponents)
		{
			if (!IsValid(Component))
			{
				OutError = FText::FromString(TEXT("ArcEcotope static instance content contains an invalid component."));
				return false;
			}

			FCanonicalContentHash64 Hash;
			Hash.AddUInt8(0);
			Hash.AddObject(Component->GetClass());
			Hash.AddString(Component->GetName());
			AddComponentContract(Hash, *Component);
			Hash.AddObject(Component->GetStaticMesh());
			AddMaterials(Hash, *Component);
			const int32 InstanceCount = Component->GetInstanceCount();
			Hash.AddInt32(InstanceCount);
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				FTransform Transform;
				if (!Component->GetInstanceTransform(InstanceIndex, Transform, false))
				{
					OutError = FText::FromString(TEXT("ArcEcotope could not read a saved static instance transform."));
					return false;
				}
				Hash.AddTransform(Transform);
			}

			FComponentContentEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.Type = 0;
			Entry.GroupName = Component->GetName();
			Entry.ContentHash = Hash.Finalize();
		}

		for (const UInstancedSkinnedMeshComponent* Component : SkinnedComponents)
		{
			if (!IsValid(Component))
			{
				OutError = FText::FromString(TEXT("ArcEcotope skinned instance content contains an invalid component."));
				return false;
			}

			FCanonicalContentHash64 Hash;
			Hash.AddUInt8(1);
			Hash.AddObject(Component->GetClass());
			Hash.AddString(Component->GetName());
			AddComponentContract(Hash, *Component);
			Hash.AddObject(Component->GetSkinnedAsset());
			Hash.AddObject(Component->GetTransformProvider());
			AddMaterials(Hash, *Component);
			const TArray<FSkinnedMeshInstanceData>& InstanceData = Component->GetInstanceData();
			if (InstanceData.Num() != Component->GetInstanceCount())
			{
				OutError = FText::FromString(TEXT("ArcEcotope skinned instance count does not match its saved instance data."));
				return false;
			}
			Hash.AddInt32(InstanceData.Num());
			for (const FSkinnedMeshInstanceData& Instance : InstanceData)
			{
				Hash.AddTransform(Instance.Transform);
				Hash.AddUInt32(Instance.AnimationIndex);
			}

			FComponentContentEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.Type = 1;
			Entry.GroupName = Component->GetName();
			Entry.ContentHash = Hash.Finalize();
		}
		return true;
	}
}

namespace UE::ArcEcotope
{
	bool IsGeneratedComponentName(const UActorComponent& Component)
	{
		const FString Name = Component.GetName();
		return Name.StartsWith(TEXT("ArcEcotopeISM_"))
			|| Name.StartsWith(TEXT("ArcEcotopeISKM_"))
			|| Name.StartsWith(TEXT("ArcEcotopeCandidateISM"))
			|| Name.StartsWith(TEXT("ArcEcotopeCandidateISKM"))
			|| Name.StartsWith(TEXT("ArcEcotopeRetired"))
			|| Name.StartsWith(TEXT("ArcEcotopeDiscarded"));
	}

	bool HasValidGeneratedComponentContract(const USceneComponent& Component, const USceneComponent* RootComponent)
	{
		return Component.CreationMethod == EComponentCreationMethod::Instance
			&& RootComponent != nullptr
			&& Component.GetAttachParent() == RootComponent;
	}

	void RenameForDestruction(UActorComponent& Component, const TCHAR* BaseName)
	{
		const FName UniqueName = MakeUniqueObjectName(
			Component.GetOuter(), Component.GetClass(), FName(BaseName));
		Component.Rename(*UniqueName.ToString(), nullptr, DerivedRenameFlags);
	}

	void RemoveInstanceComponents(
		AActor& Owner,
		TArray<TObjectPtr<UInstancedStaticMeshComponent>>& StaticComponents,
		TArray<TObjectPtr<UInstancedSkinnedMeshComponent>>& SkinnedComponents)
	{
		for (UInstancedStaticMeshComponent* Component : StaticComponents)
		{
			if (IsValid(Component))
			{
				Owner.RemoveInstanceComponent(Component);
				RenameForDestruction(*Component, TEXT("ArcEcotopeDiscardedISM"));
				Component->DestroyComponent();
			}
		}
		for (UInstancedSkinnedMeshComponent* Component : SkinnedComponents)
		{
			if (IsValid(Component))
			{
				Owner.RemoveInstanceComponent(Component);
				RenameForDestruction(*Component, TEXT("ArcEcotopeDiscardedISKM"));
				Component->DestroyComponent();
			}
		}
		StaticComponents.Reset();
		SkinnedComponents.Reset();
	}

	bool TryComputeContentHash(
		TConstArrayView<TObjectPtr<UInstancedStaticMeshComponent>> StaticComponents,
		TConstArrayView<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedComponents,
		uint64& OutHash,
		FText& OutError)
	{
		using namespace ArcEcotopeInstanceComponentsPrivate;

		ARCFOREST_SCOPED_COUNTER(UE::ArcForest::GetInstanceContentHashCounter(), InstanceComponentContentHash);
		TArray<FComponentContentEntry> Entries;
		if (!TryBuildContentEntries(StaticComponents, SkinnedComponents, Entries, OutError))
		{
			return false;
		}
		Entries.Sort([](const FComponentContentEntry& Left, const FComponentContentEntry& Right)
		{
			if (Left.Type != Right.Type)
			{
				return Left.Type < Right.Type;
			}
			if (Left.GroupName != Right.GroupName)
			{
				return Left.GroupName < Right.GroupName;
			}
			return Left.ContentHash < Right.ContentHash;
		});

		FCanonicalContentHash64 Hash;
		Hash.AddInt32(Entries.Num());
		for (const FComponentContentEntry& Entry : Entries)
		{
			Hash.AddUInt8(Entry.Type);
			Hash.AddString(Entry.GroupName);
			Hash.AddUInt64(Entry.ContentHash);
		}
		OutHash = Hash.Finalize();
		OutError = FText::GetEmpty();
		return true;
	}
}
