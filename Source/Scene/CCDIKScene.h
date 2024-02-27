#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"

// CCD-IK処理シーン
class CCDIKScene : public Scene
{
public:
	CCDIKScene();
	~CCDIKScene() override = default;

	// 更新処理
	void Update(float elapsedTime) override;

	// 描画処理
	void Render(float elapsedTime) override;

	// GUI描画処理
	void DrawGUI() override;

private:
	struct Bone
	{
		DirectX::XMFLOAT3	localPosition;
		DirectX::XMFLOAT4	localRotation;
		DirectX::XMFLOAT4X4	worldTransform;
	};

	Camera								camera;
	FreeCameraController				cameraController;
	Bone								bones[5];
	DirectX::XMFLOAT4X4					targetTransform;
	int									quality = 5;
};
