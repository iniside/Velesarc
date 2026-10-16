// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/ArcTextureGraphBake.h"

#include "Blueprint/TG_AsyncRenderTask.h"
#include "TextureGraph.h"
#include "TG_Graph.h"
#include "TG_Node.h"
#include "Expressions/TG_Expression.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "PixelFormat.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTextureGraphBake, Log, All);

namespace ArcTextureGraphBake_Internal
{
    // Resolve the user-facing name of an Output node. The editable field on the node is
    // OutputSettings.BaseName ("File Name"); GetTitleName() is only the Output PIN alias, which
    // updates solely when the node TITLE is renamed (not when File Name is typed in the details
    // panel). OutputName is a non-editable mirror synced from the alias. Prefer BaseName (what the
    // author actually types), then the title alias, then OutputName. Returns NAME_None if all empty.
    FName ResolveOutputNodeName(const UTG_Expression_Output* OutputExpr)
    {
        if (!OutputExpr->OutputSettings.BaseName.IsNone())
        {
            return OutputExpr->OutputSettings.BaseName;
        }
        const FName TitleAlias = OutputExpr->GetTitleName();
        if (!TitleAlias.IsNone())
        {
            return TitleAlias;
        }
        return OutputExpr->OutputSettings.OutputName;
    }

    // The float pixel formats whose RT readback preserves height precision. 8-bit BGRA quantizes.
    bool IsFloatPixelFormat(EPixelFormat Format)
    {
        return Format == PF_R32_FLOAT
            || Format == PF_FloatRGBA
            || Format == PF_R16F
            || Format == PF_G16;
    }

    // Read one render target back to a single-channel CPU float field (.R channel).
    // ReadLinearColorPixels returns TOP-LEFT-origin rows. FArcBakedHeightField / FSampledScalarField2f
    // index as y*W + x with Y-up sampling, so we apply a single fixed vertical flip here.
    // Assumed TextureGraph output is top-left origin; flip to match field Y-up sampling.
    // VERIFY in-editor with a known vertical gradient (plan Step 3.6) — this flip is the single
    // place to change if the result comes out mirrored. Do NOT add an author-facing toggle.
    bool ReadRenderTargetToField(UTextureRenderTarget2D* RT, FArcBakedHeightField& OutField, FString& OutError)
    {
        FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
        if (Res == nullptr)
        {
            OutError = TEXT("render target has no game-thread resource");
            return false;
        }

        TArray<FLinearColor> Pixels;
        if (!Res->ReadLinearColorPixels(Pixels))
        {
            OutError = TEXT("ReadLinearColorPixels failed");
            return false;
        }

        const int32 W = RT->SizeX;
        const int32 H = RT->SizeY;
        if (W < 1 || H < 1 || Pixels.Num() != W * H)
        {
            OutError = FString::Printf(
                TEXT("readback size mismatch (W=%d H=%d Pixels=%d)"), W, H, Pixels.Num());
            return false;
        }

        OutField.Dims = FIntPoint(W, H);
        OutField.Values.SetNumUninitialized(W * H);

        for (int32 DestY = 0; DestY < H; ++DestY)
        {
            const int32 SrcRow = (H - 1) - DestY;
            for (int32 X = 0; X < W; ++X)
            {
                OutField.Values[DestY * W + X] = Pixels[SrcRow * W + X].R;
            }
        }

        return true;
    }
}

