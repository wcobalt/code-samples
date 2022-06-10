// Copyright 2022, Flying Wild Hog sp. z o.o.

#include "QueueService.h"

#include "K1BackendCommunication.h"
#include "K1NativeGameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogK1Queue, Display, Display);

AQueueService::AQueueService()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AQueueService::UpdateQueue()
{
	auto GameInstance = Cast<UK1NativeGameInstance>(GetGameInstance());

	if (GameInstance)
	{
		UK1BackendCommunication* BackendComm = GameInstance->GetBackendComm();
		BackendComm->EnterQueue(
		[this, BackendComm =
			TWeakObjectPtr<UK1BackendCommunication>(BackendComm)]
		(UK1BackendCommunication::QueueAdmittionStatus Status, bool isOk)
		{
			if (!BackendComm.IsValid())
				return;

			// if an error happens, retry timer will be set on this time
			float RetryAfter = 15.0;
			bool bDoSetTimer = true;
			
			if (!isOk)
			{
				//the queue has died for some reason
				UE_LOG(LogK1Queue, Error,
					TEXT("Enter queue request has returned with error"));
				OnQueueUpdate.Broadcast(false, false, 0);
			}
			else
			{
				//player got into the queue
				UE_LOG(LogK1Queue, Display, TEXT("Enter queue request is"
					" sucessful. adm: %d, wsec: %d, retsec: %d, secstr: %s"),
					Status.bIsAdmitted, Status.WaitTimeSeconds,
					Status.RetryAfterSeconds, *Status.WaitTimeString);
				if (Status.bIsAdmitted)
				{
					//and he is admitted, so cancel the queue and start the game
					UE_LOG(LogK1Queue, Display, TEXT("Player is admitted"));
					OnQueueUpdate.Broadcast(true, true, 0);
					bDoSetTimer = false;
					CancelQueue();
					// if the player is logged in then go right in the hub
					if (BackendComm.Get()->IsLoggedIn())
					{
						StartHub();
					}
					// if not then enable logging in
					else
					{
						BackendComm.Get()->SetDoLoginInTick(true);
					}
				}
				else
				{
					//wait until the player is admitted
					UE_LOG(LogK1Queue, Display,
						TEXT("Player isn't admitted. Retrying in %ds"),
						Status.RetryAfterSeconds);
					RetryAfter = Status.RetryAfterSeconds;
					OnQueueUpdate.Broadcast(true, false,
						Status.WaitTimeSeconds);
				}
			}

			if (bDoSetTimer)
			{
				GetWorld()->GetTimerManager().SetTimer(QueueRetryTimerHandle,
					this, &AQueueService::UpdateQueue, RetryAfter);
			}
		});
	}
	else
	{
		UE_LOG(LogK1Queue, Fatal, TEXT("Unable to cast game instance"))
	}
}

void AQueueService::CancelQueue()
{
	if (QueueRetryTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(QueueRetryTimerHandle);
	}
	OnQueueUpdate.Clear();
}

void AQueueService::EnterQueue()
{
	TScriptDelegate<FWeakObjectPtr> Delegate;
	Delegate.BindUFunction(this, "HandleQueue");

	if (!OnQueueUpdate.Contains(Delegate))
	{
		OnQueueUpdate.Add(Delegate);
		UpdateQueue();
	}
}
