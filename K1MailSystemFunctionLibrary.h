//Flying Wild Hog. All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "K1WebPlayerState.h"
#include "Delegates/Delegate.h"
#include "K1MailSystemFunctionLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogK1MailSystem, All, All)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMailRequestResult, int32,
	PlayedId, bool, RequestResult, int32, Left);

class UK1BackendCommunication;
class UDBMailItemDataAsset;

USTRUCT(BlueprintType)
struct FMailGlobalDelegates
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintAssignable, Category = "CRUD")
		 FMailRequestResult OnMailRequestResult;
	UPROPERTY(BlueprintAssignable, Category = "CRUD")
		 FMailRequestResult OnItemIdRequestResult;
	UPROPERTY(BlueprintAssignable, Category = "CRUD")
		 FMailRequestResult OnPlayerIdRequestResult;

};

UCLASS()
class K1WEB_API UK1MailSystemFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	using MailSendCallbackType = TFunction<void(bool)>;
	
	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void SendMailToAllThePlayers(
		UDBMailItemDataAsset* InMailItemDataAsset, UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void SendMailToPlayer(UDBMailItemDataAsset* InMailItemDataAsset,
		UObject* WorldContextObject, int64 PlayerId);

	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void SendMailToPlayerByEpicId(
		UDBMailItemDataAsset* InMailItemDataAsset,
		UObject* WorldContextObject, FString EpicId);

	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void SendMailToPlayerByNick(
		UDBMailItemDataAsset* InMailItemDataAsset,
		UObject* WorldContextObject, FString Nick);
	
	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static FMailGlobalDelegates &GetMailStruct()
	{
		return MailStruct;
	}

	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void BindMailResultFunction(UObject *Object, FName FunctionName);

	UFUNCTION(BlueprintCallable, meta = (Category = "CRUD"))
	static void UnBindMailResultFunction(UObject* Object,
		FName FunctionName);

	static FMailGlobalDelegates MailStruct;

private:
	static void SendMailToPlayers(TArray<int64> Ids,
		UDBMailItemDataAsset* InMailItemDataAsset,
		UK1BackendCommunication* BackendCommunication);
};
