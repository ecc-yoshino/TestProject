#include <algorithm>
#include <imgui.h>
#include <ImGuizmo.h>
#include "Graphics.h"
#include "Scene/PhysicsBoneScene.h"

// コンストラクタ
PhysicsBoneScene::PhysicsBoneScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// カメラ設定
	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		screenWidth / screenHeight,			// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	camera.SetLookAt(
		{ 10, 3, 0 },		// 視点
		{ 0, 3, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
	cameraController.SyncCameraToController(camera);

	// ボーンデータ初期化
	for (int i = 0; i < _countof(bones); ++i)
	{
		Bone& bone = bones[i];

		Bone* parent = nullptr;
		if (i == 0)
		{
			bone.localPosition = { 0, 3, 0 };
			bone.localRotation = { 0, 0, 0, 1 };
		}
		else
		{
			bone.localPosition = { 0, 0, 1 };
			bone.localRotation = { 0, 0, 0, 1 };

			parent = &bones[i - 1];
		}

		// 行列計算
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(bone.localPosition.x, bone.localPosition.y, bone.localPosition.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&bone.localRotation));
		DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(R, T);
		DirectX::XMMATRIX ParentWorldTransform = parent != nullptr ? DirectX::XMLoadFloat4x4(&parent->worldTransform) : DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
		DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

		bone.oldWorldPosition.x = bone.worldTransform._41;
		bone.oldWorldPosition.y = bone.worldTransform._42;
		bone.oldWorldPosition.z = bone.worldTransform._43;
	}

}

