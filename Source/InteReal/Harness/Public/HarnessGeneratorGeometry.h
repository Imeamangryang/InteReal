#pragma once

#include "CoreMinimal.h"

namespace InteReal::HarnessGenerator
{
    inline constexpr float HarnessDefaultWallThicknessCm = 20.0f;
    inline constexpr float HarnessDefaultWallHeightCm = 300.0f;
    inline constexpr float HarnessFloorSlabThicknessCm = 20.0f;
    inline constexpr float HarnessCoreEndOverlapCm = 0.0f;
    inline constexpr float HarnessCoreZSealCm = 2.0f;
    inline constexpr float HarnessSurfaceGapCm = 0.2f;
    inline constexpr float HarnessSurfaceVerticalGapCm = 0.25f;
    inline constexpr float HarnessWallZFightSeparationCm = 0.25f;
    inline constexpr float HarnessCeilingShadowOverhangCm = 25.0f;
    inline constexpr float HarnessMergeEndpointToleranceCm = 1.0f;
    inline constexpr float HarnessMergeCollinearTolerance = 0.01f;
    inline constexpr bool bHarnessSurfaceBuildDebug = false;

    inline FString MakeHarnessSurfaceToken(const FString& Value)
    {
        FString Token = Value;
        Token.TrimStartAndEndInline();
        Token.ReplaceInline(TEXT(" "), TEXT("-"));
        return Token.IsEmpty() ? FString(TEXT("Unknown")) : Token;
    }

    inline FString JoinHarnessTagNames(const TArray<FName>& Tags)
    {
        TArray<FString> TagStrings;
        TagStrings.Reserve(Tags.Num());
        for (const FName& Tag : Tags)
        {
            TagStrings.Add(Tag.ToString());
        }

        return TagStrings.Num() > 0 ? FString::Join(TagStrings, TEXT(", ")) : FString(TEXT("<none>"));
    }

    inline double ComputeHarnessSignedArea(const TArray<FVector2D>& Points)
    {
        double SignedArea = 0.0;
        const int32 NumPts = Points.Num();
        for (int32 i = 0; i < NumPts; ++i)
        {
            const FVector2D& P1 = Points[i];
            const FVector2D& P2 = Points[(i + 1) % NumPts];
            SignedArea += (P1.X * P2.Y) - (P2.X * P1.Y);
        }
        return SignedArea;
    }

    inline bool IsPointInsideHarnessPolygon2D(const TArray<FVector2D>& Points, const FVector2D& Point)
    {
        bool bInside = false;
        const int32 NumPts = Points.Num();
        if (NumPts < 3)
        {
            return false;
        }

        for (int32 i = 0, j = NumPts - 1; i < NumPts; j = i++)
        {
            const FVector2D& A = Points[i];
            const FVector2D& B = Points[j];
            const bool bStraddlesY = (A.Y > Point.Y) != (B.Y > Point.Y);
            if (!bStraddlesY)
            {
                continue;
            }

            const float IntersectX = ((B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y)) + A.X;
            if (Point.X < IntersectX)
            {
                bInside = !bInside;
            }
        }

        return bInside;
    }

    inline FVector2D ComputeHarnessInteriorNormal2D(const TArray<FVector2D>& FacePoints, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D Segment = B - A;
        const FVector2D SegmentDir = Segment.GetSafeNormal();
        if (SegmentDir.IsNearlyZero())
        {
            return FVector2D::ZeroVector;
        }

        const FVector2D LeftNormal(-SegmentDir.Y, SegmentDir.X);
        const FVector2D MidPoint = (A + B) * 0.5f;
        const float ProbeDistance = FMath::Max(HarnessMergeEndpointToleranceCm * 3.0f, 3.0f);

        const bool bLeftInside = IsPointInsideHarnessPolygon2D(FacePoints, MidPoint + (LeftNormal * ProbeDistance));
        const bool bRightInside = IsPointInsideHarnessPolygon2D(FacePoints, MidPoint - (LeftNormal * ProbeDistance));
        if (bLeftInside != bRightInside)
        {
            return bLeftInside ? LeftNormal : -LeftNormal;
        }

        const bool bFaceIsCCW = ComputeHarnessSignedArea(FacePoints) > 0.0;
        return bFaceIsCCW ? LeftNormal : -LeftNormal;
    }

    inline float CrossHarness2D(const FVector2D& A, const FVector2D& B)
    {
        return (A.X * B.Y) - (A.Y * B.X);
    }

    inline bool IntersectHarnessLines2D(const FVector2D& P, const FVector2D& R, const FVector2D& Q, const FVector2D& S, FVector2D& OutPoint)
    {
        const float Denom = CrossHarness2D(R, S);
        if (FMath::Abs(Denom) <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const float T = CrossHarness2D(Q - P, S) / Denom;
        OutPoint = P + (R * T);
        return true;
    }

    inline TArray<FVector2D> OffsetHarnessPolygon2D(const TArray<FVector2D>& Points, float Offset)
    {
        const int32 NumPoints = Points.Num();
        if (NumPoints < 3 || Offset <= 0.0f)
        {
            return Points;
        }

        const bool bCCW = ComputeHarnessSignedArea(Points) > 0.0;
        TArray<FVector2D> Result;
        Result.Reserve(NumPoints);

        auto GetOutwardNormal = [bCCW](const FVector2D& Dir) -> FVector2D
        {
            return bCCW ? FVector2D(Dir.Y, -Dir.X) : FVector2D(-Dir.Y, Dir.X);
        };

        for (int32 i = 0; i < NumPoints; ++i)
        {
            const FVector2D& Prev = Points[(i - 1 + NumPoints) % NumPoints];
            const FVector2D& Curr = Points[i];
            const FVector2D& Next = Points[(i + 1) % NumPoints];

            const FVector2D PrevDir = (Curr - Prev).GetSafeNormal();
            const FVector2D NextDir = (Next - Curr).GetSafeNormal();
            if (PrevDir.IsNearlyZero() || NextDir.IsNearlyZero())
            {
                Result.Add(Curr);
                continue;
            }

            const FVector2D PrevOut = GetOutwardNormal(PrevDir);
            const FVector2D NextOut = GetOutwardNormal(NextDir);
            FVector2D MiterPoint;
            if (IntersectHarnessLines2D(Curr + PrevOut * Offset, PrevDir, Curr + NextOut * Offset, NextDir, MiterPoint))
            {
                const float MaxMiter = Offset * 4.0f;
                const FVector2D Delta = MiterPoint - Curr;
                Result.Add(Delta.SizeSquared() > FMath::Square(MaxMiter) ? Curr + Delta.GetSafeNormal() * MaxMiter : MiterPoint);
            }
            else
            {
                const FVector2D Fallback = (PrevOut + NextOut).GetSafeNormal();
                Result.Add(Curr + (Fallback.IsNearlyZero() ? PrevOut : Fallback) * Offset);
            }
        }

        return Result;
    }
}
