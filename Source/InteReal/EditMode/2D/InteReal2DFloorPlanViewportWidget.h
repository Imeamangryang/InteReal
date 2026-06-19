#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal2DFloorPlanTypes.h"
#include "InteReal2DFloorPlanViewportWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DDrawAreaClickedSignature, FVector2D, LocalPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DFurniturePlacementRequestedSignature, FVector2D, DocumentPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DFurniturePreviewMovedSignature, FVector2D, DocumentPosition);

USTRUCT(BlueprintType)
struct FInteReal2DPlacedFurniture
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    int32 FurnitureID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FVector2D CenterDocumentPosition = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FVector2D Size = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    float RotationDegrees = 0.0f;
};

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UInteReal2DFloorPlanViewportWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void SetDocument(const FInteReal2DFloorPlanDocument& InDocument);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void LoadFromHarnessFloorData(const FHarnessFloorData& InFloorData);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Layout")
    void SetDrawArea(const FVector2D& InDrawOffset, const FVector2D& InDrawSizeOverride);
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void CancelFurniturePlacement();

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void ClearPlacedFurnitures();
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void AddPlacedFurnitureAtDocumentPosition(
        const FFurnitureDataRow& FurnitureRow,
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void SetFurniturePreviewAtDocumentPosition(
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Input")
    FInteReal2DDrawAreaClickedSignature OnDrawAreaClicked;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DFurniturePlacementRequestedSignature OnFurniturePlacementRequested2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DFurniturePreviewMovedSignature OnFurniturePreviewMoved2D;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Input")
    FVector2D LastClickedLocalPosition = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Input")
    bool bLastClickInsideDrawArea = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ToolTip="도면이 위젯 내부에서 그려질 기준 위치 오프셋입니다."))
    FVector2D DrawOffset = FVector2D(30.0f, 110.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ToolTip="도면을 특정 크기 영역 안에 맞춰 그릴지 여부입니다."))
    bool bUseDrawSizeOverride = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(EditCondition="bUseDrawSizeOverride", ToolTip="도면이 맞춰질 가상 그리기 영역 크기입니다. X/Y가 0보다 커야 합니다."))
    FVector2D DrawSizeOverride = FVector2D(800.0f, 600.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ClampMin="-360.0", ClampMax="360.0", UIMin="-180.0", UIMax="180.0"))
    float DrawRotationDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bDrawBackground = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor BackgroundColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style", meta=(ClampMin="0.0", UIMin="0.0"))
    float BackgroundCornerRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float ViewPadding = 32.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float RoomLineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float OpeningLineThickness = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor RoomLineColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDoorColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningWindowColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDefaultColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bFlipYForScreenSpace = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurnitureFillColor = FLinearColor(0.1f, 0.45f, 1.0f, 0.25f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurnitureOutlineColor = FLinearColor(0.05f, 0.25f, 0.9f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurniturePreviewFillColor = FLinearColor(0.1f, 0.8f, 1.0f, 0.18f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurniturePreviewOutlineColor = FLinearColor(0.0f, 0.55f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture", meta=(ClampMin="0.0", UIMin="0.0"))
    float FurnitureOutlineThickness = 2.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Furniture")
    bool bIsPlacingFurniture2D = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Furniture")
    TArray<FInteReal2DPlacedFurniture> PlacedFurnitures2D;

protected:
    virtual void NativeConstruct() override;
    
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent
    ) override;
    
private:
    UFUNCTION()
    FEventReply HandleInputCatcherMouseButtonDown(
        FGeometry MyGeometry,
        const FPointerEvent& MouseEvent
    );
    
    UFUNCTION()
    FEventReply HandleInputCatcherMouseMove(
        FGeometry MyGeometry,
        const FPointerEvent& MouseEvent
    );
    
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<class UBorder> InputCatcherBorder;
    
    UPROPERTY()
    FInteReal2DFloorPlanDocument Document;
    
    FVector2D TransformDocumentPointToLocal(const FVector2D& DocPoint, const FVector2D& LocalSize) const;
    FVector2D TransformLocalPointToDocument(const FVector2D& LocalPoint, const FVector2D& LocalSize) const;
    FVector2D GetDocumentSize() const;
    FLinearColor ResolveOpeningColor(const FString& OpeningType) const;
    FVector2D GetDrawAreaOffset() const;
    FVector2D GetDrawAreaSize(const FVector2D& LocalSize) const;
    bool IsLocalPointInsideDrawArea(const FVector2D& LocalPoint, const FVector2D& LocalSize) const;
    void ApplyInputCatcherLayout(const FVector2D& LocalSize);
    void DrawFurnitureRect(
        FSlateWindowElementList& OutDrawElements,
        const FGeometry& AllottedGeometry,
        int32 LayerId,
        const FInteReal2DPlacedFurniture& Furniture,
        const FLinearColor& FillColor,
        const FLinearColor& OutlineColor
    ) const;
    
    UPROPERTY()
    FFurnitureDataRow PendingFurnitureRow;

    FVector2D PendingFurnitureSize = FVector2D::ZeroVector;
    FVector2D PreviewFurnitureCenterDocument = FVector2D::ZeroVector;
    bool bHasFurniturePreviewPosition = false;
};