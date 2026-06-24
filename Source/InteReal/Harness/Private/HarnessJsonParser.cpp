#include "InteReal/Harness/Public/HarnessJsonParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr float MmToCm = 0.1f;

// Spatial merge tolerance: 0.3 cm = 3 mm
constexpr float VertexMergeTolerance = 0.3f;

// Grid cell size for spatial hash (slightly larger than tolerance)
constexpr float SpatialCellSize = 0.5f;

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

FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& DefaultValue = FString())
{
	if (!Object.IsValid())
	{
		return DefaultValue;
	}

	FString Value;
	return Object->TryGetStringField(Field, Value) ? Value : DefaultValue;
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
	FString FindOrAdd(float X, float Y, const FString& NewId)
	{
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

bool ValidateV31Root(const TSharedPtr<FJsonObject>& RootObject, FString& OutError)
{
	FString Unit;
	if (!RootObject->TryGetStringField(TEXT("unit"), Unit) || !Unit.Equals(TEXT("mm"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Unsupported topology unit. Harness v3.1 requires unit='mm'.");
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
		OutError = TEXT("Unsupported coordinate_system. Harness v3.1 requires top_left, x_axis=right, y_axis=down, rotation_degrees=0.");
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
	return Kind.Equals(TEXT("window"), ESearchCase::IgnoreCase) ? TEXT("Window") : TEXT("Door");
}

FString NormalizeWallKindToType(const FString& Kind)
{
	return Kind.Equals(TEXT("outer"), ESearchCase::IgnoreCase) ? TEXT("WallOuter") : TEXT("WallInner");
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
//  Main conversion: v3.1 JSON → FHarnessFloorData
// ---------------------------------------------------------------------------

bool ConvertV31TopologyToFloorData(const TSharedPtr<FJsonObject>& RootObject, FHarnessFloorData& OutData, FString& OutError)
{
	OutData = FHarnessFloorData();
	OutData.schema_version = ReadStringField(RootObject, TEXT("schema_version"), TEXT("3.1"));

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

		const TArray<TSharedPtr<FJsonValue>>* Centerline = nullptr;
		if (!WallObject->TryGetArrayField(TEXT("centerline"), Centerline) || !Centerline || Centerline->Num() < 2)
		{
			OutError = FString::Printf(TEXT("Wall '%s' must contain a centerline with at least two points."), *WallId);
			return false;
		}

		FVector2D StartCm;
		FVector2D EndCm;
		if (!ReadPointMm((*Centerline)[0]->AsObject(), StartCm) ||
			!ReadPointMm((*Centerline)[1]->AsObject(), EndCm))
		{
			OutError = FString::Printf(TEXT("Wall '%s' contains invalid centerline point data."), *WallId);
			return false;
		}

		// Merge or create vertices via spatial hash
		const FString StartVertexId = VertexHash.FindOrAdd(StartCm.X, StartCm.Y, MakeWallStartVertexId(WallId));
		const FString EndVertexId = VertexHash.FindOrAdd(EndCm.X, EndCm.Y, MakeWallEndVertexId(WallId));

		// Build half-edge pair
		const FString EdgeId = MakeWallEdgeId(WallId);
		const FString TwinId = MakeWallTwinEdgeId(WallId);
		const FString Kind = ReadStringField(WallObject, TEXT("kind"), TEXT("inner"));
		const FString LevelId = ReadStringField(WallObject, TEXT("level_id"));
		const float ThicknessCm = ReadNumberMmAsCm(WallObject, TEXT("thickness"), 20.0f);
		const float HeightCm = ReadNumberMmAsCm(WallObject, TEXT("height"), ResolveLevelHeightCm(LevelHeightById, LevelId));

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

	// -----------------------------------------------------------------------
	//  Spaces → Vertices + Faces
	// -----------------------------------------------------------------------
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
		Face.height_cm = ResolveLevelHeightCm(LevelHeightById, LevelId);
		Face.z_offset = ResolveLevelElevationCm(LevelElevationById, LevelId);
		Face.floor_material = ReadStringField(SpaceObject, TEXT("floor_material"));

		// Boundary vertices
		for (int32 PointIndex = 0; PointIndex < Boundary->Num(); ++PointIndex)
		{
			FVector2D PointCm;
			if (!ReadPointMm((*Boundary)[PointIndex]->AsObject(), PointCm))
			{
				OutError = FString::Printf(TEXT("Space '%s' contains invalid boundary point data."), *SpaceId);
				return false;
			}

			const FString BoundaryVertexId = FString::Printf(TEXT("%s_boundary_%d"), *SpaceId, PointIndex);

			FTopologyVertex Vertex;
			Vertex.id = BoundaryVertexId;
			Vertex.x = PointCm.X;
			Vertex.y = PointCm.Y;
			OutData.vertices.Add(Vertex);

			Face.contour_vertex_ids.Add(BoundaryVertexId);
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

	// -----------------------------------------------------------------------
	//  Openings
	// -----------------------------------------------------------------------
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
			const FString HostWallId = ReadStringField(OpeningObject, TEXT("host_wall_id"));
			if (OpeningId.IsEmpty() || HostWallId.IsEmpty())
			{
				OutError = TEXT("An opening is missing required id or host_wall_id.");
				return false;
			}

			const FString OpeningKind = ReadStringField(OpeningObject, TEXT("kind"), TEXT("door"));
			const FString OpeningType = NormalizeOpeningKindToType(OpeningKind);

			FTopologyOpening Opening;
			Opening.id = OpeningId;
			Opening.type = OpeningType;
			Opening.kind = OpeningKind;
			Opening.target_edge_id = MakeWallEdgeId(HostWallId);
			Opening.host_wall_id = HostWallId;
			Opening.offset_from = ReadStringField(OpeningObject, TEXT("offset_from"), TEXT("start"));
			Opening.offset_to_center_cm = ReadNumberMmAsCm(OpeningObject, TEXT("offset_to_center"), 0.0f);
			Opening.width_cm = ReadNumberMmAsCm(OpeningObject, TEXT("width"), 90.0f);
			Opening.height_cm = ReadNumberMmAsCm(OpeningObject, TEXT("height"), OpeningType == TEXT("Window") ? 120.0f : 210.0f);
			Opening.z_offset_cm = ReadNumberMmAsCm(OpeningObject, TEXT("bottom"), OpeningType == TEXT("Window") ? 90.0f : 0.0f);

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

			// Swing sub-object
			const TSharedPtr<FJsonObject>* SwingObject = nullptr;
			if (OpeningObject->TryGetObjectField(TEXT("swing"), SwingObject) && SwingObject && SwingObject->IsValid())
			{
				Opening.swing.direction = ReadStringField(*SwingObject, TEXT("direction"), TEXT("none"));
				Opening.swing.hinge = ReadStringField(*SwingObject, TEXT("hinge"), TEXT("none"));
				double SwingAngle = 90.0;
				if ((*SwingObject)->TryGetNumberField(TEXT("angle"), SwingAngle))
				{
					Opening.swing.angle = static_cast<float>(SwingAngle);
				}
			}

			OutData.openings.Add(Opening);
		}
	}

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

	if (!ValidateV31Root(RootObject, OutError))
	{
		return false;
	}

	if (!ConvertV31TopologyToFloorData(RootObject, OutData, OutError))
	{
		return false;
	}

	if (OutData.vertices.IsEmpty() || OutData.half_edges.IsEmpty() || OutData.faces.IsEmpty())
	{
		OutError = TEXT("Invalid v3.1 topology. Converted floor data is missing vertices, walls, or spaces.");
		return false;
	}

	return true;
}
