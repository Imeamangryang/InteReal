#include "InteReal/Harness/Public/HarnessJsonParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Public/HarnessGeneratorGeometry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using namespace InteReal::HarnessGenerator;

constexpr float MmToCm = 0.1f;
constexpr TCHAR ExpectedTopologySchemaVersion[] = TEXT("3.2");

// Spatial merge tolerance for JSON vertex reuse.
// Use the same 3 cm tolerance as wall endpoint/run matching so near-identical
// room boundary points from the server are stitched together more reliably.
constexpr float VertexMergeTolerance = 3.0f;

// Grid cell size for spatial hash. Keep this at least as large as the merge tolerance so the 3x3 neighbour search can find every point within tolerance.
constexpr float SpatialCellSize = VertexMergeTolerance;

// Opening width diagnostics. Server JSON v3.2 can carry both span.start/end and measurement.opening_width_mm.
// Small differences are expected from rounding, but larger deltas should be visible in logs.
constexpr float OpeningWidthMismatchWarningCm = 2.0f;
constexpr float OpeningWidthMismatchStrongWarningCm = 5.0f;

// Opening host-wall diagnostics. These checks do not change placement; they only make
// server JSON / Unreal interpretation mismatches visible in the log.
constexpr float OpeningHostWallLateralWarningCm = 5.0f;
constexpr float OpeningHostWallLateralStrongWarningCm = 15.0f;
constexpr float OpeningHostWallParallelWarningCross = 0.10f;
constexpr float OpeningHostWallOutsideWarningCm = 3.0f;

// ---------------------------------------------------------------------------
//  JSON helpers
// ---------------------------------------------------------------------------

bool DeserializeJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

bool ReadPointMm(const TSharedPtr<FJsonObject>& Object, FVector2D& OutPointCm)
{
	if (!Object.IsValid())
	{
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	if (!Object->TryGetNumberField(TEXT("x"), X) || !Object->TryGetNumberField(TEXT("y"), Y))
	{
		return false;
	}

	OutPointCm = FVector2D(static_cast<float>(X) * MmToCm, static_cast<float>(Y) * MmToCm);
	return true;
}


bool TryReadPointFieldMm(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector2D& OutPointCm)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PointObject = nullptr;
	return Object->TryGetObjectField(Field, PointObject) && PointObject && PointObject->IsValid() && ReadPointMm(*PointObject, OutPointCm);
}

bool TryReadDirectionField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector2D& OutDirection)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DirectionObject = nullptr;
	if (!Object->TryGetObjectField(Field, DirectionObject) || !DirectionObject || !DirectionObject->IsValid())
	{
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	if (!(*DirectionObject)->TryGetNumberField(TEXT("x"), X) || !(*DirectionObject)->TryGetNumberField(TEXT("y"), Y))
	{
		return false;
	}

	OutDirection = FVector2D(static_cast<float>(X), static_cast<float>(Y)).GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}

float ReadNumberMmAsCm(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float DefaultCm = 0.0f)
{
	if (!Object.IsValid())
	{
		return DefaultCm;
	}

	double ValueMm = 0.0;
	return Object->TryGetNumberField(Field, ValueMm)
		? static_cast<float>(ValueMm) * MmToCm
		: DefaultCm;
}

bool TryReadNumberMmAsCm(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValueCm)
{
	if (!Object.IsValid())
	{
		return false;
	}

	double ValueMm = 0.0;
	if (!Object->TryGetNumberField(Field, ValueMm))
	{
		return false;
	}

	OutValueCm = static_cast<float>(ValueMm) * MmToCm;
	return true;
}

bool TryReadNestedNumberMmAsCm(const TSharedPtr<FJsonObject>& Object, const TCHAR* ObjectField, const TCHAR* NumberField, float& OutValueCm)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* NestedObject = nullptr;
	return Object->TryGetObjectField(ObjectField, NestedObject) && NestedObject && NestedObject->IsValid() && TryReadNumberMmAsCm(*NestedObject, NumberField, OutValueCm);
}

FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& DefaultValue = FString())
{
	if (!Object.IsValid())
	{
		return DefaultValue;
	}

	FString Value;
	return Object->TryGetStringField(Field, Value) ? Value : DefaultValue;
}

FString ReadFirstStringField(const TSharedPtr<FJsonObject>& Object, const TArray<const TCHAR*>& Fields, const FString& DefaultValue = FString())
{
	if (!Object.IsValid())
	{
		return DefaultValue;
	}

	for (const TCHAR* Field : Fields)
	{
		if (!Field)
		{
			continue;
		}

		FString Value;
		if (Object->TryGetStringField(Field, Value) && !Value.IsEmpty())
		{
			return Value;
		}
	}
	return DefaultValue;
}


void ReadStringArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& OutValues)
{
	OutValues.Reset();
	if (!Object.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue) && !StringValue.IsEmpty())
		{
			OutValues.Add(StringValue);
		}
	}
}

FString ReadOpeningAnchorWallId(const TSharedPtr<FJsonObject>& OpeningObject)
{
	const TSharedPtr<FJsonObject>* AnchorsObject = nullptr;
	if (!OpeningObject.IsValid() ||
		!OpeningObject->TryGetObjectField(TEXT("anchors"), AnchorsObject) ||
		!AnchorsObject ||
		!AnchorsObject->IsValid())
	{
		return FString();
	}

	auto ReadAnchorWallId = [](const TSharedPtr<FJsonObject>& Anchors, const TCHAR* Field) -> FString
	{
		const TSharedPtr<FJsonObject>* AnchorObject = nullptr;
		if (Anchors->TryGetObjectField(Field, AnchorObject) && AnchorObject && AnchorObject->IsValid())
		{
			return ReadStringField(*AnchorObject, TEXT("wall_id"));
		}
		return FString();
	};

	const FString StartWallId = ReadAnchorWallId(*AnchorsObject, TEXT("start"));
	return StartWallId.IsEmpty() ? ReadAnchorWallId(*AnchorsObject, TEXT("end")) : StartWallId;
}

// ---------------------------------------------------------------------------
//  Spatial hash for vertex merging
// ---------------------------------------------------------------------------

struct FSpatialVertexHash
{
	TMap<int64, TArray<int32>> CellMap; // cell key → indices into VertexArray
	TArray<FTopologyVertex>* VertexArray = nullptr;

	void Init(TArray<FTopologyVertex>& InVertices)
	{
		VertexArray = &InVertices;
	}

	static int64 MakeCellKey(int32 CellX, int32 CellY)
	{
		return (static_cast<int64>(CellX) << 32) | static_cast<int64>(static_cast<uint32>(CellY));
	}

	static int32 ToCell(float Coord)
	{
		return FMath::FloorToInt32(Coord / SpatialCellSize);
	}

