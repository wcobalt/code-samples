//Flying Wild Hog. All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "IHttpRouter.h"

/**
* A service which allows to interact with different parts of Google OAuth
* subsystem using Desktop OAuth Workflow
* 
* Isn't thread-safe
* 
* Uses `LogGoogleDesktopOAuth` log category
*/
class FGoogleDesktopOAuth
{
public:
	/**
	* Enum which describes resulting statuses of different Google OAuth
	* flows-related operations
	*/
	enum class Status
	{
		//Is used if everything went well
		kSuccessCode = 0,

		//Is used if a Google OAuth endpoint answered with unexpected content
		//type
		kUnsupportedResponseContentTypeCode = 1,

		//Is used if a Google OAuth endpoint answered in unexpected format
		kInvalidResponseFormatCode = 2,

		//Is used for any error which is not covered by the other error codes
		//constants
		kUnknownErrorCode = 3,

		//Is used if a request could not be completed due to a connection error
		kConnectionErrorCode = 4,

		/**
		* Is used if the refresh request could not be completed due to invalid
		* refresh token
		* 
		* Used only with @see RefreshAuthToken
		*/
		kInvalidGrantErrorCode = 5
	};

	/**
	* Authentication method for Google OAuth Desktop flow
	*/
	enum class AuthenticationMethod
	{
		LoopbackIp
	};

	//Type of callback function used when refreshing a token
	using RefreshCallbackType = TFunction<void(FString Token,
		int64 ExpiresOn, Status Code)>;

	//Type of callback function used when performing manual authentication
	using ManualAuthenticationCallbackType = TFunction<void(FString Token,
		int64 ExpiresOn, FString RefreshToken, Status Code)>;

	using AccessTokenCheckCallbackType = TFunction<void(Status Code)>;

	/**
	* Refreshes OAuth token issued for an user authenticating into a Google App
	* 
	* Callback is called with one of the following values:
	* - `Status::kSuccessCode` - if everything went well. In this case `Token`
	* and `ExpiresOn` are set to valid values
	* - `Status::kUnsupportedResponseContentTypeCode` - if the Google's OAuth
	* endpoint answered with unsupported content type. Values of `Token` and
	* `ExpiresOn` are unspecified
	* - `Status::kInvalidResponseFormatCode` - if the Google's OAuth endpoint
	* answered in an unknown form. Values of `Token` and `ExpiresOn` are
	* unspecified
	* - `Status::kConnectionErrorCode` - if the request couldn't complete due
	* to connectivity problems. Values of `Token` and `ExpiresOn` are
	* unspecified
	* - `Status::kUnknownErrorCode` - if the Google's OAuth endpoint answered
	* with any other error. Values of `Token` and `ExpiresOn` are unspecified
	* 
	* @param Callback Callback to be called no matter whether the refresh
	* request was successfull or not
	* @param ClientID Client ID of the Google App authentication happens for
	* @param ClientSecret Client secret of the Google App authentication
	* happens for
	* @param RefreshToken Refresh token which is owned by the user who is going
	* to be authenticated
	* @see Status
	*/
	void RefreshAuthToken(RefreshCallbackType Callback, FString ClientId,
		FString ClientSecret, FString RefreshToken);

	void AuthenticateManually(ManualAuthenticationCallbackType Callback,
		AuthenticationMethod Method, FString Scopes, FString ClientId,
		FString ClientSecret, int64 LoopbackPort);
	
	void CheckAccessToken(AccessTokenCheckCallbackType Callback,
		FString AccessToken);

private:
	using SuccessfulRequestCallbackType =
		TFunction<void(TSharedPtr<FJsonObject> Data, Status Code)>;

	using FailedRequestCallbackType = TFunction<void(Status Code)>;

	//Type of callback function used when returning from an HTTP request
	using HttpRequestCallbackType = TFunction<void(FHttpRequestPtr Request,
		FHttpResponsePtr Response, bool bWasSuccessful)>;

	/**
	* Sends HTTP request to Google OAuth token refresh endpoint.
	* 
	* Note: `FHttpRequestPtr`, `FHttpResponsePtr` instances and a `bool`
	* are passed to the callback as they are recieved from HTTP module's API
	* call (@see `IHttpRequest::ProcessRequest`).
	* 
	* @param Callback Callback to be called when the request returns
	* @param ClientID Client ID of the Google App authentication happens for
	* @param ClientSecret Client secret of the Google App authentication happens
	* for
	* @param GrantType Google OAuth workflow's grant type to use
	* @param RefreshToken Refresh token which is owned by the user who is going
	* to be authenticated
	*/
	void RefreshAuthTokenImpl(HttpRequestCallbackType Callback,
		FString ClientID, FString ClientSecret, FString GrantType,
		FString RefreshToken) const;

