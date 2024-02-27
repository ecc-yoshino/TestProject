#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"

// 揺れもの処理(ボーン)シーン
class PhysicsBoneScene : public Scene
{
public:
	PhysicsBoneScene();
	~PhysicsBoneScene() override = default;

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
		DirectX::XMFLOAT3	oldWorldPosition;
	};

	Camera								camera;
	FreeCameraController				cameraController;
	Bone								bones[8];
	float								gravity = 0.3f;
	float								maxVelocity = 0.3f;
};