	/**
	 * Find or add a vertex at the given position. If an existing vertex is within
	 * VertexMergeTolerance, returns its ID. Otherwise adds a new vertex with NewId.
	 */
	FString FindOrAdd(float X, float Y, const FString& NewId, bool* bOutAdded = nullptr)
	{
		if (bOutAdded)
		{
			*bOutAdded = false;
		}
		const int32 CX = ToCell(X);
		const int32 CY = ToCell(Y);

		// Search the 3×3 neighbourhood
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				const int64 Key = MakeCellKey(CX + DX, CY + DY);
				if (const TArray<int32>* Indices = CellMap.Find(Key))
				{
					for (const int32 Idx : *Indices)
					{
						const FTopologyVertex& Existing = (*VertexArray)[Idx];
						const float DistSq = FMath::Square(Existing.x - X) + FMath::Square(Existing.y - Y);
						if (DistSq <= FMath::Square(VertexMergeTolerance))
						{
							return Existing.id;
						}
					}
				}
			}
		}

		// No match – add new vertex
		if (bOutAdded)
		{
			*bOutAdded = true;
		}
		FTopologyVertex NewVertex;
		NewVertex.id = NewId;
		NewVertex.x = X;
		NewVertex.y = Y;

		const int32 NewIndex = VertexArray->Add(NewVertex);

		const int64 Key = MakeCellKey(CX, CY);
		CellMap.FindOrAdd(Key).Add(NewIndex);

		return NewId;
	}
};

// ---------------------------------------------------------------------------
//  Validation
// ---------------------------------------------------------------------------

