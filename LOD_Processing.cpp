// Copyright 2018 SforT LLC. All Rights Reserved.

#include "LOD_Processing.h"
// For work with RawMesh.
#include "RawMesh.h"
// For work with Content Browser.
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
// For work with Static Mesh Actor
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshComponentAdapter.h"
// For work with Save
#include "FileHelpers.h"
//For work with materials
#include "Materials/Material.h"
//SmoothLODCreator headers
#include "SmoothLODMaterialInterface.h"
#include "GlobalLogString.h"



// Init work dir path
const FString LOD_Processing::WorkDirPath(FString(TEXT("/Game/SmoothLOD_Plugin")));
FString LOD_Processing::LogData(TEXT(""));

LOD_Processing::LOD_Processing()
{	
}

LOD_Processing::~LOD_Processing()
{
}

void LOD_Processing::CreateSmoothLOD(UStaticMesh* StaticMesh, const int32& NumOfLOD)
{		
	GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("StaticMesh - %s\n"), *StaticMesh->GetName()));
	GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("Num LODs = %i\n"), StaticMesh->GetNumLODs()));
	GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT(" New LOD will be create between LOD[%i] and LOD[%i]\n\n***********\n\n"), NumOfLOD, NumOfLOD + 1));

	UStaticMesh* ResultMesh = nullptr;
	if (StaticMesh)
	{
		ResultMesh = AddSmoothLOD(StaticMesh, NumOfLOD);
	}

	// Calculation and set of the screen size for new LOD
	if(ResultMesh)
	{
		GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("ResultMesh - %s\n"), *ResultMesh->GetName()));
		GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("Num LODs = %i\n"), ResultMesh->GetNumLODs()));
		TArray<float> ScreenSizes(GetLODsScreenSize(StaticMesh));

		if (NumOfLOD + 1 < ScreenSizes.Num())
		{
			float sizeFirst(ScreenSizes[NumOfLOD]);
			float sizeSecond(ScreenSizes[NumOfLOD + 1]);
			ResultMesh->Modify();
			ResultMesh->PreEditChange(nullptr);
			ResultMesh->SourceModels[NumOfLOD + 1].ScreenSize = sizeFirst - (sizeFirst - sizeSecond) * 0.6;
			GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("Set ScreenSize for %i LOD = %f\n"), NumOfLOD + 1, ResultMesh->SourceModels[NumOfLOD + 1].ScreenSize.Default));
			ResultMesh->PostEditChange();
			ResultMesh->MarkPackageDirty();
		}
		UpdateMaterialsByScreenSize(ResultMesh);
	}
}

// Get screen size for each LOD of static mesh
TArray<float> LOD_Processing::GetLODsScreenSize(const UStaticMesh * StaticMesh )
{
	TArray<float> LODsScreenSize;
	float TempScreenSize(1);
	GlobalLogString::Get()->LogData.Append(TEXT("ScreenSizes of LODs for input mesh: "));
	for (int i(0); i < StaticMesh->SourceModels.Num(); ++i)
	{
		TempScreenSize = StaticMesh->bAutoComputeLODScreenSize ? FMath::Pow(0.75, i) : StaticMesh->SourceModels[i].ScreenSize.Default;
		UE_LOG(LogTemp, Display, TEXT("LOD #%d - Screen Size = %f - isAutoCompute = %s"), i, TempScreenSize, StaticMesh->bAutoComputeLODScreenSize ? TEXT("True") : TEXT("False"));
		GlobalLogString::Get()->LogData.Append(FString::Printf(TEXT("%f,   "), TempScreenSize));
		LODsScreenSize.Add(TempScreenSize);
	}
	GlobalLogString::Get()->LogData.Append(TEXT("\n"));
	return LODsScreenSize;
}