	// more on loopback auth
	// https://developers.google.com/identity/protocols/oauth2/native-app
	void HandleTheResultOfLoopbackAuthentication(
		ManualAuthenticationCallbackType Callback,
		const FHttpResultCallback& OnComplete, FString RedirectUri,
		FString ClientId, FString ClientSecret, FHttpServerRequest Request);

	// general request result handler, contains boilerplate code
	void HandleResultOfRequest(FHttpRequestPtr Request,
		FHttpResponsePtr Response, bool bWasSuccessful,
		SuccessfulRequestCallbackType SuccessfulRequestCallback,
		FailedRequestCallbackType FailedRequestCallback);

	void Answer(const FHttpResultCallback& OnComplete, FString Title,
		FString Message) const;

	//Token refresh section

	//Base part of URL token refresh is accessible through
	const FString kGoogleRefreshTokenUrl =
		"https://oauth2.googleapis.com/token";

	const FString kGoogleRefreshTokenClientIdField = "client_id";

	const FString kGoogleRefreshTokenClientSecretField = "client_secret";

	const FString kGoogleRefreshTokenRefreshTokenField = "refresh_token";

	const FString kGoogleRefreshTokenGrantTypeField = "grant_type";

	const FString kGoogleRefreshTokenGrantTypeValue = "refresh_token";

	//JSON field which contains refreshed access token. Applicable to successful
	//responses from token refresh endpoint only.
	const FString kGoogleRefreshTokenResponseAccessTokenField = "access_token";

	//JSON field which contains "expires in" value. Applicable to successful
	//responses from token refresh endpoint only.
	const FString kGoogleRefreshTokenResponseExpiresInField = "expires_in";

	//Manual authentication section

	const FString kManualAuthenticationUrlBase =
		"https://accounts.google.com/o/oauth2/v2/auth?";

	const FString kManualAuthenticationScopeField = "scope";

	const FString kManualAuthenticationResponseTypeField = "response_type";

	const FString kManualAuthenticationResponseTypeValue = "code";

	const FString kManualAuthenticationRedirectUriField = "redirect_uri";

	const FString kManualAuthenticationClientIdField = "client_id";

	const FString kLoopbackAddressBase = "http://127.0.0.1:";

	const FString kLoopbackAddressPath = "/google_oauth";

	const FString kManualAuthenticationResponseCodeField = "code";

	const FString kManualAuthenticationResponseErrorField = "error";

	//Code exchange section

	const FString kCodeExchangeUrlBase = "https://oauth2.googleapis.com/token";

	const FString kCodeExchangeCodeField = "code";

	const FString kCodeExchangeClientIdField = "client_id";

	const FString kCodeExchangeClientSecretField = "client_secret";

	const FString kCodeExchangeRedirectUriField = "redirect_uri";

	const FString kCodeExchangeGrantTypeField = "grant_type";

	const FString kCodeExchangeGrantTypeValue = "authorization_code";

	const FString kCodeExchangeResponseAccessTokenField = "access_token";

	const FString kCodeExchangeResponseExpiresInField = "expires_in";

	const FString kCodeExchangeResponseRefreshTokenField = "refresh_token";

	//Miscellaneous section

	const FString kInvalidGrantErrorCode = "invalid_grant";

	//`Content-Type` for JSON
	const FString kJsonContentType = "application/json";

	//`Content-Type` for HTML
	const FString kHtmlContentType = "text/html";

	//`Content-Type` for url encoded data
	const FString kUrlencodedContentType = "application/x-www-form-urlencoded";

	//Name of `Content-Type` HTTP header
	const FString kContentTypeHeader = "Content-Type";

	//HTTP POST method name
	const FString kPostMethod = "POST";

	//HTTP GET method name
	const FString kGetMethod = "GET";
	
	//JSON field which contains a string with error code
	const FString kGoogleRefreshTokenResponseErrorCodeField = "error";

	//JSON field which contains a string with error description
	const FString kGoogleRefreshTokenResponseErrorField = "error_description";

	const FString kUserInfoUrlBase =
		"https://openidconnect.googleapis.com/v1/userinfo?access_token=";
	
	/**
	* Keeps the last set by `AuthenticateManually()` UE4's HTTP Server router
	* handle for the address Google OAuth endpoint is going to refer to when
	* executing Google OAuth desktop loopback IP flow
	*/
	FHttpRouteHandle RouterHandle;
};