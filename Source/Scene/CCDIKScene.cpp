#include <imgui.h>
#include <ImGuizmo.h>
#include "Graphics.h"
#include "Scene/CCDIKScene.h"

// コンストラクタ
CCDIKScene::CCDIKScene()
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
		{ 10, 3, 5 },		// 視点
		{ 0, 3, 5 },		// 注視点
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
	}
	targetTransform = bones[_countof(bones) - 1].worldTransform;
}

// 更新処理
void CCDIKScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// ターゲットをギズモで動かす
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& projection = camera.GetProjection();
	ImGuizmo::Manipulate(
		&view._11, &projection._11,
		ImGuizmo::TRANSLATE,
		ImGuizmo::WORLD,
		&targetTransform._11,
		nullptr);

	DirectX::XMMATRIX TargetWorldTransform = DirectX::XMLoadFloat4x4(&targetTransform);
	DirectX::XMVECTOR TargetWorldPosition = TargetWorldTransform.r[3];

	bool firsts[_countof(bones)] = {};
	// IKシミュレーション
	Bone& effector = bones[_countof(bones) - 1];
	for (int q = 0; q < quality; ++q)
	{
		// エフェクタボーンがターゲット位置を向くように先端から根元に向かって計算する
		for (int i = _countof(bones) - 2; i > 0; --i)
		{
			Bone& bone = bones[i];

			// TODO①:CCD-IKアルゴリズムでエフェクタボーン位置がターゲット位置に近づくようにボーンの回転制御をせよ
			{
				// 自身のボーンから「エフェクタボーンへのベクトル」と「ターゲット位置へのベクトル」の角度を算出する
				DirectX::XMMATRIX EffectorWorldTransform = DirectX::XMLoadFloat4x4(&effector.worldTransform);
				DirectX::XMVECTOR EffectorWorldPosition = EffectorWorldTransform.r[3];

				DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
				DirectX::XMVECTOR WorldPosition = WorldTransform.r[3];
				DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);

				DirectX::XMVECTOR EffectorLocalPosition = DirectX::XMVector3Transform(EffectorWorldPosition, InverseWorldTransform);
				DirectX::XMVECTOR TargetLocalPosition = DirectX::XMVector3Transform(TargetWorldPosition, InverseWorldTransform);

				DirectX::XMVECTOR EffectorLocalDirection = DirectX::XMVector3Normalize(EffectorLocalPosition);
				DirectX::XMVECTOR TargetLocalDirection = DirectX::XMVector3Normalize(TargetLocalPosition);

				DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(EffectorLocalDirection, TargetLocalDirection);
				float angle = acosf(DirectX::XMVectorGetX(Dot));
				if (angle > FLT_EPSILON)
				{
					// 回転軸算出
					DirectX::XMVECTOR LocalAxis = DirectX::XMVector3Cross(EffectorLocalDirection, TargetLocalDirection);
					LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

					// 回転処理
					DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
					DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone.localRotation);
					LocalRotation = DirectX::XMQuaternionMultiply(LocalRotationAxis, LocalRotation);
					DirectX::XMStoreFloat4(&bone.localRotation, LocalRotation);

					// 自分以下のワールド行列計算
					for (int j = i; j < _countof(bones); ++j)
					{
						Bone& p = bones[j - 1];
						Bone& b = bones[j];

						DirectX::XMMATRIX LocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&b.localRotation));
						DirectX::XMMATRIX LocalPositionTransform = DirectX::XMMatrixTranslation(b.localPosition.x, b.localPosition.y, b.localPosition.z);
						DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(LocalRotationTransform, LocalPositionTransform);
						DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&p.worldTransform);
						DirectX::XMMATRIX WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
						DirectX::XMStoreFloat4x4(&b.worldTransform, WorldTransform);
					}
				}
			}
		}
	}
}

// 描画処理
void CCDIKScene::Render(float elapsedTime)
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
void CCDIKScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();

	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_Once);

	if (ImGui::Begin(u8"3本以上のボーンIK制御"))
	{
		ImGui::DragInt("Quality", &quality, 1, 1, 100);
	}
	ImGui::End();
}