bool ValidateTopologyRoot(const TSharedPtr<FJsonObject>& RootObject, FString& OutError)
{
	FString SchemaVersion;
	if (!RootObject->TryGetStringField(TEXT("schema_version"), SchemaVersion))
	{
		OutError = FString::Printf(TEXT("Missing required schema_version. Harness topology import supports only schema_version='%s'."), ExpectedTopologySchemaVersion);
		return false;
	}

	if (!SchemaVersion.Equals(ExpectedTopologySchemaVersion, ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(TEXT("Unsupported topology schema_version='%s'. Harness topology import supports only schema_version='%s'."), *SchemaVersion, ExpectedTopologySchemaVersion);
		return false;
	}

	FString Unit;
	if (!RootObject->TryGetStringField(TEXT("unit"), Unit) || !Unit.Equals(TEXT("mm"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Unsupported topology unit. Harness topology import requires unit='mm'.");
		return false;
	}

	const TSharedPtr<FJsonObject>* CoordinateSystem = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("coordinate_system"), CoordinateSystem) || !CoordinateSystem || !CoordinateSystem->IsValid())
	{
		OutError = TEXT("Missing required coordinate_system object.");
		return false;
	}

	const FString Origin = ReadStringField(*CoordinateSystem, TEXT("origin"));
	const FString XAxis = ReadStringField(*CoordinateSystem, TEXT("x_axis"));
	const FString YAxis = ReadStringField(*CoordinateSystem, TEXT("y_axis"));
	double RotationDegrees = 0.0;
	(*CoordinateSystem)->TryGetNumberField(TEXT("rotation_degrees"), RotationDegrees);

	if (!Origin.Equals(TEXT("top_left"), ESearchCase::IgnoreCase) ||
		!XAxis.Equals(TEXT("right"), ESearchCase::IgnoreCase) ||
		!YAxis.Equals(TEXT("down"), ESearchCase::IgnoreCase) ||
		!FMath::IsNearlyZero(static_cast<float>(RotationDegrees), KINDA_SMALL_NUMBER))
	{
		OutError = TEXT("Unsupported coordinate_system. Harness topology import requires top_left, x_axis=right, y_axis=down, rotation_degrees=0.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Spaces = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("walls"), Walls) || !Walls || Walls->Num() == 0)
	{
		OutError = TEXT("Missing required non-empty walls array.");
		return false;
	}
	if (!RootObject->TryGetArrayField(TEXT("spaces"), Spaces) || !Spaces || Spaces->Num() == 0)
	{
		OutError = TEXT("Missing required non-empty spaces array.");
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
//  ID helpers
// ---------------------------------------------------------------------------

FString MakeWallStartVertexId(const FString& WallId)
{
	return FString::Printf(TEXT("%s_start"), *WallId);
}

FString MakeWallEndVertexId(const FString& WallId)
{
	return FString::Printf(TEXT("%s_end"), *WallId);
}

FString MakeWallEdgeId(const FString& WallId)
{
	return FString::Printf(TEXT("%s_edge"), *WallId);
}

FString MakeWallTwinEdgeId(const FString& WallId)
{
	return FString::Printf(TEXT("%s_edge_twin"), *WallId);
}

FString NormalizeOpeningKindToType(const FString& Kind)
{
	const FString Lower = Kind.ToLower();
	if (Lower.Equals(TEXT("door")) ||
		Lower.Equals(TEXT("entrance_door")) ||
		Lower.Equals(TEXT("entry_door")) ||
		Lower.Equals(TEXT("front_door")) ||
		Lower.Equals(TEXT("interior_door")) ||
		Lower.Equals(TEXT("sliding_door")) ||
		Lower.Equals(TEXT("pocket_door")) ||
		Lower.Equals(TEXT("swing_door")) ||
		Lower.EndsWith(TEXT("_door")) ||
		Lower.Contains(TEXT("door")))
	{
		return TEXT("Door");
	}

	if (Lower.Equals(TEXT("window")) ||
		Lower.Equals(TEXT("fixed_window")) ||
		Lower.Equals(TEXT("sliding_window")) ||
		Lower.Equals(TEXT("casement_window")) ||
		Lower.Equals(TEXT("picture_window")) ||
		Lower.EndsWith(TEXT("_window")) ||
		Lower.Contains(TEXT("window")))
	{
		return TEXT("Window");
	}

	if (Lower.Equals(TEXT("opening")) ||
		Lower.Equals(TEXT("passage")) ||
		Lower.Equals(TEXT("passageway")) ||
		Lower.Equals(TEXT("arch")) ||
		Lower.Equals(TEXT("archway")) ||
		Lower.Equals(TEXT("portal")))
	{
		return TEXT("Opening");
	}

	return TEXT("Unknown");
}

bool IsWindowLikeOpeningType(const FString& Type)
{
	return Type.Equals(TEXT("Window"), ESearchCase::IgnoreCase);
}

float GetDefaultOpeningHeightCm(const FString& Type)
{
	return IsWindowLikeOpeningType(Type) ? 120.0f : 210.0f;
}

float GetDefaultOpeningBottomCm(const FString& Type)
{
	return IsWindowLikeOpeningType(Type) ? 90.0f : 0.0f;
}

float GetDefaultOpeningWidthCm(const FString& Type)
{
	return IsWindowLikeOpeningType(Type) ? 80.0f : 90.0f;
}

bool IsExternalConnectToken(const FString& ConnectId)
{
	const FString Lower = ConnectId.ToLower();
	return Lower.Equals(TEXT("exterior")) ||
		Lower.Equals(TEXT("external")) ||
		Lower.Equals(TEXT("outside")) ||
		Lower.Equals(TEXT("outdoor")) ||
		Lower.Contains(TEXT("exterior")) ||
		Lower.Contains(TEXT("external")) ||
		Lower.Contains(TEXT("outside")) ||
		Lower.Contains(TEXT("outdoor")) ||
		Lower.Contains(TEXT("balcony")) ||
		Lower.Contains(TEXT("terrace")) ||
		Lower.Contains(TEXT("veranda"));
}

void ClassifyOpeningConnects(FTopologyOpening& Opening, const TSet<FString>& SpaceIds)
{
	Opening.connected_space_count = 0;
	Opening.external_connect_count = 0;
	Opening.unknown_connect_count = 0;
	Opening.bConnectsExterior = false;
	Opening.bConnectsInterior = false;
	Opening.bConnectsSingleSpace = false;
	Opening.connection_type = TEXT("Unspecified");

	for (const FString& ConnectId : Opening.connects)
	{
		if (SpaceIds.Contains(ConnectId))
		{
			++Opening.connected_space_count;
		}
		else if (IsExternalConnectToken(ConnectId))
		{
			++Opening.external_connect_count;
		}
		else
		{
			++Opening.unknown_connect_count;
		}
	}

	Opening.bConnectsExterior = Opening.external_connect_count > 0;
	Opening.bConnectsInterior = !Opening.bConnectsExterior && Opening.connected_space_count >= 2;
	Opening.bConnectsSingleSpace = !Opening.bConnectsExterior && Opening.connected_space_count == 1;

	if (Opening.bConnectsExterior)
	{
		Opening.connection_type = TEXT("Exterior");
	}
	else if (Opening.bConnectsInterior)
	{
		Opening.connection_type = TEXT("Interior");
	}
	else if (Opening.bConnectsSingleSpace)
	{
		Opening.connection_type = TEXT("Boundary");
	}
	else if (Opening.connects.Num() > 0)
	{
		Opening.connection_type = TEXT("Unknown");
	}
}

FString JoinStringArrayForLog(const TArray<FString>& Values)
{
	return Values.Num() > 0 ? FString::Join(Values, TEXT(",")) : TEXT("None");
}

FString NormalizeWallKindToType(const FString& Kind)
{
	return Kind.Equals(TEXT("outer"), ESearchCase::IgnoreCase) ? TEXT("WallOuter") : TEXT("WallInner");
}

void LogOpeningWidthDiagnostics(const FTopologyOpening& Opening, bool bHasLegacyWidth, float LegacyWidthCm)
{
	const float SpanGeometryWidthCm = Opening.bHasSpan ? FVector2D::Distance(Opening.span_start_cm, Opening.span_end_cm) : 0.0f;
	const bool bHasSpanWidth = SpanGeometryWidthCm > UE_SMALL_NUMBER;
	const bool bHasMeasuredWidth = Opening.measured_width_cm > UE_SMALL_NUMBER;
	const bool bHasDrawnWidth = Opening.drawn_width_cm > UE_SMALL_NUMBER;

	if (bHasSpanWidth && bHasMeasuredWidth)
	{
		const float DeltaCm = FMath::Abs(SpanGeometryWidthCm - Opening.measured_width_cm);
		if (DeltaCm >= OpeningWidthMismatchWarningCm)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening width mismatch. OpeningId=%s Kind=%s Type=%s HostWallId=%s SpanWidth=%.2fcm MeasuredWidth=%.2fcm SelectedWidth=%.2fcm Delta=%.2fcm Severity=%s"),
				*Opening.id,
				*Opening.kind,
				*Opening.type,
				*Opening.host_wall_id,
				SpanGeometryWidthCm,
				Opening.measured_width_cm,
				Opening.width_cm,
				DeltaCm,
				DeltaCm >= OpeningWidthMismatchStrongWarningCm ? TEXT("StrongWarning") : TEXT("Warning"));
		}
	}

	if (bHasSpanWidth && bHasDrawnWidth)
	{
		const float DrawnDeltaCm = FMath::Abs(SpanGeometryWidthCm - Opening.drawn_width_cm);
		if (DrawnDeltaCm >= OpeningWidthMismatchWarningCm)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening drawn width mismatch. OpeningId=%s Kind=%s HostWallId=%s SpanGeometryWidth=%.2fcm DrawnWidth=%.2fcm Delta=%.2fcm Severity=%s"),
				*Opening.id,
				*Opening.kind,
				*Opening.host_wall_id,
				SpanGeometryWidthCm,
				Opening.drawn_width_cm,
				DrawnDeltaCm,
				DrawnDeltaCm >= OpeningWidthMismatchStrongWarningCm ? TEXT("StrongWarning") : TEXT("Warning"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening width source. OpeningId=%s Kind=%s Type=%s HostWallId=%s SpanWidth=%.2fcm DrawnWidth=%.2fcm MeasuredWidth=%.2fcm LegacyWidth=%s SelectedWidth=%.2fcm"),
		*Opening.id,
		*Opening.kind,
		*Opening.type,
		*Opening.host_wall_id,
		SpanGeometryWidthCm,
		Opening.drawn_width_cm,
		Opening.measured_width_cm,
		bHasLegacyWidth ? *FString::Printf(TEXT("%.2fcm"), LegacyWidthCm) : TEXT("None"),
		Opening.width_cm);
}


void LogOpeningHostWallDiagnostics(const FHarnessFloorData& Data)
{
	TMap<FString, const FTopologyVertex*> VertexById;
	for (const FTopologyVertex& Vertex : Data.vertices)
	{
		VertexById.Add(Vertex.id, &Vertex);
	}

	TMap<FString, const FTopologyHalfEdge*> PrimaryEdgeByWallId;
	for (const FTopologyHalfEdge& Edge : Data.half_edges)
	{
		if (Edge.id.Equals(MakeWallEdgeId(Edge.wall_id), ESearchCase::IgnoreCase))
		{
			PrimaryEdgeByWallId.Add(Edge.wall_id, &Edge);
		}
	}

	int32 CheckedCount = 0;
	int32 MissingHostCount = 0;
	int32 MissingSpanCount = 0;
	int32 LateralWarningCount = 0;
	int32 ParallelWarningCount = 0;
	int32 OutsideWarningCount = 0;

	for (const FTopologyOpening& Opening : Data.openings)
	{
		if (Opening.host_wall_id.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening host wall missing. OpeningId=%s Kind=%s"), *Opening.id, *Opening.kind);
			MissingHostCount++;
			continue;
		}

		const FTopologyHalfEdge* const* EdgePtr = PrimaryEdgeByWallId.Find(Opening.host_wall_id);
		if (!EdgePtr || !*EdgePtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening host wall not found. OpeningId=%s Kind=%s HostWallId=%s"), *Opening.id, *Opening.kind, *Opening.host_wall_id);
			MissingHostCount++;
			continue;
		}

		if (!Opening.bHasSpan)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening span missing. OpeningId=%s Kind=%s HostWallId=%s"), *Opening.id, *Opening.kind, *Opening.host_wall_id);
			MissingSpanCount++;
			continue;
		}

		const FTopologyHalfEdge& Edge = **EdgePtr;
		const FTopologyVertex* const* StartVertexPtr = VertexById.Find(Edge.vertex_start);
		const FTopologyVertex* const* EndVertexPtr = VertexById.Find(Edge.vertex_end);
		if (!StartVertexPtr || !*StartVertexPtr || !EndVertexPtr || !*EndVertexPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening host wall vertices missing. OpeningId=%s HostWallId=%s EdgeId=%s StartVertex=%s EndVertex=%s"), *Opening.id, *Opening.host_wall_id, *Edge.id, *Edge.vertex_start, *Edge.vertex_end);
			MissingHostCount++;
			continue;
		}

		const FVector2D WallStart((*StartVertexPtr)->x, (*StartVertexPtr)->y);
		const FVector2D WallEnd((*EndVertexPtr)->x, (*EndVertexPtr)->y);
		const FVector2D WallVector = WallEnd - WallStart;
		const float WallLength = WallVector.Size();
		if (WallLength <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening host wall has zero length. OpeningId=%s HostWallId=%s"), *Opening.id, *Opening.host_wall_id);
			MissingHostCount++;
			continue;
		}

		const FVector2D WallDirection = WallVector / WallLength;
		const FVector2D WallNormal(-WallDirection.Y, WallDirection.X);
		const FVector2D SpanVector = Opening.span_end_cm - Opening.span_start_cm;
		const float SpanLength = SpanVector.Size();
		const FVector2D SpanDirection = SpanLength > KINDA_SMALL_NUMBER ? SpanVector / SpanLength : WallDirection;
		const FVector2D SpanCenter = (Opening.span_start_cm + Opening.span_end_cm) * 0.5f;
		const float CenterAlong = FVector2D::DotProduct(SpanCenter - WallStart, WallDirection);
		const float LateralDistance = FMath::Abs(FVector2D::DotProduct(SpanCenter - WallStart, WallNormal));
		const float ParallelCross = FMath::Abs((WallDirection.X * SpanDirection.Y) - (WallDirection.Y * SpanDirection.X));
		const float SelectedHalfWidth = FMath::Max(Opening.width_cm, SpanLength) * 0.5f;
		const float LeftAlong = CenterAlong - SelectedHalfWidth;
		const float RightAlong = CenterAlong + SelectedHalfWidth;
		const float OutsideBefore = FMath::Max(0.0f, -LeftAlong);
		const float OutsideAfter = FMath::Max(0.0f, RightAlong - WallLength);
		const float OutsideDistance = FMath::Max(OutsideBefore, OutsideAfter);
		CheckedCount++;

		UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening host wall check. OpeningId=%s Kind=%s HostWallId=%s WallLength=%.2fcm SpanLength=%.2fcm SelectedWidth=%.2fcm CenterAlong=%.2fcm LateralDistance=%.2fcm ParallelCross=%.3f Range=[%.2f, %.2f]"),
			*Opening.id,
			*Opening.kind,
			*Opening.host_wall_id,
			WallLength,
			SpanLength,
			Opening.width_cm,
			CenterAlong,
			LateralDistance,
			ParallelCross,
			LeftAlong,
			RightAlong);

		if (LateralDistance >= OpeningHostWallLateralWarningCm)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening span is away from host wall centerline. OpeningId=%s HostWallId=%s LateralDistance=%.2fcm Severity=%s"), *Opening.id, *Opening.host_wall_id, LateralDistance, LateralDistance >= OpeningHostWallLateralStrongWarningCm ? TEXT("StrongWarning") : TEXT("Warning"));
			LateralWarningCount++;
		}

		if (ParallelCross >= OpeningHostWallParallelWarningCross)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening span is not parallel to host wall. OpeningId=%s HostWallId=%s ParallelCross=%.3f"), *Opening.id, *Opening.host_wall_id, ParallelCross);
			ParallelWarningCount++;
		}

		if (OutsideDistance >= OpeningHostWallOutsideWarningCm)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening selected width extends outside host wall. OpeningId=%s HostWallId=%s WallLength=%.2fcm Range=[%.2f, %.2f] Outside=%.2fcm"), *Opening.id, *Opening.host_wall_id, WallLength, LeftAlong, RightAlong, OutsideDistance);
			OutsideWarningCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening host wall diagnostics summary. Checked=%d MissingHost=%d MissingSpan=%d LateralWarnings=%d ParallelWarnings=%d OutsideWarnings=%d"), CheckedCount, MissingHostCount, MissingSpanCount, LateralWarningCount, ParallelWarningCount, OutsideWarningCount);
}


void ParseWallGroups(const TSharedPtr<FJsonObject>& RootObject, FHarnessFloorData& OutData)
{
	const TArray<TSharedPtr<FJsonValue>>* WallGroups = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("wall_groups"), WallGroups) || !WallGroups)
	{
		UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Wall groups summary. Total=0 Valid=0 Skipped=0 Present=false"));
		return;
	}

	int32 SkippedCount = 0;
	for (const TSharedPtr<FJsonValue>& GroupValue : *WallGroups)
	{
		const TSharedPtr<FJsonObject> GroupObject = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
		if (!GroupObject.IsValid())
		{
			++SkippedCount;
			continue;
		}

		FTopologyWallGroup Group;
		Group.id = ReadStringField(GroupObject, TEXT("id"));
		Group.name = ReadStringField(GroupObject, TEXT("name"));
		Group.kind = ReadStringField(GroupObject, TEXT("kind"));
		ReadStringArrayField(GroupObject, TEXT("wall_ids"), Group.wall_ids);

		if (Group.id.IsEmpty())
		{
			++SkippedCount;
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Wall group skipped because id is missing. WallIds=%d"), Group.wall_ids.Num());
			continue;
		}

		OutData.wall_groups.Add(Group);
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Wall groups summary. Total=%d Valid=%d Skipped=%d Present=true"), WallGroups->Num(), OutData.wall_groups.Num(), SkippedCount);
}

