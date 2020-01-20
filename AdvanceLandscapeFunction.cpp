// Copyright 2018 ScienceTech LLC. All Rights Reserved.

#include "AdvanceLandscapeFunction.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Async/ParallelFor.h"
#include "EncoderName.h"

FAdvanceLandscapeFunction::FAdvanceLandscapeFunction()
{
}

FAdvanceLandscapeFunction::~FAdvanceLandscapeFunction() 
{
}

void FAdvanceLandscapeFunction::StitchingEdgesOfLandscapes(FString & LogMassage, FString MaskName, bool bUseWMMask)
{
	LogMassage.Append(TEXT("\n\n    **** Start ***** \n\n"));
	TArray<ALandscapeProxy*> Landscapes(GetAllLandscapesOfWorld());
	FEncoderName encoder(MaskName);
	int32 resolution(0);
	TMap<FString, TArray<uint16>> HeightmapsData(GetHeightmapsData(Landscapes, LogMassage, resolution, encoder));
	AlignHeightsToNeighbors(resolution, encoder, bUseWMMask, HeightmapsData);
	SetHeightmapsData(HeightmapsData, Landscapes);
	LogMassage.Append(TEXT("\n    **** Finish *****\n\n"));
}

void FAdvanceLandscapeFunction::AlignHeightsToNeighbors(int32 Resolution, const IEncoderNameBase& Encoder,  bool  bUseWMMask, TMap<FString, TArray<uint16>>& HeightmapsData)
{
	TArray<FString> Keys;
	ParallelFor(HeightmapsData.GetKeys(Keys), [&](int32 index)
	{
		FString NameToFind;
		int32 xIndex, yIndex;

		if (!Encoder.CheckNameforMask(Keys[index]))
		{
			return;
		}

		Encoder.GetIndex(Keys[index], xIndex, yIndex);
		
		// Align to bottom
		NameToFind = Encoder.GetName(xIndex + 1, yIndex);
		if (HeightmapsData.Contains(NameToFind))
		{
			for (int i = 0; i < Resolution; i++)
			{
				uint16 mid = (HeightmapsData[Keys[index]][Resolution * (Resolution - 1) + i] + HeightmapsData[NameToFind][i]) / 2;
				HeightmapsData[Keys[index]][Resolution * (Resolution - 1) + i] = mid;
				HeightmapsData[NameToFind][i] = mid;
			}
		}

		// Align to right
		NameToFind = Encoder.GetName(xIndex, yIndex + 1);
		if (HeightmapsData.Contains(NameToFind))
		{
			for (int i = 0; i < Resolution; i++)
			{
				uint16 mid = (HeightmapsData[Keys[index]][Resolution * i + Resolution - 1] + HeightmapsData[NameToFind][Resolution * i]) / 2;
				HeightmapsData[Keys[index]][Resolution * i + Resolution - 1] = mid;
				HeightmapsData[NameToFind][Resolution * i] = mid;
			}
		}
	}/*, true*/);

	// Align corners
	for (auto &Heightmap : HeightmapsData)
	{
		FString NameToFind;
		int32 xIndex, yIndex;
		Encoder.GetIndex(Heightmap.Key, xIndex, yIndex);

		FString RightName(Encoder.GetName(xIndex, yIndex + 1));
		FString BottomRightName(Encoder.GetName(xIndex + 1, yIndex + 1));
		FString BottomName(Encoder.GetName(xIndex + 1, yIndex));

		// Find the average between the four corners and stitch
		if (HeightmapsData.Contains(RightName) && HeightmapsData.Contains(BottomName) && HeightmapsData.Contains(BottomRightName))
		{
			int average = (Heightmap.Value[Resolution * Resolution - 1] + HeightmapsData[RightName][Resolution * (Resolution - 1)] +
				HeightmapsData[BottomRightName][0] + HeightmapsData[BottomName][Resolution - 1]) / 4;
			Heightmap.Value[Resolution * Resolution - 1] = average;
			HeightmapsData[RightName][Resolution * (Resolution - 1)] = average;
			HeightmapsData[BottomRightName][0] = average;
			HeightmapsData[BottomName][Resolution - 1] = average; 
		}
	}
}

