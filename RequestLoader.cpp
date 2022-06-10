#include "RequestLoader.h"
#include "version.h"

DEFINE_LOG_CATEGORY_STATIC(LogXYZProductRequestLoader, All, All);

FRequestLoader::FRequestLoader(
    TUniquePtr<IRequestProvider> RequestProvider,
    TUniquePtr<IRequestDeserializer> RequestDeserializer)
        : RequestProvider(MoveTemp(RequestProvider)),
          RequestDeserializer(MoveTemp(RequestDeserializer)) {}

TUniquePtr<FExtractionRequest> FRequestLoader::LoadRequest() const
{
    FString Contents = RequestProvider->RetrieveContents();

    RequestDeserializer->Deserialize(MoveTemp(Contents));
    
    FVersion RequiredVersion = RequestDeserializer->ExtractVersion();

    if (!CheckVersionsCompatibility(RequiredVersion))
    {
        FString RequiredVersionString = FString::Printf(TEXT("%d.%d.%d"),
            RequiredVersion.Major, RequiredVersion.Minor,
            RequiredVersion.Index);
        FString PluginVersionString = FString::Printf(TEXT("%d.%d.%d"),
            VERSION_MAJOR, VERSION_MINOR, VERSION_INDEX);
        
        UE_LOG(LogXYZProductRequestLoader, Error,
            TEXT("The required version and the plugin version mismatch: "
                 "%s against %s"),
            *RequiredVersionString, *PluginVersionString);

        return nullptr;
    }
    
    return RequestDeserializer->ExtractRequest();
}

bool FRequestLoader::CheckVersionsCompatibility(
    const FVersion& RequiredMinimalVersion) const
{
    bool bMajorGreater = VERSION_MAJOR > RequiredMinimalVersion.Major;
    bool bMajorEqual = VERSION_MAJOR == RequiredMinimalVersion.Major;
    bool bMinorGreater = VERSION_MINOR > RequiredMinimalVersion.Minor;
    bool bMinorEqual = VERSION_MINOR == RequiredMinimalVersion.Minor;
    bool bIndexGreaterOrEqual = VERSION_INDEX >= RequiredMinimalVersion.Index;
    
    return bMajorGreater || (bMajorEqual && bMinorGreater)
        || (bMajorEqual && bMinorEqual && bIndexGreaterOrEqual);
}