// 更新処理
void PhysicsBoneScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// ルートボーンをギズモで動かす
	Bone& root = bones[0];
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& projection = camera.GetProjection();
	ImGuizmo::Manipulate(
		&view._11, &projection._11,
		ImGuizmo::TRANSLATE,
		ImGuizmo::WORLD,
		&root.worldTransform._11,
		nullptr);

	// 次のボーンのワールド行列計算
	Bone& next = bones[1];
	next.oldWorldPosition.x = bones[1].worldTransform._41;
	next.oldWorldPosition.y = bones[1].worldTransform._42;
	next.oldWorldPosition.z = bones[1].worldTransform._43;
	DirectX::XMMATRIX RootWorldTransform = DirectX::XMLoadFloat4x4(&root.worldTransform);
	DirectX::XMMATRIX NextLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&next.localRotation));
	DirectX::XMMATRIX NextLocalPositionTransform = DirectX::XMMatrixTranslation(next.localPosition.x, next.localPosition.y, next.localPosition.z);
	DirectX::XMMATRIX NextLocalTransform = DirectX::XMMatrixMultiply(NextLocalRotationTransform, NextLocalPositionTransform);
	DirectX::XMMATRIX NextWorldTransform = DirectX::XMMatrixMultiply(NextLocalTransform, RootWorldTransform);
	DirectX::XMStoreFloat4x4(&next.worldTransform, NextWorldTransform);

	// ボーンシミュレーション
	const float gravity = this->gravity * elapsedTime;
	for (int i = 2; i < _countof(bones); ++i)
	{
		// 各ボーン情報取得
		Bone& parent = bones[i - 2];
		Bone& bone = bones[i - 1];
		Bone& child = bones[i];

		// TODO①:ボーンの回転制御でロープ表現の物理処理を実装せよ
		{
			DirectX::XMMATRIX ChildWorldTransform = DirectX::XMLoadFloat4x4(&child.worldTransform);
			DirectX::XMVECTOR ChildWorldPosition = ChildWorldTransform.r[3];
			DirectX::XMVECTOR ChildOldWorldPosition = DirectX::XMLoadFloat3(&child.oldWorldPosition);
			DirectX::XMStoreFloat3(&child.oldWorldPosition, ChildWorldPosition);

			// 前回位置から今回位置の差分と重力による速力を算出する
			DirectX::XMVECTOR Gravity = DirectX::XMVectorSet(0, -gravity, 0, 0);
			DirectX::XMVECTOR Velocity = DirectX::XMVectorSubtract(ChildWorldPosition, ChildOldWorldPosition);
			DirectX::XMVECTOR Length = DirectX::XMVector3Length(Velocity);
			Velocity = DirectX::XMVectorScale(Velocity, 0.9f);
			Velocity = DirectX::XMVectorAdd(Velocity, Gravity);

			// 子ボーンの位置を動かす
			ChildWorldPosition = DirectX::XMVectorAdd(ChildWorldPosition, Velocity);

			// 現在のボーンのローカル空間方向を求める
			DirectX::XMVECTOR ChildLocalPosition = DirectX::XMLoadFloat3(&child.localPosition);
			DirectX::XMVECTOR LocalDirection = DirectX::XMVector3Normalize(ChildLocalPosition);

			// 最終的なボーンのローカル空間方向を求める
			DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
			DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
			DirectX::XMVECTOR LocalTargetDirection = DirectX::XMVector3Transform(ChildWorldPosition, InverseWorldTransform);
			LocalTargetDirection = DirectX::XMVector3Normalize(LocalTargetDirection);

			// 回転角度算出
			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(LocalDirection, LocalTargetDirection);
			float angle = acosf(DirectX::XMVectorGetX(Dot));
			if (angle > FLT_EPSILON)
			{
				// ローカル回転軸算出
				DirectX::XMVECTOR LocalAxis = DirectX::XMVector3Cross(LocalDirection, LocalTargetDirection);
				LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

				// ローカル回転処理
				DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
				DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone.localRotation);
				LocalRotation = DirectX::XMQuaternionMultiply(LocalRotationAxis, LocalRotation);
				DirectX::XMStoreFloat4(&bone.localRotation, LocalRotation);
			}

			// ワールド行列更新
			DirectX::XMMATRIX LocalPositionTransform = DirectX::XMMatrixTranslation(bone.localPosition.x, bone.localPosition.y, bone.localPosition.z);
			DirectX::XMMATRIX LocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&bone.localRotation));
			DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(LocalRotationTransform, LocalPositionTransform);
			DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&parent.worldTransform);
			WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
			DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

			// 子ワールド行列更新
			DirectX::XMMATRIX ChildLocalPositionTransform = DirectX::XMMatrixTranslation(child.localPosition.x, child.localPosition.y, child.localPosition.z);
			DirectX::XMMATRIX ChildLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&child.localRotation));
			DirectX::XMMATRIX ChildLocalTransform = DirectX::XMMatrixMultiply(ChildLocalRotationTransform, ChildLocalPositionTransform);
			ChildWorldTransform = DirectX::XMMatrixMultiply(ChildLocalTransform, WorldTransform);
			DirectX::XMStoreFloat4x4(&child.worldTransform, ChildWorldTransform);
		}
	}

}

// 描画処理
void PhysicsBoneScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();

	// ボーン描画
	for (size_t i = 0; i < _countof(bones) - 1; ++i)
	{
		const Bone& bone = bones[i];
		const Bone& child = bones[i + 1];

		DirectX::XMFLOAT4X4 world;
		DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&bone.worldTransform);
		float length = child.localPosition.z;
		World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[0]), length);
		World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[1]), length);
		World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[2]), length);
		DirectX::XMStoreFloat4x4(&world, World);
		primitiveRenderer->DrawAxis(world, { 1, 1, 0, 1 });
		shapeRenderer->DrawBone(bone.worldTransform, length, { 1, 1, 0, 1 });
	}

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// グリッド描画
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// シェイプ描画
	shapeRenderer->Render(dc, camera.GetView(), camera.GetProjection());
}

// GUI描画処理
void PhysicsBoneScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();

	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Once);

	if (ImGui::Begin(u8"揺れもの処理(ボーン)"))
	{
		ImGui::SliderFloat("gravity", &gravity, 0, 1, "%.3f");
	}
	ImGui::End();
}
