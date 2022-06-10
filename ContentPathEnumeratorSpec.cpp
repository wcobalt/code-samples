/*
 Copyright © 2022, XYZ Sp. z o.o. All rights reserved.
 For any additional information refer to https://XYZ.com
 */

#include "CoreMinimal.h"

#include "Misc/AutomationTest.h"
#include "Enumeration/ContentPathEnumerator.h"

BEGIN_DEFINE_SPEC(FContentPathEnumeratorSpec,
                  "XYZProduct.Enumeration.ContentPath",
                  EAutomationTestFlags::ProductFilter |
                  EAutomationTestFlags::ApplicationContextMask)

void CheckPresenceOfObjects(
    FContentPathEnumerator& Enumerator,
    const TSet<FString>& ExpectedAssets)
{
    TSet<UObject*> RetrievedAssets;

    while (auto Object = Enumerator.GetNext())
    {
        RetrievedAssets.Add(Object);
    }

    TestEqual(
        "Expecting count of found assets and expected assets to be equal",
         RetrievedAssets.Num(), ExpectedAssets.Num());
    
    for (auto& ExpectedAssetPath : ExpectedAssets)
    {
        bool bPresented = false;
        for (auto Asset : RetrievedAssets)
        {
            if (Asset->GetPathName().Equals(ExpectedAssetPath))
            {
                bPresented = true;
                break;
            }
        }

        FString Message = FString::Printf(
            TEXT("Expecting asset `%s` to be presented"),
            *ExpectedAssetPath);
        
        TestTrue(Message, bPresented);
    }
}

END_DEFINE_SPEC(FContentPathEnumeratorSpec)

void FContentPathEnumeratorSpec::Define()
{
    It("Empty path",
        [this]()
        {
            FContentPathEnumerator Enumerator("");

            CheckPresenceOfObjects(Enumerator, {});
        }
    );

    It("Invalid path",
        [this]()
        {
            FContentPathEnumerator Enumerator("Invalid/Path 123123123.");

            CheckPresenceOfObjects(Enumerator, {});
        }
    );

    It("Game content, empty directory",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0000");

            CheckPresenceOfObjects(Enumerator, {});
        }
    );

    It("Game content, directory with trash",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0001");

            CheckPresenceOfObjects(Enumerator, {});
        }
    );

    It("Game content, recursive directory with 1 class",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0002");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0002/Inside/Test_0002_C0.Test_0002_C0"
                }
            );
        }
    );

    It("Game content, directory from 2nd level with 1 class",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0002/Inside");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0002/Inside/Test_0002_C0.Test_0002_C0"
                }
            );
        }
    );

    It("Game content, directory from 2nd level (with ending `/`)"
        " with 1 class",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0002/Inside/");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0002/Inside/Test_0002_C0.Test_0002_C0"
                }
            );
        }
    );

    It("Game content, directory with all kinds of BP classes",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0003");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0003/Test_0003_C0.Test_0003_C0",
                    "/Game/Test_0003/Test_0003_C1.Test_0003_C1",
                    "/Game/Test_0003/Test_0003_C2.Test_0003_C2",
                    "/Game/Test_0003/Test_0003_C3.Test_0003_C3",
                    "/Game/Test_0003/Test_0003_C4.Test_0003_C4",
                    "/Game/Test_0003/Test_0003_C5.Test_0003_C5",
                    "/Game/Test_0003/Test_0003_C6.Test_0003_C6",
                    "/Game/Test_0003/Test_0003_C7.Test_0003_C7",
                    "/Game/Test_0003/Test_0003_C8.Test_0003_C8",
                    "/Game/Test_0003/Test_0003_C9.Test_0003_C9",
                    "/Game/Test_0003/Test_0003_C10.Test_0003_C10"
                }
            );
        }
    );

    It("Game content, directory with all BP class, struct and enum",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0004");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0004/Test_0004_C0.Test_0004_C0",
                    "/Game/Test_0004/Test_0004_E0.Test_0004_E0",
                    "/Game/Test_0004/Test_0004_S0.Test_0004_S0"
                }
            );
        }
    );

    It("Game content, highly recursive directory with 3 BP classes",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0005");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0005/Test_0005_C0.Test_0005_C0",
                    "/Game/Test_0005/Inside/Test_0005_S0.Test_0005_S0",
                    "/Game/Test_0005/Inside/Inside/Test_0005_E0.Test_0005_E0"
                }
            );
        }
    );

    It("Plugin content, recursive directory with a BP class and"
        " a struct",
        [this]()
        {
            FContentPathEnumerator Enumerator(
                "/XYZProductTests/Test_0006");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/XYZProductTests/Test_0006/Test_0006_C0.Test_0006_C0",
                    "/XYZProductTests/Test_0006/Inside/"
                        "Test_0006_E0.Test_0006_E0"
                }
            );
        }
    );

    It("Game content, directory with mixed trash and a BP class",
        [this]()
        {
            FContentPathEnumerator Enumerator("/Game/Test_0007");

            CheckPresenceOfObjects(Enumerator,
                {
                    "/Game/Test_0007/Test_0007_C0.Test_0007_C0"
                }
            );
        }
    );
}
