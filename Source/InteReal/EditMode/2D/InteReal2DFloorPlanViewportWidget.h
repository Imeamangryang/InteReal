#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteReal2DFloorPlanTypes.h"
#include "InteReal2DFloorPlanViewportWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UInteReal2DFloorPlanViewportWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void SetDocument(const FInteReal2DFloorPlanDocument& InDocument);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void LoadFromHarnessFloorData(const FHarnessFloorData& InFloorData);

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float ViewPadding = 32.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float RoomLineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float OpeningLineThickness = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor RoomLineColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDoorColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningWindowColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDefaultColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bFlipYForScreenSpace = true;

protected:
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

private:
    UPROPERTY()
    FInteReal2DFloorPlanDocument Document;

private:
    FVector2D TransformDocumentPointToLocal(const FVector2D& DocPoint, const FVector2D& LocalSize) const;
    FVector2D GetDocumentSize() const;
    FLinearColor ResolveOpeningColor(const FString& OpeningType) const;
};