void ParseFinishGroups(const TSharedPtr<FJsonObject>& RootObject, FHarnessFloorData& OutData)
{
	const TArray<TSharedPtr<FJsonValue>>* FinishGroups = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("finish_groups"), FinishGroups) || !FinishGroups)
	{
		UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Finish groups summary. Total=0 Valid=0 Skipped=0 Present=false"));
		return;
	}

	int32 SkippedCount = 0;
	for (const TSharedPtr<FJsonValue>& GroupValue : *FinishGroups)
	{
		const TSharedPtr<FJsonObject> GroupObject = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
		if (!GroupObject.IsValid())
		{
			++SkippedCount;
			continue;
		}

		FTopologyFinishGroup Group;
		Group.id = ReadStringField(GroupObject, TEXT("id"));
		Group.name = ReadStringField(GroupObject, TEXT("name"));
		Group.kind = ReadStringField(GroupObject, TEXT("kind"));
		Group.material_id = ReadStringField(GroupObject, TEXT("material_id"));
		if (Group.material_id.IsEmpty())
		{
			Group.material_id = ReadStringField(GroupObject, TEXT("finish_id"));
		}
		ReadStringArrayField(GroupObject, TEXT("wall_ids"), Group.wall_ids);
		ReadStringArrayField(GroupObject, TEXT("space_ids"), Group.space_ids);

		if (Group.id.IsEmpty())
		{
			++SkippedCount;
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Finish group skipped because id is missing. WallIds=%d SpaceIds=%d"), Group.wall_ids.Num(), Group.space_ids.Num());
			continue;
		}

		OutData.finish_groups.Add(Group);
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Finish groups summary. Total=%d Valid=%d Skipped=%d Present=true"), FinishGroups->Num(), OutData.finish_groups.Num(), SkippedCount);
}

