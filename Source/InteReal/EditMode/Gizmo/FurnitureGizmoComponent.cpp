#include "FurnitureGizmoComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

UFurnitureGizmoComponent::UFurnitureGizmoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	RingMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RingMeshComp"));
	RingMeshComp->SetupAttachment(this);
	RingMeshComp->SetCastShadow(false);
	RingMeshComp->bReceivesDecals = false;
	RingMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RingMeshComp->SetCollisionObjectType(ECC_WorldDynamic);
	RingMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	RingMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	ArrowsMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ArrowsMeshComp"));
	ArrowsMeshComp->SetupAttachment(this);
	ArrowsMeshComp->SetCastShadow(false);
	ArrowsMeshComp->bReceivesDecals = false;
	ArrowsMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetGizmoVisible(false);
}

void UFurnitureGizmoComponent::SetupFromLocalBounds(FBox LocalBounds)
{
	if (!RingMeshComp || !ArrowsMeshComp) return;

	FVector Extents = LocalBounds.GetExtent();

	// XY=0 고정 — 메시 피벗 오프셋이 누적되어 가구 밖으로 날아가는 버그 차단
	SetRelativeLocation(FVector(0.f, 0.f, LocalBounds.Min.Z + 2.0f));

	float FurnitureSize = FMath::Max(Extents.X, Extents.Y);
	float RingRadius = FurnitureSize * 1.2f;
	float TubeRadius = 3.0f;

	BuildRotationRing(RingRadius, TubeRadius, 32, 8);
	BuildArrows(RingRadius * 0.8f, 2.0f);

	if (GizmoMaterial)
	{
		if (!RingDynMat)
		{
			RingDynMat = UMaterialInstanceDynamic::Create(GizmoMaterial, this);
			RingMeshComp->SetMaterial(0, RingDynMat);
		}
		if (!ArrowsDynMat)
		{
			ArrowsDynMat = UMaterialInstanceDynamic::Create(GizmoMaterial, this);
			ArrowsMeshComp->SetMaterial(0, ArrowsDynMat);
		}
	}

	SetGizmoColor(FLinearColor(0.f, 0.8f, 1.f, 1.f), 4.f);
}

void UFurnitureGizmoComponent::BuildRotationRing(float Radius, float TubeRadius, int32 Segments, int32 TubeSegments)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

	for (int32 i = 0; i <= Segments; ++i)
	{
		float Angle = (i % Segments) * 2.f * PI / Segments;
		FVector Center = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		FVector Dir = Center.GetSafeNormal();

		for (int32 j = 0; j <= TubeSegments; ++j)
		{
			float TubeAngle = j * 2.f * PI / TubeSegments;
			FVector TubeOffset = Dir * FMath::Cos(TubeAngle) * TubeRadius + FVector::UpVector * FMath::Sin(TubeAngle) *
				TubeRadius;

			Vertices.Add(Center + TubeOffset);
			Normals.Add(TubeOffset.GetSafeNormal());
			UVs.Add(FVector2D((float)i / Segments, (float)j / TubeSegments));

			if (i < Segments && j < TubeSegments)
			{
				int32 Cur = i * (TubeSegments + 1) + j;
				int32 Next = (i + 1) * (TubeSegments + 1) + j;

				Triangles.Add(Cur);
				Triangles.Add(Cur + 1);
				Triangles.Add(Next);
				Triangles.Add(Next);
				Triangles.Add(Cur + 1);
				Triangles.Add(Next + 1);
			}
		}
	}

	RingMeshComp->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
}

void UFurnitureGizmoComponent::BuildArrows(float Length, float ArrowRadius)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

	CreateCylinderMesh(Vertices, Triangles, Normals, FVector::ZeroVector, FVector(Length, 0.f, 0.f), ArrowRadius, 12);
	CreateCylinderMesh(Vertices, Triangles, Normals, FVector::ZeroVector, FVector(0.f, Length, 0.f), ArrowRadius, 12);

	ArrowsMeshComp->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
}

