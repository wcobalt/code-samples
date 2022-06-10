//Flying Wild Hog. All rights reserved

#include "GoogleDesktopOAuth.h"
#include "HttpModule.h"
#include "Misc/StringBuilder.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HttpUtils.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"

//Default values are All, All
DEFINE_LOG_CATEGORY_STATIC(LogGoogleDesktopOAuth, All, All);

void FGoogleDesktopOAuth::RefreshAuthToken(
	RefreshCallbackType Callback, FString ClientId,
	FString ClientSecret, FString RefreshToken)
{
	/*
	* Kick off the refreshing procedure
	*
	* 1. Send request
	* 2. Check response
	*/

	RefreshAuthTokenImpl(
		[this, Callback = MoveTemp(Callback)]
		(FHttpRequestPtr Request, FHttpResponsePtr Response,
			bool bWasSuccessful) {
		HandleResultOfRequest(Request, Response, bWasSuccessful,
			[Callback, this]
			(TSharedPtr<FJsonObject> Data, Status Code)
			{
				//try to retrieve `access_token` and `expires_in`
				FString AccessToken;
				int64 ExpiresIn;
				bool bIsPresented = Data->TryGetStringField(
						kGoogleRefreshTokenResponseAccessTokenField,
					AccessToken);

				if (!bIsPresented)
				{
					UE_LOG(LogGoogleDesktopOAuth, Error,
						TEXT("Unable to extract `%s` field from the response,"
							 " aborting"),
						*kGoogleRefreshTokenResponseAccessTokenField);

					Callback("", 0, Status::kInvalidResponseFormatCode);
					return;
				}

				ExpiresIn = Data->GetIntegerField(
					kGoogleRefreshTokenResponseExpiresInField);

				UE_LOG(LogGoogleDesktopOAuth, Log,
					TEXT("The access token has been successfully refreshed"));
				//set absolute expiration datetime
				int64 ExpiresOn = FDateTime::Now().ToUnixTimestamp() +
					ExpiresIn;
				Callback(MoveTemp(AccessToken), ExpiresOn,
					Status::kSuccessCode);
			},
			[Callback](Status Code)
			{
				UE_LOG(LogGoogleDesktopOAuth, Log,
					TEXT("The access token failed to refresh"));

				Callback("", 0, Code);
			}
		);
	}, MoveTemp(ClientId), MoveTemp(ClientSecret),
		kGoogleRefreshTokenGrantTypeValue, MoveTemp(RefreshToken));
}

void FGoogleDesktopOAuth::RefreshAuthTokenImpl(HttpRequestCallbackType Callback,
	FString ClientID, FString ClientSecret, FString GrantType,
	FString RefreshToken) const
{
	auto Request =
		FHttpModule::Get().CreateRequest();
	Request->SetVerb(kPostMethod);
	Request->SetHeader(kContentTypeHeader, kUrlencodedContentType);
	Request->SetURL(kGoogleRefreshTokenUrl);

	//for the parameters meaning refer to google oauth docs
	TMap<FString, FString> Params;
	Params.Add(kGoogleRefreshTokenClientIdField, ClientID);
	Params.Add(kGoogleRefreshTokenClientSecretField, ClientSecret);
	Params.Add(kGoogleRefreshTokenRefreshTokenField, RefreshToken);
	Params.Add(kGoogleRefreshTokenGrantTypeField, GrantType);

	FString Content = FHttpUtils::BuildUrlEncodedPayload(Params);
	Request->SetContentAsString(Content);
	Request->OnProcessRequestComplete().BindLambda(Callback);
	Request->ProcessRequest();
}