void ParseAssetRequirements(const TSharedPtr<FJsonObject>& RootObject, FHarnessFloorData& OutData)
{
	const TArray<TSharedPtr<FJsonValue>>* AssetRequirements = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("asset_requirements"), AssetRequirements) || !AssetRequirements)
	{
		UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Asset requirements summary. Total=0 Valid=0 Skipped=0 Present=false"));
		return;
	}

	int32 SkippedCount = 0;
	for (const TSharedPtr<FJsonValue>& RequirementValue : *AssetRequirements)
	{
		const TSharedPtr<FJsonObject> RequirementObject = RequirementValue.IsValid() ? RequirementValue->AsObject() : nullptr;
		if (!RequirementObject.IsValid())
		{
			++SkippedCount;
			continue;
		}

		FTopologyAssetRequirement Requirement;
		Requirement.id = ReadStringField(RequirementObject, TEXT("id"));
		Requirement.kind = ReadStringField(RequirementObject, TEXT("kind"));
		Requirement.target_id = ReadStringField(RequirementObject, TEXT("target_id"));
		Requirement.target_type = ReadStringField(RequirementObject, TEXT("target_type"));
		Requirement.status = ReadStringField(RequirementObject, TEXT("status"));
		Requirement.selected_asset_id = ReadStringField(RequirementObject, TEXT("selected_asset_id"));
		if (Requirement.selected_asset_id.IsEmpty())
		{
			Requirement.selected_asset_id = ReadStringField(RequirementObject, TEXT("asset_id"));
		}

		if (Requirement.id.IsEmpty())
		{
			++SkippedCount;
			UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Asset requirement skipped because id is missing. Kind=%s TargetType=%s TargetId=%s"), *Requirement.kind, *Requirement.target_type, *Requirement.target_id);
			continue;
		}

		OutData.asset_requirements.Add(Requirement);
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Asset requirements summary. Total=%d Valid=%d Skipped=%d Present=true"), AssetRequirements->Num(), OutData.asset_requirements.Num(), SkippedCount);
}

// ---------------------------------------------------------------------------
//  Level lookup helpers
// ---------------------------------------------------------------------------

float ResolveLevelElevationCm(const TMap<FString, float>& LevelElevationById, const FString& LevelId)
{
	if (const float* Elevation = LevelElevationById.Find(LevelId))
	{
		return *Elevation;
	}
	return 0.0f;
}

float ResolveLevelHeightCm(const TMap<FString, float>& LevelHeightById, const FString& LevelId)
{
	if (const float* Height = LevelHeightById.Find(LevelId))
	{
		return *Height;
	}
	return 240.0f;
}

// ---------------------------------------------------------------------------
//  Main conversion: topology JSON → FHarnessFloorData
// ---------------------------------------------------------------------------

