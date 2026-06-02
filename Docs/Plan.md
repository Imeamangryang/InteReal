- 기능 분석

1. Placement System
- Placement Preview
- Rotation
- Free Placement
- Grid Snap
- Object Snap
- Collision Validation

2. Furniture Definition System
- Furniture Category
- Placement Rule
- Material Preset
- Surface Type

--- 현재 가구 추가 파이프라인 ---

1. Blender Kit에서 가구 모델 Blender로 가져오기
2. Blender에서 가구 모델 glb로 Export
3. glb 모델을 Unreal Engine으로 Import
4. Unreal Engine에서 Thumbnail 이미지 생성(Render Target & Scene Capture Component 활용)
5. Thumbnail 이미지의 압축 형식을 HDR Compressed로 설정 (Alpha 채널을 없애고 UI로 활용하기 위해)
5. Unreal Engine에서 가구 모델과 Thumbnail 이미지 등을 Data Table에 등록
6. Data Table을 활용하여 Placement System에서 가구 모델과 Thumbnail 이미지를 불러와 UI에 표시

--- 개선 버전 가구 추가 파이프라인 ---

1. Blender Kit에서 가구 모델 Blender로 가져오기
2. Blender에서 가구 모델 glb로 Export
3. glb 모델을 Unreal Engine으로 Import

4. Editor Utility Widget을 활용하여 다음 작업들을 처리
    - Scan Static Meshes(Content/Assets/Models에 저장된 모델들을 스캔하여 목록화)
    - Thumbnail 자동 생성
    - Thumbnail 이미지의 압축 형식을 HDR Compressed로 설정
    - Data Table Row 자동 생성 (모델과 Thumbnail 이미지를 Data Table에 등록)
5. UI에서 Data Table을 활용하여 가구 모델과 Thumbnail 이미지를 불러와 표시