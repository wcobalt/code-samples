//Flying Wild Hog. All rights reserved

#include "K1MailSystemFunctionLibrary.h"
#include "Action/WebAsyncActions.h"
#include "K1BackendCommunication.h"
#include "K1WebPlayerState.h"
#include "TimerManager.h"
#include "WebPlayerCRUDSystem.h"
#include "JsonData/JsonMailItemData.h"
#include "JsonData/JsonResourceAmountData.h"
#include "Row/Resource.h"
#include "K1Data/Public/K1DatabaseManager.h"

DEFINE_LOG_CATEGORY(LogK1MailSystem)

void UK1MailSystemFunctionLibrary::BindMailResultFunction(UObject* Object,
	FName FunctionName)
{
	FScriptDelegate DelegateMail;
	DelegateMail.BindUFunction(Object, FunctionName);
	MailStruct.OnMailRequestResult.Add(DelegateMail);
}

void UK1MailSystemFunctionLibrary::UnBindMailResultFunction(UObject* Object,
	FName FunctionName)
{
	FScriptDelegate DelegateMail;
	DelegateMail.BindUFunction(Object, FunctionName);
	MailStruct.OnMailRequestResult.Remove(DelegateMail);
}

FMailGlobalDelegates UK1MailSystemFunctionLibrary::MailStruct = {};

void UK1MailSystemFunctionLibrary::SendMailToAllThePlayers(
	UDBMailItemDataAsset* InMailItemDataAsset,
	UObject* WorldContextObject)
{
	if (!InMailItemDataAsset || !WorldContextObject) return;
	
	auto BackendComm =
		UK1BackendCommunication::GetBackendComm(WorldContextObject);
	if (BackendComm)
	{
		BackendComm->GetAllPlayerIds(
			[InMailItemDataAsset, BackendComm](TArray<int64> Ids, bool bSuccess)
			{
				if (!bSuccess)
				{
					MailStruct.OnMailRequestResult.Broadcast(-1, false,
						Ids.Num());
					return;
				}
				
				SendMailToPlayers(MoveTemp(Ids), InMailItemDataAsset,
					BackendComm);
			}
		);
	}
}

void UK1MailSystemFunctionLibrary::SendMailToPlayer(
	UDBMailItemDataAsset* InMailItemDataAsset, UObject* WorldContextObject,
	int64 PlayerId)
{
	if (!InMailItemDataAsset || !WorldContextObject) return;

	auto BackendComm =
		UK1BackendCommunication::GetBackendComm(WorldContextObject);
	if (BackendComm)
	{
		SendMailToPlayers({PlayerId}, InMailItemDataAsset, BackendComm);
	}
}

void UK1MailSystemFunctionLibrary::SendMailToPlayerByEpicId(
	UDBMailItemDataAsset* InMailItemDataAsset, UObject* WorldContextObject,
	FString EpicId)
{
	auto BackendComm =
		UK1BackendCommunication::GetBackendComm(WorldContextObject);
	if (BackendComm)
	{
		BackendComm->GetPlayerIdByEpicUserId(MoveTemp(EpicId),
			[InMailItemDataAsset, WorldContextObject]
			(int64 inId, bool bSuccess)
			{
				if (!bSuccess)
				{
					MailStruct.OnMailRequestResult.Broadcast(-1, false, 0);
					return;
				}
				
				SendMailToPlayer(InMailItemDataAsset, WorldContextObject, inId);
			}
		);
	}
}

void UK1MailSystemFunctionLibrary::SendMailToPlayerByNick(
	UDBMailItemDataAsset* InMailItemDataAsset, UObject* WorldContextObject,
	FString Nick)
{
	auto BackendComm =
		UK1BackendCommunication::GetBackendComm(WorldContextObject);
	if (BackendComm)
	{
		BackendComm->GetPlayerIdByNickName(MoveTemp(Nick),
			[InMailItemDataAsset, WorldContextObject]
			(int64 inId, bool bSuccess)
			{
				if (!bSuccess)
				{
					MailStruct.OnMailRequestResult.Broadcast(-1, false, 0);
					return;
				}
				
				SendMailToPlayer(InMailItemDataAsset, WorldContextObject, inId);
			}
		);
	}
}

void UK1MailSystemFunctionLibrary::SendMailToPlayers(TArray<int64> Ids,
	UDBMailItemDataAsset* InMailItemDataAsset,
	UK1BackendCommunication* BackendCommunication)
{
	if (!Ids.Num())
	{
		return;
	}
	int64 Id = Ids.Pop();

	BackendCommunication->GetMinNextPlayerInventoryIndex(Id,
		[Id, InMailItemDataAsset, BackendCommunication, Ids = MoveTemp(Ids)]
		(int64 Index) mutable
		{
			TSharedPtr<FLocalSaveGameDiffData> DiffData =
				MakeShared<FLocalSaveGameDiffData>();
			DiffData->PlayerId = Id;

			FDBPlayerInventoryData MailInventoryItem;
			MailInventoryItem.PlayerId = Id;
			MailInventoryItem.InventoryItemDataBytes =
				InMailItemDataAsset->ToSerialized();
			MailInventoryItem.Index = Index;
			DiffData->Added.PlayerInventoryData.Add(MailInventoryItem);

			BackendCommunication->SetAllPlayerData("", "", DiffData,
				[Ids = MoveTemp(Ids), InMailItemDataAsset,
					BackendCommunication, Id]
				(EFlushResult Result) mutable
				{
					MailStruct.OnMailRequestResult.Broadcast(Id,
						Result == EFlushResult::Success, Ids.Num());
					// call the method recursively but now with the top element
					// of Ids being removed (by Pop()) and do so until the Ids
					// become empty. no stack overflow can ever happen
					// because the next call will be on an other thread
					SendMailToPlayers(MoveTemp(Ids), InMailItemDataAsset,
						BackendCommunication);
				}
			);
		}
	);
}