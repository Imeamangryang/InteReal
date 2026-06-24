# User-Driven Load/Save Pipeline 구현 내역

본 문서는 Unreal Engine 5.7 환경에서 유저 주도형 인테리어 도면 로드/세이브 시스템을 구축하기 위해 생성 및 수정된 C++ 아키텍처 내역을 요약합니다. SOLID 원칙과 모듈화(SRP)를 기반으로 작성되었습니다.

## 1. 아키텍처 개요
기존의 절차적 건물 생성기(`UHarnessGeneratorComponent`)를 기반으로, 외부 서버(HTTP)와 통신하여 도면 데이터를 받아오고 유저가 변경한 인테리어(Furniture Delta)를 분리하여 저장 및 로드하는 파이프라인을 구축했습니다.

## 2. 생성 및 수정된 파일

### 2.1. 프로젝트 설정 (Modified)
*   **`Source/InteReal/InteReal.Build.cs`**
    *   **변경 사항**: `Http`, `Json`, `JsonUtilities` 모듈을 `PublicDependencyModuleNames`에 추가하여 네트워킹 및 JSON 직렬화/역직렬화 기능을 활성화했습니다.

### 2.2. 공통 데이터 타입 (Created)
*   **`Source/InteReal/Harness/Public/HarnessDataTypes.h`**
    *   **역할**: 컴포넌트 간 데이터를 주고받기 위한 Blueprint 호환 구조체 정의.
    *   **주요 구조체**:
        *   `FFloorPlanInfo`: 서버에서 받아올 도면의 기본 정보(ID, 이름).
        *   `FFurnitureDelta`: 가구 배치의 변경점(ID, Transform).
        *   `FFurnitureDeltaList`: `FFurnitureDelta`의 배열을 감싸는 컨테이너 구조체.

### 2.3. 네트워크 컴포넌트 (Created)
*   **`Source/InteReal/Harness/Public/HarnessNetworkComponent.h`**
*   **`Source/InteReal/Harness/Private/HarnessNetworkComponent.cpp`**
    *   **역할**: REST API 통신(GET/POST) 전담 모듈.
    *   **주요 기능**:
        *   `RequestFloorPlanList()`: 도면 목록 동기화.
        *   `DownloadFloorPlanBase(PlanId)`: 건물의 원본 구조 JSON 다운로드.
        *   `DownloadFloorPlanDelta(PlanId)`: 유저가 커스텀한 가구 배치 JSON 다운로드.
        *   `UploadFloorPlanDelta(PlanId, JsonString)`: 현재 씬의 가구 상태를 서버로 업로드.
    *   **특징**: 비동기 콜백을 위한 `DYNAMIC_MULTICAST_DELEGATE` 정의 및 바인딩 완료.

### 2.4. 세이브 매니저 컴포넌트 (Created)
*   **`Source/InteReal/Harness/Public/HarnessSaveManagerComponent.h`**
*   **`Source/InteReal/Harness/Private/HarnessSaveManagerComponent.cpp`**
    *   **역할**: 무거운 3D 씬이 아닌 가벼운 텍스트(JSON) 기반 델타(Delta) 저장 및 복원.
    *   **주요 기능**:
        *   `SaveInteriorState()`: `InteriorFurniture` 태그가 달린 액터를 순회하여 ID와 Transform만 추출 후 JSON 직렬화.
        *   `LoadInteriorState(JsonString)`: 델타 JSON을 파싱하여 기존 가구를 파괴(Clear)한 뒤 새 Transform에 맞게 액터 스폰.
    *   **특징**: UDataTable과 FName 식별자를 사용하여 에셋 하드코딩 방지.

### 2.5. 파이프라인 통합 매니저 (Created/Modified)
*   **`Source/InteReal/Harness/Public/HarnessPipelineManager.h`**
*   **`Source/InteReal/Harness/Private/HarnessPipelineManager.cpp`**
    *   **역할**: 네트워크 통신 결과와 제너레이터, 세이브 매니저를 연결하는 컨트롤 타워.
    *   **주요 기능**:
        *   도면 로드 프로세스 일원화: `기존 씬 정리` -> `원본 JSON 다운로드 및 생성` -> `델타 JSON 다운로드 및 인테리어 스폰`.
    *   **특징**: `HarnessJsonParser` 및 각 컴포넌트의 Delegate를 활용하여 순차적 비동기 처리를 안전하게 수행하도록 구성.