void FGoogleDesktopOAuth::HandleResultOfRequest(FHttpRequestPtr Request,
	FHttpResponsePtr Response, bool bWasSuccessful,
	SuccessfulRequestCallbackType SuccessfulRequestCallback,
	FailedRequestCallbackType FailedRequestCallback)
{
	// a lot of boilerplate code that checks for each kind of errors:
	// connectivity errors, wrong response formats, HTTP codes, etc
	// also it tries to form the error message using the standard google oauth
	// errors fields, like: kGoogleRefreshTokenResponseErrorCodeField and
	// kGoogleRefreshTokenResponseErrorField
	if (Request->GetStatus() != EHttpRequestStatus::Failed_ConnectionError)
	{
		int32 ResponseCode = Response->GetResponseCode();
		UE_LOG(LogGoogleDesktopOAuth, Log,
			TEXT("The auth-related request has returned. Status - %d"),
			ResponseCode);

		FString ResponseContentType =
			FHttpUtils::ExtractMimeFromContentType(Response->GetContentType());

		if (ResponseContentType == kJsonContentType)
		{
			TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
			TSharedRef<TJsonReader<TCHAR>> JsonReader =
				TJsonReaderFactory<TCHAR>::Create(
					Response->GetContentAsString());
			FJsonSerializer::Deserialize(JsonReader, JsonObject);

			//todo(artsiom.drapun@fuerogames.pl): move codes to consts?
			if (ResponseCode >= 200 && ResponseCode < 300)
			{
				UE_LOG(LogGoogleDesktopOAuth, Log,
					TEXT("The auth-related request has been successfully"
						 " completed"));

				SuccessfulRequestCallback(JsonObject, Status::kSuccessCode);
			}
			else
			{
				//try to retrieve error code and error description fields values
				FString ErrorCode;
				FString Error;

				JsonObject->TryGetStringField(
					kGoogleRefreshTokenResponseErrorCodeField, ErrorCode);
				JsonObject->TryGetStringField(
					kGoogleRefreshTokenResponseErrorField, Error);

				if (ErrorCode == kInvalidGrantErrorCode)
				{
					UE_LOG(LogGoogleDesktopOAuth, Error,
						TEXT("Invalid grant error happened when performing an"
							 " auth-related request"), *ErrorCode, *Error);

					FailedRequestCallback(Status::kInvalidGrantErrorCode);
				}
				else
				{
					UE_LOG(LogGoogleDesktopOAuth, Error,
					 TEXT("An error happened when performing an auth-related"
					 	  " request. Code - `%s`: \"%s\""), *ErrorCode, *Error);

					FailedRequestCallback(Status::kUnknownErrorCode);
				}
			}
		}
		else
		{
			UE_LOG(LogGoogleDesktopOAuth, Error,
				TEXT("Unsupported response content type - `%s`, supported one"
					 " is `%s`"), *ResponseContentType, *kJsonContentType);

			FailedRequestCallback(Status::kUnsupportedResponseContentTypeCode);
		}
	}
	else
	{
		UE_LOG(LogGoogleDesktopOAuth, Error,
			TEXT("The auth-related request couldn't be completed due to"
				 " connectivity problems"));

		FailedRequestCallback(Status::kConnectionErrorCode);
	}
}