bool ConvertTopologyToFloorData(const TSharedPtr<FJsonObject>& RootObject, FHarnessFloorData& OutData, FString& OutError)
{
	OutData = FHarnessFloorData();
	OutData.schema_version = ReadStringField(RootObject, TEXT("schema_version"));

	// Plan info (optional top-level fields)
	const TSharedPtr<FJsonObject>* PlanObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("plan"), PlanObject) && PlanObject && PlanObject->IsValid())
	{
		double PlanId = 0.0;
		if ((*PlanObject)->TryGetNumberField(TEXT("id"), PlanId))
		{
			OutData.plan.id = static_cast<int32>(PlanId);
		}
		double PlanVersion = 1.0;
		if ((*PlanObject)->TryGetNumberField(TEXT("version"), PlanVersion))
		{
			OutData.plan.version = static_cast<int32>(PlanVersion);
		}
		OutData.plan.name = ReadStringField(*PlanObject, TEXT("name"));
	}

	// -----------------------------------------------------------------------
	//  Levels
	// -----------------------------------------------------------------------
	TMap<FString, float> LevelElevationById;
	TMap<FString, float> LevelHeightById;
	float CommonWallHeightCm = 0.0f;
	const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
	if (RootObject->TryGetArrayField(TEXT("levels"), Levels) && Levels)
	{
		for (const TSharedPtr<FJsonValue>& LevelValue : *Levels)
		{
			const TSharedPtr<FJsonObject> LevelObject = LevelValue.IsValid() ? LevelValue->AsObject() : nullptr;
			if (!LevelObject.IsValid())
			{
				continue;
			}

			const FString LevelId = ReadStringField(LevelObject, TEXT("id"));
			if (LevelId.IsEmpty())
			{
				continue;
			}

			FTopologyLevel Level;
			Level.id = LevelId;
			Level.name = ReadStringField(LevelObject, TEXT("name"));
			Level.elevation_cm = ReadNumberMmAsCm(LevelObject, TEXT("elevation"), 0.0f);
			Level.default_height_cm = ReadNumberMmAsCm(LevelObject, TEXT("default_height"), 240.0f);
			OutData.levels.Add(Level);

			LevelElevationById.Add(LevelId, Level.elevation_cm);
			LevelHeightById.Add(LevelId, Level.default_height_cm);
			CommonWallHeightCm = CommonWallHeightCm > UE_SMALL_NUMBER
				? FMath::Max(CommonWallHeightCm, Level.default_height_cm)
				: Level.default_height_cm;
		}
	}

	// -----------------------------------------------------------------------
	//  Spatial hash for vertex merging
	// -----------------------------------------------------------------------
	FSpatialVertexHash VertexHash;
	VertexHash.Init(OutData.vertices);

	// -----------------------------------------------------------------------
	//  Walls → Vertices + Half-edges
	// -----------------------------------------------------------------------
	int32 WallDeclaredLengthCount = 0;
	int32 WallGeometryFallbackCount = 0;
	int32 WallCenterlineFallbackCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
	RootObject->TryGetArrayField(TEXT("walls"), Walls);
	for (const TSharedPtr<FJsonValue>& WallValue : *Walls)
	{
		const TSharedPtr<FJsonObject> WallObject = WallValue.IsValid() ? WallValue->AsObject() : nullptr;
		if (!WallObject.IsValid())
		{
			continue;
		}

		const FString WallId = ReadStringField(WallObject, TEXT("id"));
		if (WallId.IsEmpty())
		{
			OutError = TEXT("A wall is missing required id.");
			return false;
		}

		FVector2D StartCm;
		FVector2D EndCm;
		FVector2D Direction;
		float DeclaredLengthCm = 0.0f;
		const bool bHasTopLevelStart = TryReadPointFieldMm(WallObject, TEXT("start"), StartCm);
		const bool bHasTopLevelEnd = TryReadPointFieldMm(WallObject, TEXT("end"), EndCm);
		const bool bHasTopLevelDirection = TryReadDirectionField(WallObject, TEXT("direction"), Direction);
		bool bHasDeclaredLength = TryReadNumberMmAsCm(WallObject, TEXT("length_mm"), DeclaredLengthCm);
		if (!bHasDeclaredLength)
		{
			bHasDeclaredLength = TryReadNumberMmAsCm(WallObject, TEXT("manual_length_mm"), DeclaredLengthCm);
		}
		if (!bHasDeclaredLength)
		{
			bHasDeclaredLength = TryReadNestedNumberMmAsCm(WallObject, TEXT("measurement"), TEXT("length_mm"), DeclaredLengthCm);
		}

		if (bHasTopLevelStart && bHasTopLevelDirection && bHasDeclaredLength && DeclaredLengthCm > UE_SMALL_NUMBER)
		{
			EndCm = StartCm + (Direction * DeclaredLengthCm);
			++WallDeclaredLengthCount;
		}
		else if (bHasTopLevelStart && bHasTopLevelEnd)
		{
			++WallGeometryFallbackCount;
		}
		else
		{
			const TArray<TSharedPtr<FJsonValue>>* Centerline = nullptr;
			if (!WallObject->TryGetArrayField(TEXT("centerline"), Centerline) || !Centerline || Centerline->Num() < 2)
			{
				OutError = FString::Printf(TEXT("Wall '%s' must contain either start+direction+length_mm, top-level start/end, or a centerline with at least two points."), *WallId);
				return false;
			}

			if (!ReadPointMm((*Centerline)[0]->AsObject(), StartCm) || !ReadPointMm((*Centerline)[1]->AsObject(), EndCm))
			{
				OutError = FString::Printf(TEXT("Wall '%s' contains invalid centerline point data."), *WallId);
				return false;
			}
			++WallCenterlineFallbackCount;
		}

		// Merge or create vertices via spatial hash
		const FString StartVertexId = VertexHash.FindOrAdd(StartCm.X, StartCm.Y, MakeWallStartVertexId(WallId));
		const FString EndVertexId = VertexHash.FindOrAdd(EndCm.X, EndCm.Y, MakeWallEndVertexId(WallId));

		// Build half-edge pair
		const FString EdgeId = MakeWallEdgeId(WallId);
		const FString TwinId = MakeWallTwinEdgeId(WallId);
		const FString Kind = ReadStringField(WallObject, TEXT("kind"), TEXT("inner"));
		const FString LevelId = ReadStringField(WallObject, TEXT("level_id"));
		const float ThicknessCm = HarnessDefaultWallThicknessCm;
		const float HeightCm = ReadNumberMmAsCm(WallObject, TEXT("height"), ResolveLevelHeightCm(LevelHeightById, LevelId));
		CommonWallHeightCm = CommonWallHeightCm > UE_SMALL_NUMBER
			? FMath::Max(CommonWallHeightCm, HeightCm)
			: HeightCm;

		FTopologyHalfEdge Edge;
		Edge.id = EdgeId;
		Edge.wall_id = WallId;
		Edge.vertex_start = StartVertexId;
		Edge.vertex_end = EndVertexId;
		Edge.twin_id = TwinId;
		Edge.wall_thickness = ThicknessCm;
		Edge.wall_height = HeightCm;
		Edge.type = NormalizeWallKindToType(Kind);
		OutData.half_edges.Add(Edge);

		FTopologyHalfEdge TwinEdge;
		TwinEdge.id = TwinId;
		TwinEdge.wall_id = WallId;
		TwinEdge.vertex_start = EndVertexId;
		TwinEdge.vertex_end = StartVertexId;
		TwinEdge.twin_id = EdgeId;
		TwinEdge.wall_thickness = ThicknessCm;
		TwinEdge.wall_height = HeightCm;
		TwinEdge.type = NormalizeWallKindToType(Kind);
		OutData.half_edges.Add(TwinEdge);
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Wall length source summary. Total=%d DeclaredLength=%d TopLevelStartEndFallback=%d CenterlineFallback=%d"), WallDeclaredLengthCount + WallGeometryFallbackCount + WallCenterlineFallbackCount, WallDeclaredLengthCount, WallGeometryFallbackCount, WallCenterlineFallbackCount);

	OutData.common_wall_height_cm = CommonWallHeightCm > UE_SMALL_NUMBER ? CommonWallHeightCm : 240.0f;

	// -----------------------------------------------------------------------
	//  Spaces → Vertices + Faces
	// -----------------------------------------------------------------------
	TSet<FString> SpaceIds;
	int32 SpaceBoundaryPointCount = 0;
	int32 SpaceBoundaryMergedPointCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* Spaces = nullptr;
	RootObject->TryGetArrayField(TEXT("spaces"), Spaces);
	for (const TSharedPtr<FJsonValue>& SpaceValue : *Spaces)
	{
		const TSharedPtr<FJsonObject> SpaceObject = SpaceValue.IsValid() ? SpaceValue->AsObject() : nullptr;
		if (!SpaceObject.IsValid())
		{
			continue;
		}

		const FString SpaceId = ReadStringField(SpaceObject, TEXT("id"));
		if (SpaceId.IsEmpty())
		{
			OutError = TEXT("A space is missing required id.");
			return false;
		}
		SpaceIds.Add(SpaceId);

		const TArray<TSharedPtr<FJsonValue>>* Boundary = nullptr;
		if (!SpaceObject->TryGetArrayField(TEXT("boundary"), Boundary) || !Boundary || Boundary->Num() < 3)
		{
			OutError = FString::Printf(TEXT("Space '%s' must contain at least three boundary points."), *SpaceId);
			return false;
		}

		const FString LevelId = ReadStringField(SpaceObject, TEXT("level_id"));
		const FString SpaceKind = ReadStringField(SpaceObject, TEXT("kind"));

		FTopologyFace Face;
		Face.face_id = SpaceId;
		Face.kind = SpaceKind;
		Face.label = ReadStringField(SpaceObject, TEXT("name"), SpaceKind.IsEmpty() ? SpaceId : SpaceKind);
		Face.height_cm = OutData.common_wall_height_cm;
		Face.z_offset = ResolveLevelElevationCm(LevelElevationById, LevelId);
		Face.floor_material = ReadStringField(SpaceObject, TEXT("floor_material"));

		// Boundary vertices
		// v3.2 spaces often share exact/near-exact coordinates with adjacent spaces and wall centerlines.
		// Reuse the same spatial hash used by walls so floor polygons can stitch on shared IDs
		// instead of creating a separate duplicate vertex for every space boundary point.
		for (int32 PointIndex = 0; PointIndex < Boundary->Num(); ++PointIndex)
		{
			FVector2D PointCm;
			if (!ReadPointMm((*Boundary)[PointIndex]->AsObject(), PointCm))
			{
				OutError = FString::Printf(TEXT("Space '%s' contains invalid boundary point data."), *SpaceId);
				return false;
			}

			const FString BoundaryVertexId = FString::Printf(TEXT("%s_boundary_%d"), *SpaceId, PointIndex);
			bool bAddedBoundaryVertex = false;
			const FString ResolvedBoundaryVertexId = VertexHash.FindOrAdd(PointCm.X, PointCm.Y, BoundaryVertexId, &bAddedBoundaryVertex);
			++SpaceBoundaryPointCount;
			if (!bAddedBoundaryVertex)
			{
				++SpaceBoundaryMergedPointCount;
			}

			Face.contour_vertex_ids.Add(ResolvedBoundaryVertexId);
		}

		// Boundary wall references
		const TArray<TSharedPtr<FJsonValue>>* BoundaryWalls = nullptr;
		if (SpaceObject->TryGetArrayField(TEXT("boundary_walls"), BoundaryWalls) && BoundaryWalls)
		{
			for (const TSharedPtr<FJsonValue>& WallRef : *BoundaryWalls)
			{
				FString WallRefId;
				if (WallRef.IsValid() && WallRef->TryGetString(WallRefId) && !WallRefId.IsEmpty())
				{
					Face.boundary_wall_ids.Add(WallRefId);
				}
			}
		}

		OutData.faces.Add(Face);
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Space boundary vertex stitching. BoundaryPoints=%d ReusedOrMerged=%d UniqueVerticesAfterSpaces=%d ToleranceCm=%.2f"), SpaceBoundaryPointCount, SpaceBoundaryMergedPointCount, OutData.vertices.Num(), VertexMergeTolerance);

	// -----------------------------------------------------------------------
	//  Openings
	// -----------------------------------------------------------------------
	int32 OpeningKindDoorCount = 0;
	int32 OpeningKindWindowCount = 0;
	int32 OpeningKindPassageCount = 0;
	int32 OpeningKindUnknownCount = 0;
	int32 OpeningConnectsTotalCount = 0;
	int32 OpeningInteriorCount = 0;
	int32 OpeningExteriorCount = 0;
	int32 OpeningBoundaryCount = 0;
	int32 OpeningUnknownConnectionCount = 0;
	int32 OpeningUnspecifiedConnectionCount = 0;
	int32 OpeningUnknownConnectTokenCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* Openings = nullptr;
	if (RootObject->TryGetArrayField(TEXT("openings"), Openings) && Openings)
	{
		for (const TSharedPtr<FJsonValue>& OpeningValue : *Openings)
		{
			const TSharedPtr<FJsonObject> OpeningObject = OpeningValue.IsValid() ? OpeningValue->AsObject() : nullptr;
			if (!OpeningObject.IsValid())
			{
				continue;
			}

			const FString OpeningId = ReadStringField(OpeningObject, TEXT("id"));
			if (OpeningId.IsEmpty())
			{
				OutError = TEXT("An opening is missing required id.");
				return false;
			}

			const FString OpeningKind = ReadStringField(OpeningObject, TEXT("kind"), TEXT("door"));
			const FString OpeningType = NormalizeOpeningKindToType(OpeningKind);
			FString HostWallId = ReadStringField(OpeningObject, TEXT("host_wall_id"));
			if (HostWallId.IsEmpty())
			{
				HostWallId = ReadOpeningAnchorWallId(OpeningObject);
			}

			FTopologyOpening Opening;
			Opening.id = OpeningId;
			Opening.type = OpeningType;
			Opening.kind = OpeningKind;
			Opening.host_wall_id = HostWallId;
			Opening.target_edge_id = HostWallId.IsEmpty() ? FString() : MakeWallEdgeId(HostWallId);
			Opening.offset_from = ReadStringField(OpeningObject, TEXT("offset_from"), TEXT("start"));
			Opening.offset_to_center_cm = ReadNumberMmAsCm(OpeningObject, TEXT("offset_to_center"), 0.0f);
			Opening.height_cm = ReadNumberMmAsCm(OpeningObject, TEXT("height"), GetDefaultOpeningHeightCm(OpeningType));
			Opening.z_offset_cm = ReadNumberMmAsCm(OpeningObject, TEXT("bottom"), GetDefaultOpeningBottomCm(OpeningType));

			const TSharedPtr<FJsonObject>* SpanObject = nullptr;
			if (OpeningObject->TryGetObjectField(TEXT("span"), SpanObject) && SpanObject && SpanObject->IsValid())
			{
				const TSharedPtr<FJsonObject>* SpanStartObject = nullptr;
				const TSharedPtr<FJsonObject>* SpanEndObject = nullptr;
				FVector2D SpanStartCm;
				FVector2D SpanEndCm;
				if ((*SpanObject)->TryGetObjectField(TEXT("start"), SpanStartObject) &&
					(*SpanObject)->TryGetObjectField(TEXT("end"), SpanEndObject) &&
					SpanStartObject &&
					SpanEndObject &&
					ReadPointMm(*SpanStartObject, SpanStartCm) &&
					ReadPointMm(*SpanEndObject, SpanEndCm))
				{
					Opening.bHasSpan = true;
					Opening.span_start_cm = SpanStartCm;
					Opening.span_end_cm = SpanEndCm;
					Opening.drawn_width_cm = ReadNumberMmAsCm(*SpanObject, TEXT("drawn_width_mm"), FVector2D::Distance(SpanStartCm, SpanEndCm));
				}
			}

			const TSharedPtr<FJsonObject>* MeasurementObject = nullptr;
			if (OpeningObject->TryGetObjectField(TEXT("measurement"), MeasurementObject) && MeasurementObject && MeasurementObject->IsValid())
			{
				TryReadNumberMmAsCm(*MeasurementObject, TEXT("opening_width_mm"), Opening.measured_width_cm);
			}

			float LegacyWidthCm = 0.0f;
			const bool bHasLegacyWidth = TryReadNumberMmAsCm(OpeningObject, TEXT("width"), LegacyWidthCm);
			Opening.width_cm = Opening.measured_width_cm > UE_SMALL_NUMBER
				? Opening.measured_width_cm
				: (bHasLegacyWidth
					? LegacyWidthCm
					: (Opening.drawn_width_cm > UE_SMALL_NUMBER ? Opening.drawn_width_cm : GetDefaultOpeningWidthCm(OpeningType)));

			if (OpeningType.Equals(TEXT("Door"), ESearchCase::IgnoreCase))
			{
				OpeningKindDoorCount++;
			}
			else if (OpeningType.Equals(TEXT("Window"), ESearchCase::IgnoreCase))
			{
				OpeningKindWindowCount++;
			}
			else if (OpeningType.Equals(TEXT("Opening"), ESearchCase::IgnoreCase))
			{
				OpeningKindPassageCount++;
			}
			else
			{
				OpeningKindUnknownCount++;
				UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Unknown opening kind. OpeningId=%s Kind=%s HostWallId=%s DefaultType=Unknown DefaultWidth=%.2fcm DefaultHeight=%.2fcm DefaultBottom=%.2fcm"), *Opening.id, *Opening.kind, *Opening.host_wall_id, GetDefaultOpeningWidthCm(OpeningType), GetDefaultOpeningHeightCm(OpeningType), GetDefaultOpeningBottomCm(OpeningType));
			}

			LogOpeningWidthDiagnostics(Opening, bHasLegacyWidth, LegacyWidthCm);

			// Connects array (space IDs this opening connects)
			const TArray<TSharedPtr<FJsonValue>>* ConnectsArray = nullptr;
			if (OpeningObject->TryGetArrayField(TEXT("connects"), ConnectsArray) && ConnectsArray)
			{
				for (const TSharedPtr<FJsonValue>& ConnectValue : *ConnectsArray)
				{
					FString ConnectId;
					if (ConnectValue.IsValid() && ConnectValue->TryGetString(ConnectId) && !ConnectId.IsEmpty())
					{
						Opening.connects.Add(ConnectId);
					}
				}
			}

			ClassifyOpeningConnects(Opening, SpaceIds);
			OpeningConnectsTotalCount++;
			if (Opening.connection_type.Equals(TEXT("Interior"), ESearchCase::IgnoreCase))
			{
				OpeningInteriorCount++;
			}
			else if (Opening.connection_type.Equals(TEXT("Exterior"), ESearchCase::IgnoreCase))
			{
				OpeningExteriorCount++;
			}
			else if (Opening.connection_type.Equals(TEXT("Boundary"), ESearchCase::IgnoreCase))
			{
				OpeningBoundaryCount++;
			}
			else if (Opening.connection_type.Equals(TEXT("Unknown"), ESearchCase::IgnoreCase))
			{
				OpeningUnknownConnectionCount++;
			}
			else
			{
				OpeningUnspecifiedConnectionCount++;
			}
			OpeningUnknownConnectTokenCount += Opening.unknown_connect_count;

			UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening connects classification. OpeningId=%s Kind=%s Type=%s HostWallId=%s ConnectionType=%s Connects=[%s] KnownSpaces=%d ExternalTokens=%d UnknownTokens=%d"),
				*Opening.id,
				*Opening.kind,
				*Opening.type,
				*Opening.host_wall_id,
				*Opening.connection_type,
				*JoinStringArrayForLog(Opening.connects),
				Opening.connected_space_count,
				Opening.external_connect_count,
				Opening.unknown_connect_count);

			if (Opening.unknown_connect_count > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[HarnessJsonParser] Opening has unknown connects. OpeningId=%s Connects=[%s] KnownSpaces=%d ExternalTokens=%d UnknownTokens=%d"),
					*Opening.id,
					*JoinStringArrayForLog(Opening.connects),
					Opening.connected_space_count,
					Opening.external_connect_count,
					Opening.unknown_connect_count);
			}

			// Swing sub-object
			const TSharedPtr<FJsonObject>* SwingObject = nullptr;
			if (OpeningObject->TryGetObjectField(TEXT("swing"), SwingObject) && SwingObject && SwingObject->IsValid())
			{
				Opening.swing.direction = ReadFirstStringField(*SwingObject, { TEXT("direction"), TEXT("facing"), TEXT("front_back"), TEXT("frontBack"), TEXT("side") }, TEXT("none"));
				Opening.swing.hinge = ReadFirstStringField(*SwingObject, { TEXT("hinge"), TEXT("hinge_side"), TEXT("hingeSide") }, TEXT("none"));
				double SwingAngle = 90.0;
				if ((*SwingObject)->TryGetNumberField(TEXT("angle"), SwingAngle))
				{
					Opening.swing.angle = static_cast<float>(SwingAngle);
				}
			}
			else
			{
				Opening.swing.direction = ReadFirstStringField(OpeningObject, { TEXT("swing_direction"), TEXT("swingDirection"), TEXT("facing"), TEXT("front_back"), TEXT("frontBack"), TEXT("side") }, TEXT("none"));
				Opening.swing.hinge = ReadFirstStringField(OpeningObject, { TEXT("hinge"), TEXT("hinge_side"), TEXT("hingeSide") }, TEXT("none"));
			}

			OutData.openings.Add(Opening);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening kind summary. Total=%d Doors=%d Windows=%d Passages=%d Unknown=%d"),
		OpeningKindDoorCount + OpeningKindWindowCount + OpeningKindPassageCount + OpeningKindUnknownCount,
		OpeningKindDoorCount,
		OpeningKindWindowCount,
		OpeningKindPassageCount,
		OpeningKindUnknownCount);

	UE_LOG(LogTemp, Log, TEXT("[HarnessJsonParser] Opening connects summary. Total=%d Interior=%d Exterior=%d Boundary=%d Unknown=%d Unspecified=%d UnknownTokens=%d KnownSpaceIds=%d"),
		OpeningConnectsTotalCount,
		OpeningInteriorCount,
		OpeningExteriorCount,
		OpeningBoundaryCount,
		OpeningUnknownConnectionCount,
		OpeningUnspecifiedConnectionCount,
		OpeningUnknownConnectTokenCount,
		SpaceIds.Num());

	ParseWallGroups(RootObject, OutData);
	ParseFinishGroups(RootObject, OutData);
	ParseAssetRequirements(RootObject, OutData);

	LogOpeningHostWallDiagnostics(OutData);

	return true;
}
}

bool FHarnessJsonParser::LoadFloorDataFromJsonFile(const FString& FilePath, FHarnessFloorData& OutData, FString& OutError)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to load file from path: %s"), *FilePath);
		return false;
	}

	return ParseFloorDataFromJsonString(JsonString, OutData, OutError);
}

bool FHarnessJsonParser::ParseFloorDataFromJsonString(const FString& JsonString, FHarnessFloorData& OutData, FString& OutError)
{
	OutData = FHarnessFloorData();

	TSharedPtr<FJsonObject> RootObject;
	if (!DeserializeJsonObject(JsonString, RootObject))
	{
		OutError = TEXT("JSON deserialization failed. Invalid JSON format.");
		return false;
	}

	if (!ValidateTopologyRoot(RootObject, OutError))
	{
		return false;
	}

	if (!ConvertTopologyToFloorData(RootObject, OutData, OutError))
	{
		return false;
	}

	if (OutData.vertices.IsEmpty() || OutData.half_edges.IsEmpty() || OutData.faces.IsEmpty())
	{
		OutError = TEXT("Invalid topology payload. Converted floor data is missing vertices, walls, or spaces.");
		return false;
	}

	return true;
}