### 2.6. UI 및 UMG 통합 (Created)
*   **`Source/InteReal/Harness/Public/HarnessMainHUD.h`**
*   **`Source/InteReal/Harness/Private/HarnessMainHUD.cpp`**
    *   **역할**: 화면 좌상단 프로젝트 목록 드롭다운 UI 구현 베이스 클래스.
    *   **주요 기능**:
        *   `OnLoadProjectListClicked()`: 네트워크 모듈을 호출하여 목록 가져오기.
        *   `OnPlanListReceived()`: 델리게이트 응답 시 `UScrollBox` 내부에 동적으로 `UButton` 팝업 항목 생성.
        *   `OnProjectButtonClicked()`: 유저 클릭 시 해당 `PlanId`를 `HarnessPipelineManager`로 전달하여 파이프라인 트리거.

### 2.7. Mock 데이터 테스팅 지원 (Modified)
*   **`Source/InteReal/Harness/Public/HarnessNetworkComponent.h`**
*   **`Source/InteReal/Harness/Private/HarnessNetworkComponent.cpp`**
    *   **변경 사항**: API 서버가 없는 환경에서도 파이프라인을 테스트할 수 있도록 `bUseMockData` 플래그를 추가했습니다.
    *   **작동 방식**: 플래그 활성화 시 HTTP 요청을 우회하고 프로젝트 최상위 `TestData/` 디렉토리에 존재하는 로컬 JSON 파일(`test1.json` ~ `test12.json`)을 직접 읽어와 델리게이트를 호출(Broadcast)합니다. Content Browser import 감시를 피하기 위해 테스트 JSON은 `Content/` 밖에 둡니다.

## 3. 언리얼 에디터(UE 5.7) 설정 가이드

코드로 구현된 파이프라인을 실제 게임에서 작동시키기 위해 에디터에서 수행해야 할 단계입니다.

### 3.1. 컴포넌트 부착 (Actor 설정)
파이프라인을 관장할 메인 액터(예: `BP_HarnessManager` 또는 `BP_GameMode`)를 생성하고, 다음 컴포넌트들을 추가합니다:
1.  **HarnessNetworkComponent**
    *   디테일 패널에서 `Use Mock Data`가 체크되어 있는지 확인합니다. (테스트용)
2.  **HarnessSaveManagerComponent**
    *   디테일 패널의 `Furniture Data Table`에 가구 에셋 정보가 담긴 데이터 테이블(DataTable)을 할당합니다.
3.  **HarnessGeneratorComponent** (기존에 만든 생성기 컴포넌트)
4.  **HarnessPipelineManager**

### 3.2. UI 위젯 블루프린트 생성 및 바인딩
1.  **위젯 생성**: `UHarnessMainHUD`를 부모 클래스로 하는 위젯 블루프린트(`WBP_MainHUD` 등)를 생성합니다.
2.  **UI 배치**: 
    *   버튼(`Button`)을 하나 배치하고 이름을 **`Btn_LoadProjectList`**로 정확히 지정합니다.
    *   스크롤 박스(`ScrollBox`)를 배치하고 이름을 **`ScrollBox_ProjectList`**로 정확히 지정합니다.
3.  **초기화 연결**: 레벨 블루프린트나 폰의 `BeginPlay`에서 위젯을 생성(`CreateWidget`)하고 `AddToViewport`로 띄운 뒤, 위젯의 **`Setup HUD`** 함수를 호출하여 앞서 만든 `HarnessNetworkComponent`와 `HarnessPipelineManager` 레퍼런스를 연결해 줍니다.

### 3.3. 파이프라인 매니저 초기화
메인 액터의 `BeginPlay`에서 **`Initialize Pipeline`** 함수를 호출하고, 액터에 부착된 3개의 컴포넌트(`Network`, `SaveManager`, `Generator`)를 각각 연결해 줍니다.

### 3.4. 실행 및 테스트
1.  게임을 플레이하고 UI에 있는 `Btn_LoadProjectList` 버튼을 클릭합니다.
2.  스크롤 박스에 `test1` ~ `test11` 버튼들이 생성되는지 확인합니다.
3.  버튼을 클릭하면 `PipelineManager`가 기존 건물을 철거하고 해당 JSON 도면을 로드하여 새로 빌드하는 전체 과정이 자동으로 진행됩니다.
