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

---

## 5. 2026-06-25 추가 트러블슈팅: 원거리 빛샘 및 창틀 그림자 지터

### 문제 상황
1. **원거리 빛샘**
   * 가까이서 보면 벽/바닥/천장 접합부가 정상인데, 카메라가 멀어지면 빛이 새는 것처럼 보임.
   * 메시 자체가 뚫린 것처럼 보였지만, 가까운 거리에서는 정상적으로 차폐됨.

2. **창문으로 들어온 햇빛 주변 그림자 지터**
   * 창틀을 통과한 직사광선이 바닥에 강한 명암 경계를 만들 때, 그림자 가장자리가 지지직거리거나 반짝이는 현상이 발생함.
   * 특히 얇은 창틀/프레임이 만드는 고대비 그림자 경계에서 두드러짐.

### 원인 분석

#### 1) 원거리 빛샘: Virtual Shadow Map 비활성화
기존 프로젝트 설정은 다음과 같았음.

```ini
r.Shadow.Virtual.Enable=0
```

이 상태에서는 원거리 그림자가 일반 섀도우 맵/캐스케이드 정밀도에 크게 의존한다. 카메라가 멀어질수록 섀도우 맵 한 픽셀이 커지고 shadow bias 영향도 커져, 벽-바닥-천장처럼 얇고 긴 접합부에서 그림자가 실제 위치보다 밀려 보일 수 있다.

따라서 실제 메시가 열려 있지 않아도, 원거리에서는 빛이 틈으로 들어오는 것처럼 보이는 artifact가 발생했다.

확인 방법:

```ini
r.Shadow.Virtual.Enable 1
```

콘솔에서 위 값을 적용하자 원거리에서도 빛샘이 사라졌으므로, 원인은 geometry 문제가 아니라 shadow precision 문제로 확정했다.

#### 2) 그림자 지터: FXAA의 시간 안정화 부족
기존 프로젝트 설정은 다음과 같았음.

```ini
r.AntiAliasingMethod=1
```

`1`은 FXAA 계열이다. FXAA는 단일 프레임의 화면 가장자리를 후처리로 완화하는 방식이라, 프레임 간 누적 안정화가 약하다. Virtual Shadow Map은 원거리 그림자 정밀도는 개선하지만, 창틀처럼 얇은 물체가 만드는 강한 그림자 경계는 여전히 샘플링 노이즈나 계단 현상이 보일 수 있다.

FXAA 상태에서는 이 그림자 경계가 시간적으로 안정화되지 않아 카메라 이동이나 미세한 화면 변화에서 지지직거리는 것처럼 보였다.

확인 방법:

```ini
r.AntiAliasingMethod 2
```

콘솔에서 TAA로 바꾸자 창틀 그림자 주변 지터가 즉시 완화되었으므로, 원인은 AA 방식으로 확정했다.

### 적용한 수정

적용 파일:

```text
Config/DefaultEngine.ini
```

변경값:

```ini
r.Shadow.Virtual.Enable=1
r.AntiAliasingMethod=2
```

### 왜 이 수정이 맞는가

* `r.Shadow.Virtual.Enable=1`
  * Virtual Shadow Map을 켜서 원거리에서도 그림자 해상도와 접합부 표현을 더 안정적으로 유지한다.
  * 벽/바닥/천장 접합부가 멀리서 빛샘처럼 보이던 문제를 줄인다.

* `r.AntiAliasingMethod=2`
  * TAA를 사용해 그림자 경계와 얇은 프레임 주변의 프레임 간 흔들림을 누적 안정화한다.
  * 창틀 그림자처럼 고대비/얇은 그림자 edge의 지터를 FXAA보다 훨씬 잘 억제한다.

### 개선 결과

* 원거리에서 보이던 벽/바닥/천장 접합부 빛샘이 사라짐.
* 창문으로 들어온 햇빛 주변의 그림자 경계가 안정화됨.
* 창틀 그림자 주변의 지지직거림이 크게 줄어듦.
* 메시 생성 로직을 억지로 변경하지 않고, 실제 원인인 렌더링 정밀도와 temporal 안정화 문제를 설정으로 해결함.
