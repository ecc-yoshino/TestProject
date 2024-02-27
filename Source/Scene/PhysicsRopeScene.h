#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"

// 揺れもの処理(ロープ)シーン
class PhysicsRopeScene : public Scene
{
public:
	PhysicsRopeScene();
	~PhysicsRopeScene() override = default;

	// 更新処理
	void Update(float elapsedTime) override;

	// 描画処理
	void Render(float elapsedTime) override;

	// GUI描画処理
	void DrawGUI() override;

private:
	struct Joint
	{
		DirectX::XMFLOAT3	position;
		DirectX::XMFLOAT3	oldPosition;
	};

	Camera								camera;
	FreeCameraController				cameraController;

	float		jointInterval = 1.0f;
	Joint		joints[5];
	float		gravity = 0.3f;
};