namespace ArcTextureGraphBake
{
    bool Bake(UTextureGraph* TextureGraph, FName HeightOutputName,
              FBakeResult& OutResult, FString& OutError)
    {
        // 1. Null/empty guards.
        if (TextureGraph == nullptr)
        {
            OutError = TEXT("TextureGraph is null");
            return false;
        }

        UTG_Graph* Graph = TextureGraph->Graph();
        if (Graph == nullptr)
        {
            OutError = TEXT("TextureGraph->Graph() is null");
            return false;
        }

        if (HeightOutputName.IsNone())
        {
            OutError = TEXT("HeightOutputName is empty");
            return false;
        }

        // 2. Name<->RT correlation: collect output node title names in ForEachNodes order BEFORE
        //    rendering. The render task duplicates the graph (StaticDuplicateObject preserves node
        //    array order) and builds OutputRts by the SAME ForEachNodes walk, so OutputRts[i]
        //    corresponds to OutputNames[i]. Never fall back to positional guessing.
        TArray<FName> OutputNames;
        Graph->ForEachNodes(
            [&OutputNames](const UTG_Node* Node, uint32 /*Index*/)
            {
                if (Node == nullptr)
                {
                    return;
                }

                const UTG_Expression_Output* OutputExpr =
                    Cast<UTG_Expression_Output>(Node->GetExpression());
                if (OutputExpr != nullptr)
                {
                    OutputNames.Add(ArcTextureGraphBake_Internal::ResolveOutputNodeName(OutputExpr));
                }
            });

        if (OutputNames.Num() == 0)
        {
            OutError = TEXT("TextureGraph has no Output nodes");
            return false;
        }

        // Always surface what we resolved so a name mismatch is debuggable without code spelunking.
        {
            FString NameList;
            for (int32 NameIdx = 0; NameIdx < OutputNames.Num(); ++NameIdx)
            {
                NameList += (NameIdx == 0 ? TEXT("") : TEXT(", "));
                NameList += FString::Printf(TEXT("'%s'"), *OutputNames[NameIdx].ToString());
            }
            UE_LOG(LogArcTextureGraphBake, Log,
                TEXT("TextureGraph has %d Output node(s): %s"), OutputNames.Num(), *NameList);

            if (!OutputNames.Contains(HeightOutputName))
            {
                OutError = FString::Printf(
                    TEXT("HeightOutputName '%s' is not among the graph's Output nodes [%s]. ")
                    TEXT("Use the Output node's \"File Name\" (BaseName), or its title if File Name is blank."),
                    *HeightOutputName.ToString(), *NameList);
                return false;
            }
        }

        // 3. Render (blocking on the game thread).
        UTG_AsyncRenderTask* Task = UTG_AsyncRenderTask::TG_AsyncRenderTask(TextureGraph);
        if (Task == nullptr)
        {
            OutError = TEXT("TG_AsyncRenderTask returned null");
            return false;
        }

        const TArray<UTextureRenderTarget2D*>& RTs = Task->ActivateBlocking(nullptr);

        if (RTs.Num() == 0)
        {
            Task->SetReadyToDestroy();
            OutError = TEXT("render produced no output render targets");
            return false;
        }

        // 2 (cont.) Count correlation gate — must match exactly or we cannot route names to RTs.
        if (OutputNames.Num() != RTs.Num())
        {
            OutError = FString::Printf(
                TEXT("output node/RT count mismatch — cannot correlate (names=%d RTs=%d)"),
                OutputNames.Num(), RTs.Num());
            Task->SetReadyToDestroy();
            return false;
        }

        bool bSawHeight = false;
        bool bSuccess = true;

        for (int32 Index = 0; Index < RTs.Num(); ++Index)
        {
            UTextureRenderTarget2D* RT = RTs[Index];
            const FName Name = OutputNames[Index];

            if (RT == nullptr)
            {
                OutError = FString::Printf(
                    TEXT("output '%s' has a null render target"), *Name.ToString());
                bSuccess = false;
                break;
            }

            const bool bIsHeight = (Name == HeightOutputName);

            // 4. Format/precision guard — height only.
            if (bIsHeight && !ArcTextureGraphBake_Internal::IsFloatPixelFormat(RT->GetFormat()))
            {
                UE_LOG(LogArcTextureGraphBake, Warning,
                    TEXT("Height Output node '%s' is a NON-float pixel format (%d). Set that ")
                    TEXT("Output node to R32F — 8-bit BGRA silently quantizes height."),
                    *Name.ToString(), static_cast<int32>(RT->GetFormat()));
            }

            // 5/6. Readback (with fixed vertical flip).
            FArcBakedHeightField Field;
            FString ReadError;
            if (!ArcTextureGraphBake_Internal::ReadRenderTargetToField(RT, Field, ReadError))
            {
                OutError = FString::Printf(
                    TEXT("output '%s' readback failed: %s"), *Name.ToString(), *ReadError);
                bSuccess = false;
                break;
            }

            // 7. Route results.
            if (bIsHeight)
            {
                // 8. Validate the height field (>= 2x2, buffer size matches Dims).
                if (!Field.IsValid())
                {
                    OutError = FString::Printf(
                        TEXT("height output '%s' produced an invalid field (Dims=%dx%d)"),
                        *Name.ToString(), Field.Dims.X, Field.Dims.Y);
                    bSuccess = false;
                    break;
                }
                OutResult.Height = MoveTemp(Field);
                bSawHeight = true;
            }
            else
            {
                if (!Field.IsValid())
                {
                    OutError = FString::Printf(
                        TEXT("layer output '%s' produced an invalid field (Dims=%dx%d)"),
                        *Name.ToString(), Field.Dims.X, Field.Dims.Y);
                    bSuccess = false;
                    break;
                }
                OutResult.Layers.Add(Name, MoveTemp(Field));
            }
        }

        // 3 (cont.) Release the task (created RF_Standalone — must release or it leaks).
        Task->SetReadyToDestroy();

        if (!bSuccess)
        {
            return false;
        }

        if (!bSawHeight)
        {
            // Should be unreachable given the earlier Contains() check + count gate, but stay safe.
            OutError = FString::Printf(
                TEXT("height output '%s' was never produced by the render"),
                *HeightOutputName.ToString());
            return false;
        }

        return true;
    }
}