void UFurnitureGizmoComponent::CreateCylinderMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles,
                                                  TArray<FVector>& Normals,
                                                  FVector Start, FVector End, float Radius, int32 Segments)
{
	int32 BaseIdx = Vertices.Num();
	FVector Dir = (End - Start).GetSafeNormal();
	FVector Up = (FMath::Abs(Dir.Z) < 0.99f) ? FVector::UpVector : FVector::ForwardVector;
	FVector Right = FVector::CrossProduct(Dir, Up).GetSafeNormal();
	Up = FVector::CrossProduct(Right, Dir).GetSafeNormal();

	for (int32 i = 0; i <= Segments; ++i)
	{
		float Angle = (i % Segments) * 2.f * PI / Segments;
		FVector Offset = (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * Radius;

		Vertices.Add(Start + Offset);
		Normals.Add(Offset.GetSafeNormal());
		Vertices.Add(End + Offset);
		Normals.Add(Offset.GetSafeNormal());

		if (i < Segments)
		{
			int32 CS = BaseIdx + i * 2;
			int32 CE = CS + 1;
			int32 NS = BaseIdx + (i + 1) * 2;
			int32 NE = NS + 1;

			Triangles.Add(CS);
			Triangles.Add(NS);
			Triangles.Add(CE);
			Triangles.Add(CE);
			Triangles.Add(NS);
			Triangles.Add(NE);
		}
	}
}

void UFurnitureGizmoComponent::UpdateRadialRotationRing(float InBoundsMax, float DeltaAngleDeg)
{
	if (!RingMeshComp) return;

	TArray<FVector> Vertices;
	TArray<int32>   Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

	float RingRadius   = InBoundsMax * 1.2f;
	float TubeRadius   = 3.5f;
	int32 Segments     = 32;
	int32 TubeSegments = 8;

	float TargetRad = FMath::DegreesToRadians(DeltaAngleDeg);

	for (int32 i = 0; i <= Segments; ++i)
	{
		float Angle  = (i * TargetRad) / Segments;
		FVector Center = FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 0.f);
		FVector Dir    = Center.GetSafeNormal();

		for (int32 j = 0; j <= TubeSegments; ++j)
		{
			float TubeAngle    = j * 2.f * PI / TubeSegments;
			FVector TubeOffset = Dir * FMath::Cos(TubeAngle) * TubeRadius + FVector::UpVector * FMath::Sin(TubeAngle) * TubeRadius;

			Vertices.Add(Center + TubeOffset);
			Normals.Add(TubeOffset.GetSafeNormal());
			UVs.Add(FVector2D((float)i / Segments, (float)j / TubeSegments));

			if (i < Segments && j < TubeSegments)
			{
				int32 Cur  = i * (TubeSegments + 1) + j;
				int32 Next = (i + 1) * (TubeSegments + 1) + j;
				Triangles.Add(Cur);     Triangles.Add(Cur + 1);  Triangles.Add(Next);
				Triangles.Add(Next);    Triangles.Add(Cur + 1);  Triangles.Add(Next + 1);
			}
		}
	}

	// 섹션 0에 덮어쓰기 — 매 프레임 호출해도 실시간으로 늘어나는 링 표현
	RingMeshComp->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
}

void UFurnitureGizmoComponent::SetGizmoVisible(bool bIsVisible)
{
	if (RingMeshComp) RingMeshComp->SetVisibility(bIsVisible);
	if (ArrowsMeshComp) ArrowsMeshComp->SetVisibility(bIsVisible);
}

void UFurnitureGizmoComponent::SetGizmoColor(FLinearColor Color, float Intensity)
{
	if (RingDynMat)
	{
		RingDynMat->SetVectorParameterValue(TEXT("GizmoColor"), Color);
		RingDynMat->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}
	if (ArrowsDynMat)
	{
		ArrowsDynMat->SetVectorParameterValue(TEXT("GizmoColor"), Color);
		ArrowsDynMat->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}
}
