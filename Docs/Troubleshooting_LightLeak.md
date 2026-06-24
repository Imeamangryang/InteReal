# 트러블슈팅: 절차적 생성 건물의 빛샘(Light Leaking) 및 천장 투과 현상 해결

## 1. 문제 상황 (Problem)
1. **모서리 빛샘 현상**: JSON 데이터를 기반으로 절차적 생성(Procedural Generation)된 3D 평면도의 벽체와 천장/바닥이 만나는 경계선 틈새로 강한 태양광(주황색 빛)이 새어 들어오는 현상 발생.
2. **천장 빛 투과 현상**: 실내를 내려다보기 위해 천장 메쉬의 윗면(Top Face) 생성을 생략하고 내부가 보이도록 처리했으나, 투명해진 천장을 통해 직사광선이 내부로 그대로 쏟아져 들어옴.

---

## 2. 원인 분석 (Cause)

### 1) 경계면 픽셀 오차 및 조명 편향(Shadow Bias)
* 런타임에 동적으로 생성된 단면(Plane) 형태의 벽체 메쉬들이 두께 없이 정확히 끝단에서만 맞닿아 있음.
* 언리얼 엔진의 `DirectionalLight` 기본 **그림자 편향(Shadow Bias)** 값(0.5)으로 인해 그림자 시작 지점이 물체 표면에서 살짝 밀려나면서 모서리에 그림자 공백(틈새)이 발생함.

### 2) 투명 메쉬의 그림자 생성(Hidden Shadow) 설정 오류
* 천장을 투명하게 만들면서 그림자만 캐스팅하기 위해 전용 블록인 `CeilingShadowBlocker`를 추가했음.
* 그러나 해당 컴포넌트의 렌더링 속성을 `SetRenderInMainPass(false)`로 설정하여 시각적으로 숨기려 했음.
* **루멘(Lumen) 및 가상 섀도우 맵(VSM) 한계**: 언리얼 엔진 5에서는 `Main Pass`에서 렌더링이 제외된 오브젝트의 경우 섀도우 파이프라인이나 루멘 표면 캐시 계산에서 누락되어 `bCastHiddenShadow = true` 설정이 정상 작동하지 않고 빛이 완전히 통과하는 문제가 발생함.

---

## 3. 해결 과정 및 적용 코드 (Solution)

### 해결 1: 미세 틈새 차단을 위한 컨택트 섀도우 및 Bias 설정
빛을 총괄하는 `EnvController.cpp` 파일에서 조명 컴포넌트(`SunLight`)의 옵션을 조정하여 물리적인 미세 틈새를 소프트웨어적으로 메움.
* **ShadowBias 낮춤**: `0.5` -> `0.1`로 낮추어 그림자를 메쉬 표면에 바짝 밀착시킴.
* **ContactShadowLength 주의**: 이전에 빛샘을 막기 위해 Contact Shadow를 적용했으나, 화면 공간(Screen Space) 기반이라 카메라 이동 시 그림자가 지지직거리며 움직이는 노이즈 현상(Artifact)이 심하게 발생하여 현재는 `0.0f`로 비활성화함.

**[적용 파일: `Source/InteReal/Lighting/Controller/EnvController.cpp`]**
```cpp
if (SunLight)
{
    // 그림자 시작 지점을 물체에 더 가깝게 당김 (모서리 빛샘 방지)
    SunLight->SetShadowBias(0.1f);
    
    // 화면 공간 그림자(Contact Shadow)는 카메라 이동 시 지지직거리는 노이즈(Artifact)를 유발하므로 비활성화
    SunLight->ContactShadowLength = 0.0f;
    
    // 추가적인 반사광/기울기 편향 초기화
    SunLight->SetSpecularScale(0.0f);
    SunLight->SetShadowSlopeBias(0.0f);
}
```

### 해결 2: 그림자 전용 블록(Shadow Blocker)의 렌더링 플래그 표준화
천장 및 외벽을 투명하게 만들면서 그림자를 남기기 위해, `SetRenderInMainPass(false)`를 폐기하고 언리얼 엔진의 표준 '숨겨진 그림자' 렌더링 플래그로 수정함.
* `SetHiddenInGame(true)`: 게임 내에서 메쉬를 시각적으로 완벽히 숨김.
* `SetRenderInMainPass(true)`: 섀도우 패스가 그림자를 정상적으로 그릴 수 있도록 렌더링 파이프라인 유지.

**[적용 파일: `Source/InteReal/Harness/Private/HarnessGeneratorComponent_Planes.cpp`]**
```cpp
// 실제 게임에는 보이지 않고 그림자만 그리도록 언리얼 엔진 표준 플래그 설정
CeilingShadowBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

// 메쉬를 시각적으로 숨김 (MainPass를 끄는 대신 HiddenInGame 사용)
CeilingShadowBlocker->SetHiddenInGame(true, true); 
CeilingShadowBlocker->SetRenderInMainPass(true);   // HiddenShadow가 정상 작동하려면 true 유지 필수

CeilingShadowBlocker->CastShadow = true;
CeilingShadowBlocker->bCastHiddenShadow = true;    // 숨겨진 상태에서도 그림자 투사
CeilingShadowBlocker->bCastShadowAsTwoSided = true;
```

---

## 4. 결과 (Result)
* 모서리와 틈새로 새어 들어오던 강한 직사광선(빛샘)이 `ShadowBias` 조절을 통해 크게 완화됨. (단, `ContactShadow`는 노이즈 이슈로 사용 중단)
* 천장 윗면이 렌더링되지 않아 실내가 시원하게 내려다보이는 탑다운(Top-Down) 뷰 상태에서도, `CeilingShadowBlocker`의 히든 섀도우가 루멘(Lumen) 환경에서 정상 작동하여 **자연스러운 실내 음영이 유지**됨.