UStaticMesh*  LOD_Processing::AddSmoothLOD(UStaticMesh * StaticMesh, const int32 & NumOfLOD)
{
	if (!StaticMesh || StaticMesh->GetNumLODs() < 2)
	{
		
		GlobalLogString::Get()->LogData.Append(TEXT("Error: StaticMeshPtr = NULL or Num LODs < 2"));
		return nullptr;
	}

	// Create the new static mesh
	FString PackageName(WorkDirPath + TEXT("/") + StaticMesh->GetName());

	FAssetToolsModule& AssetToolsModule(FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools"));

	UPackage* Package = nullptr;
	UStaticMesh* TempMesh = nullptr;
	{
		FString AssetNameStr;
		FString PackageNameStr;
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName + TEXT("/") + *StaticMesh->GetName() + TEXT("_Smooth"), TEXT(""), PackageNameStr, AssetNameStr);
		Package = CreatePackage(NULL, *PackageNameStr);
		check(Package);
		TempMesh = NewObject<UStaticMesh>(Package, *AssetNameStr, RF_Public | RF_Standalone);
	}
	TempMesh->PreEditChange(nullptr);
	TempMesh->InitResources();

	TempMesh->LightingGuid = FGuid::NewGuid();

	//  Copy parameters of the original mesh
	for (const auto staticMaterial : StaticMesh->StaticMaterials)
	{
		TempMesh->StaticMaterials.Add(staticMaterial);
	}

	TempMesh->bAutoComputeLODScreenSize = StaticMesh->bAutoComputeLODScreenSize;
	TempMesh->bAllowCPUAccess = StaticMesh->bAllowCPUAccess;
	TempMesh->bCustomizedCollision = StaticMesh->bCustomizedCollision;
	TempMesh->bGenerateMeshDistanceField = StaticMesh->bGenerateMeshDistanceField;
	TempMesh->bHasNavigationData = StaticMesh->bHasNavigationData;
	TempMesh->bRequiresLODDistanceConversion = StaticMesh->bRequiresLODDistanceConversion;
	TempMesh->bRequiresLODScreenSizeConversion = StaticMesh->bRequiresLODScreenSizeConversion;
	TempMesh->bSupportUniformlyDistributedSampling = StaticMesh->bSupportUniformlyDistributedSampling;
	TempMesh->LightMapCoordinateIndex = StaticMesh->LightMapCoordinateIndex;
	TempMesh->LightMapResolution = StaticMesh->LightMapResolution;
	TempMesh->LightmapUVDensity = StaticMesh->LightmapUVDensity;
	TempMesh->LightmapUVVersion = StaticMesh->LightmapUVVersion;
	TempMesh->LODForCollision = StaticMesh->LODForCollision;
	TempMesh->LODGroup = StaticMesh->LODGroup;
	TempMesh->LpvBiasMultiplier = StaticMesh->LpvBiasMultiplier;
	TempMesh->MinLOD = StaticMesh->MinLOD;
	
	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>();
	MeshComponent->SetStaticMesh(StaticMesh);
	FStaticMeshComponentAdapter Adapter(MeshComponent);
	FRawMesh FirstLOD, SecondLOD;
	Adapter.RetrieveRawMeshData(NumOfLOD, FirstLOD, false);
	Adapter.RetrieveRawMeshData(NumOfLOD+1, SecondLOD, false);

	FirstLOD.FaceSmoothingMasks.Append(SecondLOD.FaceSmoothingMasks);

	FString NamePostfix =  TEXT("_FirstSmooth");
	FString Path = FPackageName::GetLongPackagePath(TempMesh->GetOutermost()->GetName());

	// Set new materials for First LOD
	TArray<uint32> UniqueMaterialIndices;
	for (uint32 MaterialIndex : FirstLOD.FaceMaterialIndices)
	{
		UniqueMaterialIndices.AddUnique(MaterialIndex);
	}
	
	for (auto& MaterialIndex : UniqueMaterialIndices)
	{
		UMaterialInterface* MatInterface = StaticMesh->GetMaterial(MaterialIndex);
		if (MatInterface)
		{
			UMaterial* material = MatInterface->GetMaterial();

			if (material)
			{
				FString AssetNameStr;
				FString PackageNameStr;
				AssetToolsModule.Get().CreateUniqueAssetName(Path + TEXT("/") + material->GetName() + NamePostfix, TEXT(""), PackageNameStr, AssetNameStr);
				UObject* DuplicatedAsset(AssetToolsModule.Get().DuplicateAsset(AssetNameStr, Path, material));
				UMaterial* NewMaterial(Cast<UMaterial>(DuplicatedAsset));
				// Used only basic materials. An instance material will be replaced with the duplicate of the parent material.
				SmoothLODMaterialInterface::EditMaterial(NewMaterial, ETypeMeshLOD::TM_First);
				int32 NewMaterialIndex = TempMesh->StaticMaterials.Add(FStaticMaterial(NewMaterial, *NewMaterial->GetName()));

				for (auto& OldMaterialIndex : FirstLOD.FaceMaterialIndices)
				{
					if (MaterialIndex == OldMaterialIndex)
					{
						OldMaterialIndex = NewMaterialIndex;
					}
				}
			}
		}
	}

	// Create and add new materials to FirstLOD which correspond to the number of materials of the SecondLOD
	NamePostfix = TEXT("_SecondSmooth");
	UniqueMaterialIndices.Empty();
	for (uint32 MaterialIndex : SecondLOD.FaceMaterialIndices)
	{
		UniqueMaterialIndices.AddUnique(MaterialIndex);
	}
	for (auto& MaterialIndex : UniqueMaterialIndices)
	{
		UMaterialInterface* MatInterface = StaticMesh->GetMaterial(MaterialIndex);
		if (MatInterface)
		{
			UMaterial* material = MatInterface->GetMaterial();

			if (material)
			{
				FString AssetNameStr;
				FString PackageNameStr;
				AssetToolsModule.Get().CreateUniqueAssetName(Path + TEXT("/") + material->GetName() + NamePostfix, TEXT(""), PackageNameStr, AssetNameStr);
				UObject* DuplicatedAsset(AssetToolsModule.Get().DuplicateAsset(AssetNameStr, Path, material));
				UMaterial* NewMaterial(Cast<UMaterial>(DuplicatedAsset));

				// Used only basic materials. An instance material will be replaced with the duplicate of the parent material.
				SmoothLODMaterialInterface::EditMaterial(NewMaterial, ETypeMeshLOD::TM_Second);
				int32 NewMaterialIndex = TempMesh->StaticMaterials.Add(FStaticMaterial(NewMaterial, *NewMaterial->GetName()));

				for (auto& OldMaterialIndex : SecondLOD.FaceMaterialIndices)
				{
					if (MaterialIndex == OldMaterialIndex)
					{
						OldMaterialIndex = NewMaterialIndex;
					}
				}
			}
		}
	}
	FirstLOD.FaceMaterialIndices.Append(SecondLOD.FaceMaterialIndices);
	for (auto& matInerface : TempMesh->StaticMaterials)
	{
		matInerface.MaterialInterface->PreEditChange(nullptr);
		matInerface.MaterialInterface->PostEditChange ();
	}

	const int32 IndicesOffset = FirstLOD.VertexPositions.Num();
	for (int32 WedgeIndex : SecondLOD.WedgeIndices)
	{
		FirstLOD.WedgeIndices.Add(WedgeIndex + IndicesOffset);
	}

	for (FVector VertexPos : SecondLOD.VertexPositions)
	{
		FirstLOD.VertexPositions.Add(VertexPos/* - MergedAssetPivot*/);
	}

	FirstLOD.WedgeTangentX.Append(SecondLOD.WedgeTangentX);
	FirstLOD.WedgeTangentY.Append(SecondLOD.WedgeTangentY);
	FirstLOD.WedgeTangentZ.Append(SecondLOD.WedgeTangentZ);

	for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
	{
		if (FirstLOD.WedgeTexCoords->Num() > ChannelIdx)
		{
			const TArray<FVector2D>& SourceChannel = SecondLOD.WedgeTexCoords[ChannelIdx];
			TArray<FVector2D>& TargetChannel = FirstLOD.WedgeTexCoords[ChannelIdx];

			// Whether source mesh has data in this channel
			if (TargetChannel.Num() == 0)
			{
				const int32 TexCoordNum = FirstLOD.WedgeIndices.Num() - SecondLOD.WedgeIndices.Num();
				for (int32 CoordIdx = 0; CoordIdx < TexCoordNum; ++CoordIdx)
				{
					TargetChannel.Add(FVector2D::ZeroVector);
				}
			}
			if (SourceChannel.Num())
			{
				TargetChannel.Append(SourceChannel);
			}
			else
			{
				// Fill with zero coordinates if source mesh has no data for this channel
				const int32 TexCoordNum = SecondLOD.WedgeIndices.Num();
				for (int32 CoordIdx = 0; CoordIdx < TexCoordNum; ++CoordIdx)
				{
					TargetChannel.Add(FVector2D::ZeroVector);
				}
			}
		}
	}

	// Fill the vertex color information
	FirstLOD.WedgeColors.Empty();

	// Get information about section
	TArray<FSectionInfo> FirstSectionInfo;
	Adapter.RetrieveMeshSections(NumOfLOD, FirstSectionInfo);
	TArray<FSectionInfo> SecondSectionInfo;
	Adapter.RetrieveMeshSections(NumOfLOD, SecondSectionInfo);

	// Update information about sections (LODs number has not been updated yet)
	FMeshSectionInfoMap SectionInfoMap;
	for (int32 LodIndex(0); LodIndex < StaticMesh->GetNumLODs(); ++LodIndex)
	{
		if(LodIndex == NumOfLOD + 1)
		{
			int32 uniqIndexMaterial(-1);
			int32 numberSection(0);
			for (uint32 MaterialIndex : FirstLOD.FaceMaterialIndices)
			{
				if (uniqIndexMaterial != MaterialIndex)
				{
					uniqIndexMaterial = MaterialIndex;
					FMeshSectionInfo SectionInfo;
					SectionInfo.bCastShadow = true;
					SectionInfo.bEnableCollision = true;
					SectionInfo.MaterialIndex = MaterialIndex;
					SectionInfoMap.Set(NumOfLOD + 1, numberSection, SectionInfo);
					++numberSection;
				}
			}
		}
		{
			int32 numSection = StaticMesh->SectionInfoMap.GetSectionNumber(LodIndex);
			for (int32 SectionIndex(0); SectionIndex < numSection; ++SectionIndex)
			{
				const FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LodIndex, SectionIndex);
				if (LodIndex < NumOfLOD + 1)
				{
					SectionInfoMap.Set(LodIndex, SectionIndex, SectionInfo);
				}
				else
				{
					SectionInfoMap.Set(LodIndex + 1, SectionIndex, SectionInfo);
				}
			}
		}
	}

	// Save LODs in a new mesh
	for (int32 index(0); index < StaticMesh->SourceModels.Num(); ++index)
	{
		if (index == NumOfLOD + 1)
		{
			FStaticMeshSourceModel* SrcModel = new (TempMesh->SourceModels) FStaticMeshSourceModel();
			SrcModel->BuildSettings.bRecomputeNormals = false;
			SrcModel->BuildSettings.bRecomputeTangents = false;
			SrcModel->BuildSettings.bRemoveDegenerates = true;
			SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
			SrcModel->BuildSettings.bGenerateLightmapUVs = StaticMesh->SourceModels[NumOfLOD].BuildSettings.bGenerateLightmapUVs;
			SrcModel->BuildSettings.MinLightmapResolution = StaticMesh->SourceModels[NumOfLOD].BuildSettings.MinLightmapResolution;
			SrcModel->BuildSettings.SrcLightmapIndex = 0;
			SrcModel->BuildSettings.DstLightmapIndex = StaticMesh->SourceModels[NumOfLOD].BuildSettings.DstLightmapIndex;
			SrcModel->RawMeshBulkData->SaveRawMesh(FirstLOD);
		}

		FStaticMeshSourceModel* SrcModel = new (TempMesh->SourceModels) FStaticMeshSourceModel();
		SrcModel->BuildSettings.bRecomputeNormals = StaticMesh->SourceModels[index].BuildSettings.bRecomputeNormals;
		SrcModel->BuildSettings.bRecomputeTangents = StaticMesh->SourceModels[index].BuildSettings.bRecomputeTangents;
		SrcModel->BuildSettings.bRemoveDegenerates = StaticMesh->SourceModels[index].BuildSettings.bRemoveDegenerates;
		SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = StaticMesh->SourceModels[index].BuildSettings.bUseHighPrecisionTangentBasis;
		SrcModel->BuildSettings.bUseFullPrecisionUVs = StaticMesh->SourceModels[index].BuildSettings.bUseFullPrecisionUVs;
		SrcModel->BuildSettings.bGenerateLightmapUVs = StaticMesh->SourceModels[index].BuildSettings.bGenerateLightmapUVs;
		SrcModel->BuildSettings.MinLightmapResolution = StaticMesh->SourceModels[index].BuildSettings.MinLightmapResolution;
		SrcModel->BuildSettings.SrcLightmapIndex = StaticMesh->SourceModels[index].BuildSettings.SrcLightmapIndex;
		SrcModel->BuildSettings.DstLightmapIndex = StaticMesh->SourceModels[index].BuildSettings.DstLightmapIndex;
		SrcModel->ScreenSize = StaticMesh->SourceModels[index].ScreenSize;
		FRawMesh tempRaw;
		Adapter.RetrieveRawMeshData(index, tempRaw, false);
		SrcModel->RawMeshBulkData->SaveRawMesh(tempRaw);
	}

	TempMesh->SectionInfoMap.Clear();
	TempMesh->OriginalSectionInfoMap.Clear();
	TempMesh->SectionInfoMap.CopyFrom(SectionInfoMap);
	TempMesh->OriginalSectionInfoMap.CopyFrom(SectionInfoMap);
	TempMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	TempMesh->PostEditChange();
	TempMesh->MarkPackageDirty();
	TempMesh->Build(true);

	FAssetRegistryModule::AssetCreated(TempMesh);

	return TempMesh;
}

// Update materials by screen size
void LOD_Processing::UpdateMaterialsByScreenSize(UStaticMesh * StaticMesh)
{
	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>();
	MeshComponent->SetStaticMesh(StaticMesh);
	FStaticMeshComponentAdapter Adapter(MeshComponent);
	FRawMesh rawMeshData;
	rawMeshData.Empty();
	for (int32 i(0); i < StaticMesh->SourceModels.Num() - 1; ++i)
	{
		Adapter.RetrieveRawMeshData(i, rawMeshData, false);
		TArray<uint32> UniqueMaterialIndices;
		for (uint32 MaterialIndex : rawMeshData.FaceMaterialIndices)
		{
			UniqueMaterialIndices.AddUnique(MaterialIndex);
		}

		for (auto& matIndex : UniqueMaterialIndices)
		{
			SmoothLODMaterialInterface::SetDistanceParams(StaticMesh->StaticMaterials[matIndex].MaterialInterface->GetMaterial(),
														  StaticMesh->SourceModels[i].ScreenSize.Default, 
														  StaticMesh->SourceModels[i + 1].ScreenSize.Default,
														  StaticMesh->GetBounds().SphereRadius);
		}
		rawMeshData.Empty();
	}
}