void FGoogleDesktopOAuth::AuthenticateManually(
	ManualAuthenticationCallbackType Callback, AuthenticationMethod Method,
	FString Scopes, FString ClientId, FString ClientSecret, int64 LoopbackPort)
{
	if (Method == AuthenticationMethod::LoopbackIp)
	{
		//for the parameters meaning refer to google oauth docs
		TMap<FString, FString> UrlPayloadData;
		UrlPayloadData.Add(kManualAuthenticationScopeField, Scopes);
		UrlPayloadData.Add(kManualAuthenticationResponseTypeField,
			kManualAuthenticationResponseTypeValue);

		FString RedirectUri = kLoopbackAddressBase +
			FString::FromInt(LoopbackPort) + kLoopbackAddressPath;
		UrlPayloadData.Add(kManualAuthenticationRedirectUriField,
			RedirectUri);
		UrlPayloadData.Add(kManualAuthenticationClientIdField,
			ClientId);

		FString UrlPayload = FHttpUtils::BuildUrlEncodedPayload(UrlPayloadData);
		FPlatformProcess::LaunchURL(
			*(kManualAuthenticationUrlBase + UrlPayload), nullptr,
			nullptr);

		if (!FHttpServerModule::IsAvailable())
		{
			FModuleManager::LoadModuleChecked<FHttpServerModule>("HTTPServer");
		}

		if (FHttpServerModule::IsAvailable())
		{
			// the idea is the following: start own server instance on localhost
			// which will serve as a loopback ip. the only endpoint on this
			// instance will be kLoopbackAddressPath and the same loopback url
			// will be passed into the auth request, so when the auth request
			// returns it'll "call" loopback url
			FHttpServerModule& HttpServer = FHttpServerModule::Get();
			TSharedPtr<IHttpRouter> Router =
				HttpServer.GetHttpRouter(LoopbackPort);

			// the endpoint handler
			FHttpRequestHandler Handler = [this, Callback = MoveTemp(Callback),
				RedirectUri = MoveTemp(RedirectUri),
				ClientId = MoveTemp(ClientId),
				ClientSecret = MoveTemp(ClientSecret)]
			(const FHttpServerRequest& Request,
			 const FHttpResultCallback& OnComplete) -> bool
			{
				HandleTheResultOfLoopbackAuthentication(Callback, OnComplete,
					RedirectUri, ClientId, ClientSecret, Request);

				return true;
			};

			// in case if it'sn not the first time we start the server instance
			if (RouterHandle.IsValid())
			{
				Router->UnbindRoute(RouterHandle);
			}
			RouterHandle = Router->BindRoute(FHttpPath(kLoopbackAddressPath),
				EHttpServerRequestVerbs::VERB_GET |
				EHttpServerRequestVerbs::VERB_POST, MoveTemp(Handler));
			HttpServer.StartAllListeners();
		}
		else
		{
			UE_LOG(LogGoogleDesktopOAuth, Error,
				TEXT("The HTTP server module cannot be loaded. Unable to"
					 " perform manual authentication"));
			Callback("", 0, "", Status::kUnknownErrorCode);
		}
	}
	else
	{
		UE_LOG(LogGoogleDesktopOAuth, Error,
			TEXT("The specified method of manual authentication is"
				 " unsupported. Please wait 30 years for it to become"
				 " available"));
		Callback("", 0, "", Status::kUnknownErrorCode);
	}
}