TMap<FString, TArray<uint16>> FAdvanceLandscapeFunction::GetHeightmapsData(const TArray<ALandscapeProxy*>& Landscapes, FString& OutLog, int32& resolution, const IEncoderNameBase& encoder)
{
	// The landscape is square by default 
	// If the resolution is not specified then take the resolution of the first landscape with the correct name
	TMap<FString, TArray<uint16>> HeightmapsData;

	if (Landscapes.Num() == 0)
	{
		OutLog.Append(TEXT(" 0 - Landscapes foubd "));
		return HeightmapsData;
	}

	FVector Scale = Landscapes[0]->GetActorScale3D();
	if (resolution == 0)
	{
		for (const auto& landscape : Landscapes)
		{
			int32 MinX(MAX_int32);
			int32 MinY(MAX_int32);
			int32 MaxX(-MAX_int32);
			int32 MaxY(-MAX_int32);
			
			bool bValidName(encoder.CheckNameforMask(landscape->GetActorLabel().ToUpper()));
			if (!(Landscapes[0]->GetLandscapeInfo()->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && bValidName))
			{
				continue;
			}
			else
			{
				resolution = (MaxX  - MinX > MaxY - MinY) ? MaxY - MinY : MaxX - MinX;
				break;
			}
		}
	}

	if (resolution != 0)
	{
		OutLog.Append(FString::Printf(TEXT("Resolution  %i x %i;\n"), resolution + 1, resolution + 1));
	}

	OutLog.Append(FString::Printf(TEXT("\nScale  %s;\n"), *Scale.ToString()));
	for ( const auto& landscape : Landscapes)
	{
		FString name(landscape->GetActorLabel().ToUpper());

		// Skip landscapes with the wrong name
		if (!encoder.CheckNameforMask(name))
		{
			OutLog.Append(FString::Printf(TEXT("\nError : bad name - %s;\n"), *(landscape->GetActorLabel())));
			continue;
		}
		FLandscapeEditDataInterface  EditorLandscapeInterface(landscape->GetLandscapeInfo());

		int32 MinX(MAX_int32);
		int32 MinY(MAX_int32);
		int32 MaxX(-MAX_int32);
		int32 MaxY(-MAX_int32);

		// Skip landscapes with wrong resolution
		if (!(landscape->GetLandscapeInfo()->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && resolution == ((MaxX - MinX > MaxY - MinY) ? MaxY - MinY : MaxX - MinX)))
		{
			OutLog.Append(FString::Printf(TEXT("\nError : %s - bad resolution %i x %i;\n"), *(landscape->GetActorLabel()), (MaxX - MinX + 1), (MaxY - MinY + 1)));
			continue;
		}
		
		TArray<uint16> HeightData;
		TArray<uint16> ZeroData;
		HeightData.AddZeroed((MaxX - MinX + 1) * (MaxY - MinY + 1));
		ZeroData.AddZeroed((MaxX - MinX + 1) * (MaxY - MinY + 1));
		EditorLandscapeInterface.GetHeightDataFast(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
		HeightmapsData.Add(name, HeightData);
		
		if (!Scale.Equals(landscape->GetActorScale3D(), 0.01))
		{
			OutLog.Append(FString::Printf(TEXT("\nWarning  : %s - bad scale - %s;\n"), *(landscape->GetActorLabel()), *landscape->GetActorScale3D().ToString()));
		}
	}

	return HeightmapsData;
}

void FAdvanceLandscapeFunction::SetHeightmapsData(const TMap<FString, TArray<uint16> >& HeightmapsData, TArray<ALandscapeProxy*>& Landscapes)
{
	for (auto& landscape : Landscapes)
	{
		if (!landscape)
		{
			continue;
		}
	
		FString name = landscape->GetActorLabel();
		
		if (!HeightmapsData.Contains(name))
		{
			continue;
		}

		const TArray<uint16> HeightData = HeightmapsData[name];

		TUniquePtr<FLandscapeEditDataInterface> EditorLandscapeInterface(MakeUnique<FLandscapeEditDataInterface>(landscape->GetLandscapeInfo()));
		landscape->Modify();
		 
		int32 MinX(MAX_int32);
		int32 MinY(MAX_int32);
		int32 MaxX(-MAX_int32);
		int32 MaxY(-MAX_int32);

		if (!landscape->GetLandscapeInfo()->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		{
			continue;
		}
		EditorLandscapeInterface->SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);
		landscape->PostEditChange();
	}
}

TArray<ALandscapeProxy*> FAdvanceLandscapeFunction::GetAllLandscapesOfWorld()
{	
	TArray<ALandscapeProxy*> Landscapes;
	TSubclassOf<AActor> ActorClass = ALandscapeProxy::StaticClass();
	UWorld* World(GWorld);
	if (ActorClass && World)
	{
		for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor->IsPendingKill())
			{
				Landscapes.Add((ALandscapeProxy*)Actor);
			}
		}
	}
	return Landscapes;
}



