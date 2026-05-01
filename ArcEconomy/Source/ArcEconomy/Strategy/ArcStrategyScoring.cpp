// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/ArcStrategyScoring.h"
#include "Strategy/ArcStrategyActionSet.h"
#include "UtilityAI/ArcUtilityConsideration.h"

namespace ArcStrategy::Scoring
{
    FArcStrategyDecision EvaluateActionSet(
        const UArcStrategyActionSet& ActionSet,
        FArcUtilityContext& Context)
    {
        FArcUtilityTarget Target;

        const int32 NumActions = ActionSet.Actions.Num();
        if (NumActions == 0)
        {
            return FArcStrategyDecision();
        }

        TArray<float> ActionScores;
        ActionScores.SetNum(NumActions);

        for (int32 ActionIdx = 0; ActionIdx < NumActions; ++ActionIdx)
        {
            const FArcStrategyAction& Action = ActionSet.Actions[ActionIdx];
            const int32 NumConsiderations = Action.Considerations.Num();

            float Product = 1.0f;

            for (int32 ConsIdx = 0; ConsIdx < NumConsiderations; ++ConsIdx)
            {
                const FArcUtilityConsideration* Consideration = Action.Considerations[ConsIdx].GetPtr<FArcUtilityConsideration>();
                if (!Consideration)
                {
                    continue;
                }

                const float RawScore = Consideration->Score(Target, Context);
                const float CurvedScore = Consideration->ResponseCurve.Evaluate(FMath::Clamp(RawScore, 0.0f, 1.0f));
                const float CompensatedScore = FMath::Pow(FMath::Clamp(CurvedScore, 0.0f, 1.0f), Consideration->Weight);
                Product *= CompensatedScore;
            }

            // Geometric mean compensation and action weight
            float FinalScore = Product;
            if (NumConsiderations > 0)
            {
                FinalScore = FMath::Pow(Product, 1.0f / static_cast<float>(NumConsiderations));
            }
            FinalScore *= Action.Weight;

            ActionScores[ActionIdx] = FinalScore;
        }

        // Select winner based on selection mode
        int32 WinnerIndex = 0;
        float WinnerScore = 0.0f;

        if (ActionSet.SelectionMode == EArcUtilitySelectionMode::HighestScore)
        {
            for (int32 Idx = 0; Idx < NumActions; ++Idx)
            {
                if (ActionScores[Idx] > WinnerScore)
                {
                    WinnerScore = ActionScores[Idx];
                    WinnerIndex = Idx;
                }
            }
        }
        else if (ActionSet.SelectionMode == EArcUtilitySelectionMode::RandomFromTopPercent)
        {
            // Find best score
            float BestScore = 0.0f;
            for (int32 Idx = 0; Idx < NumActions; ++Idx)
            {
                if (ActionScores[Idx] > BestScore)
                {
                    BestScore = ActionScores[Idx];
                }
            }

            const float Threshold = BestScore * ActionSet.TopPercentThreshold;
            TArray<int32> Candidates;
            for (int32 Idx = 0; Idx < NumActions; ++Idx)
            {
                if (ActionScores[Idx] >= Threshold)
                {
                    Candidates.Add(Idx);
                }
            }

            if (Candidates.Num() > 0)
            {
                WinnerIndex = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
                WinnerScore = ActionScores[WinnerIndex];
            }
        }
        else if (ActionSet.SelectionMode == EArcUtilitySelectionMode::WeightedRandom)
        {
            float TotalScore = 0.0f;
            for (int32 Idx = 0; Idx < NumActions; ++Idx)
            {
                TotalScore += ActionScores[Idx];
            }

            if (TotalScore > UE_SMALL_NUMBER)
            {
                float Roll = FMath::FRandRange(0.0f, TotalScore);
                for (int32 Idx = 0; Idx < NumActions; ++Idx)
                {
                    Roll -= ActionScores[Idx];
                    if (Roll <= 0.0f)
                    {
                        WinnerIndex = Idx;
                        WinnerScore = ActionScores[Idx];
                        break;
                    }
                }
            }
        }

        FArcStrategyDecision Decision;
        Decision.ActionIndex = WinnerIndex;
        Decision.Intensity = FMath::Clamp(WinnerScore, 0.0f, 1.0f);
        return Decision;
    }
}