void FGoogleDesktopOAuth::HandleTheResultOfLoopbackAuthentication(
	ManualAuthenticationCallbackType Callback, 
	const FHttpResultCallback& OnComplete, FString RedirectUri,
	FString ClientId, FString ClientSecret, FHttpServerRequest Request)
{
	// we've got the code, need to exchange it for the token
	FString* Code =
		Request.QueryParams.Find(kManualAuthenticationResponseCodeField);
	FString MessageTitle;
	FString Message;

	if (Code)
	{
		Answer(OnComplete, "Authentication succeed",
			"Authentication succeed. Return to the VO Importer,"
			" please.");

		auto HttpRequest =
			FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(kPostMethod);
		HttpRequest->SetHeader(kContentTypeHeader,
			kUrlencodedContentType);
		HttpRequest->SetURL(kCodeExchangeUrlBase);

		//check google oauth docs for the meaning of each parameter
		TMap<FString, FString> Params;
		Params.Add(kCodeExchangeCodeField, *Code);
		Params.Add(kCodeExchangeClientIdField,
			MoveTemp(ClientId));
		Params.Add(kCodeExchangeClientSecretField,
			MoveTemp(ClientSecret));
		Params.Add(kCodeExchangeRedirectUriField,
			MoveTemp(RedirectUri));
		Params.Add(kCodeExchangeGrantTypeField,
			kCodeExchangeGrantTypeValue);

		FString Content = FHttpUtils::BuildUrlEncodedPayload(Params);
		HttpRequest->SetContentAsString(Content);

		// just parse the answer that was returned by the endpoint and decide
		// whether the request is a success or not
		HttpRequestCallbackType Handler = [this, Callback = MoveTemp(Callback)]
		(FHttpRequestPtr Request, FHttpResponsePtr Response,
			bool bWasSuccessful)
		{
			HandleResultOfRequest(Request, Response, bWasSuccessful,
				[Callback, this]
				(TSharedPtr<FJsonObject> Data, Status Code)
				{
					//try to retrieve `access_token`, `refresh_token` and
					//`expires_in`
					FString AccessToken;
					FString RefreshToken;
					int64 ExpiresIn;
					bool bIsPresented = Data->TryGetStringField(
						kCodeExchangeResponseAccessTokenField, AccessToken);
					bIsPresented &= Data->TryGetStringField(
							kCodeExchangeResponseRefreshTokenField,
							RefreshToken);

					if (!bIsPresented)
					{
						UE_LOG(LogGoogleDesktopOAuth, Error,
							TEXT("Unable to extract from the response one of"
								 " the following fields: `%s`, `%s`, aborting"),
							*kCodeExchangeResponseAccessTokenField,
							*kCodeExchangeResponseRefreshTokenField);

						Callback("", 0, "", Status::kInvalidResponseFormatCode);
						return;
					}

					ExpiresIn = Data->GetIntegerField(
						kCodeExchangeResponseExpiresInField);

					UE_LOG(LogGoogleDesktopOAuth, Log,
						TEXT("The code exchange was successfully completed"));
					//set absolute expiration datetime
					int64 ExpiresOn = FDateTime::Now().ToUnixTimestamp() +
						ExpiresIn;
					Callback(MoveTemp(AccessToken), ExpiresOn,
						MoveTemp(RefreshToken), Status::kSuccessCode);
				},
				[Callback](Status Code)
				{
					UE_LOG(LogGoogleDesktopOAuth, Log,
						TEXT("The code exchange failed"));

					Callback("", 0, "", Code);
				}
			);
		};
		HttpRequest->OnProcessRequestComplete().BindLambda(Handler);
		HttpRequest->ProcessRequest();
	}
	else
	{
		Answer(OnComplete, "Authentication failed",
			"Authentication failed. Return to the VO Importer,"
			" please.");

		FString* Error = Request.QueryParams.Find(
			kManualAuthenticationResponseErrorField);
		UE_LOG(LogGoogleDesktopOAuth, Error,
			TEXT("Unable to authenticate: `%s`"), **Error);

		Callback("", 0, "", Status::kInvalidGrantErrorCode);
	}
}

void FGoogleDesktopOAuth::Answer(const FHttpResultCallback& OnComplete,
	FString Title, FString Message) const
{
	//todo(artsiom.drapun@fuerogames.pl): I don't think it worth it to move
	// the output format into a const
	OnComplete(FHttpServerResponse::Create(
		*(FString::Printf(TEXT("<!doctype><html><head><title>%s</title>"
			"</head><body>%s You can now close the tab. </body></html>"),
				*Title, *Message))
		)
	);
}

void FGoogleDesktopOAuth::CheckAccessToken(
	AccessTokenCheckCallbackType Callback, FString AccessToken)
{
	//just send a "ping" request with this token to user info endpoint
	// if the request succeeds then token is valid
	auto Request =
		FHttpModule::Get().CreateRequest();
	Request->SetVerb(kGetMethod);
	Request->SetHeader(kContentTypeHeader,
		kUrlencodedContentType);
	FString Url = kUserInfoUrlBase;
	Url += AccessToken;

	Request->SetURL(Url);

	Request->OnProcessRequestComplete().BindLambda(
		[this, Callback = MoveTemp(Callback)]
		(FHttpRequestPtr Request, FHttpResponsePtr Response,
			bool bWasSuccessful)
		{
			HandleResultOfRequest(Request, Response, bWasSuccessful,
				[Callback, this]
				(TSharedPtr<FJsonObject> Data, Status Code)
				{
					UE_LOG(LogGoogleDesktopOAuth, Log,
						TEXT("The access token has been checked and the token"
							 " is fine"));

					Callback(Status::kSuccessCode);
				},
				[Callback](Status Code)
				{
					UE_LOG(LogGoogleDesktopOAuth, Log,
						TEXT("Access token check failed"));

					Callback(Code);
				}
			);
		}
	);
	Request->ProcessRequest();
}