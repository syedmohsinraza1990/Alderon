// Fill out your copyright notice in the Description page of Project Settings.


#include "API/PakLoader.h"
#include "IPlatformFilePak.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformFile.h"

TSharedPtr<FPakPlatformFile> UPakLoader::PakPlatformFile = nullptr;

bool UPakLoader::MountPakFile(const FString& PakFilePath, const FString& PakMountPoint)
{
	int32 PakOrder = 0;
	if (!PakPlatformFile.IsValid())
	{
		PakPlatformFile = MakeShareable(new FPakPlatformFile());
		PakPlatformFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT("PakFile"));
		FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile);
	}

	if (PakPlatformFile->Mount(*PakFilePath, PakOrder))
	{
		TRefCountPtr<FPakFile> PakData = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilePath, false, true);
		if (PakData.GetReference()->IsValid())
		{
			FString mountPoint = PakData.GetReference()->GetMountPoint();
			UE_LOG(LogTemp, Log, TEXT("Mount Point: %s"), *mountPoint);
		}
		UE_LOG(LogTemp, Log, TEXT("Pak file mounted successfully."));
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to mount pak file."));
		return false;
	}
}

bool UPakLoader::UnmountPakFile(const FString& PakFilePath)
{
	FPakPlatformFile* PakFileMgr = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
	if (PakFileMgr == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to get PakPlatformFile for pak file (Unmount): %s"), *(PakFilePath));
		return false;
	}
	return PakFileMgr->Unmount(*PakFilePath);
}

void UPakLoader::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FPackageName::RegisterMountPoint(RootPath, ContentPath);
}

void UPakLoader::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	FPackageName::UnRegisterMountPoint(RootPath, ContentPath);
}

FString const UPakLoader::GetPakMountPoint(const FString& PakFilePath)
{
	FPakFile* Pak = nullptr;
	TRefCountPtr<FPakFile> PakFile = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilePath, false);
	Pak = PakFile.GetReference();
	if (Pak->IsValid())
	{
		return Pak->GetMountPoint();
	}
	return FString();
}

TArray<FString> UPakLoader::GetPakContent(const FString& PakFilePath, bool bOnlyCooked /*= true*/)
{
	FPakFile* Pak = nullptr;
	TRefCountPtr<FPakFile> PakFile = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilePath, false);
	Pak = PakFile.GetReference();
	TArray<FString> PakContent;

	if (Pak->IsValid())
	{
		FString ContentPath, PakAppendPath;
		FString MountPoint = GetPakMountPoint(PakFilePath);
		TArray<FString> AssetList;
		Pak->FindPrunedFilesAtPath(AssetList, *MountPoint, true, false, true);
		MountPoint.Split("/Content/", &ContentPath, &PakAppendPath);

		TArray<FPakFile::FFilenameIterator> Records;
		for (FPakFile::FFilenameIterator It(*Pak, false); It; ++It)
		{
			Records.Add(It);
		}

		for (auto& It : Records)
		{
			if (bOnlyCooked)
			{
				if (FPaths::GetExtension(It.Filename()) == TEXT("uasset"))
				{
					PakContent.Add(FString::Printf(TEXT("%s%s"), *PakAppendPath, *It.Filename()));
				}
			}
			else
			{
				PakContent.Add(FString::Printf(TEXT("%s%s"), *PakAppendPath, *It.Filename()));
			}
		}
	}
	return PakContent;
}

FString UPakLoader::GetPakMountContentPath(const FString& PakFilePath)
{   
	FString ContentPath, RightStr;
	bool bIsContent;
	FString MountPoint = GetPakMountPoint(PakFilePath);
	bIsContent = MountPoint.Split("/Content/", &ContentPath, &RightStr);
	if (bIsContent)
	{
		return FString::Printf(TEXT("%s/Content/"), *ContentPath);
	}
	// Check Pak Content for /Content/ Path
	else
	{
		TArray<FString> Content = GetPakContent(PakFilePath);
		if (Content.Num() > 0)
		{
			FString FullPath = FString::Printf(TEXT("%s%s"), *MountPoint, *Content[0]);
			bIsContent = FullPath.Split("/Content/", &ContentPath, &RightStr);
			if (bIsContent)
			{
				return FString::Printf(TEXT("%s/Content/"), *ContentPath);
			}
		}
	}
	//Failed to Find Content
	return FString("");
}

void UPakLoader::MountAndRegisterPak(FString PakFilePath, bool& bIsMountSuccessful)
{
	FString PakRootPath = "/Game/";
	if (MountPakFile(PakFilePath, FString("")))
	{
		bIsMountSuccessful = true;
		const FString MountPoint = GetPakMountContentPath(PakFilePath);
		RegisterMountPoint(PakRootPath, MountPoint);
	}
}

UObject* UPakLoader::LoadPakObjClassReference(FString PakContentPath)
{
	FString PakRootPath = "/Game/";
	const FString FileName = Conv_PakContentPathToReferenceString(PakContentPath, PakRootPath);
	if (IsFileMounted(FileName))
	{
		UE_LOG(LogTemp, Log, TEXT("File is mounted."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("File is not mounted."));
	}
	return LoadPakFileClass(FileName);
}

UObject* UPakLoader::LoadPakFileClass(const FString& FileName)
{
	return StaticLoadObject(UObject::StaticClass(), nullptr, *FileName);
}

FString UPakLoader::Conv_PakContentPathToReferenceString(const FString PakContentPath, const FString PakMountPath)
{
	return FString::Printf(TEXT("%s%s.%s"),
		*PakMountPath, *FPaths::GetBaseFilename(PakContentPath, false), *FPaths::GetBaseFilename(PakContentPath, true));
}

bool UPakLoader::IsFileMounted(const FString& FilePath)
{
	if (PakPlatformFile.IsValid())
	{
		return PakPlatformFile->FileExists(*FilePath);
	}
	return false;
}
