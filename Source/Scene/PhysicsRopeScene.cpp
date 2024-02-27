#include <imgui.h>
#include <ImGuizmo.h>
#include "Graphics.h"
#include "Scene/PhysicsRopeScene.h"

// コンストラクタ
PhysicsRopeScene::PhysicsRopeScene()
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
		{ 0, 3, -10 },		// 視点
		{ 0, 3, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
	cameraController.SyncCameraToController(camera);

	// ジョイント初期化
	for (int i = 0; i < _countof(joints); ++i)
	{
		Joint& joint = joints[i];
		joint.position.x = i * jointInterval;
		joint.position.y = 3.0f;
		joint.position.z = 0.0f;
		joint.oldPosition = joint.position;
	}

}

// 更新処理
void PhysicsRopeScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// ルートジョイントをギズモで動かす
	DirectX::XMFLOAT3& rootJointPosition = joints[0].position;
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& projection = camera.GetProjection();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(rootJointPosition.x, rootJointPosition.y, rootJointPosition.z));
	ImGuizmo::Manipulate(
		&view._11, &projection._11,
		ImGuizmo::TRANSLATE,
		ImGuizmo::WORLD,
		&world._11,
		nullptr);
	rootJointPosition.x = world._41;
	rootJointPosition.y = world._42;
	rootJointPosition.z = world._43;

	// ロープシミュレーション
	const float gravity = this->gravity * elapsedTime;
	for (int i = 1; i < _countof(joints); ++i)
	{
		Joint& parentJoint = joints[i - 1];
		Joint& joint = joints[i];

		// TODO①:ジョイントの位置を制御し、ロープ表現の物理処理を実装せよ
		{
			DirectX::XMVECTOR ParentPosition = DirectX::XMLoadFloat3(&parentJoint.position);
			DirectX::XMVECTOR OldPosition = DirectX::XMLoadFloat3(&joint.oldPosition);
			DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&joint.position);
			DirectX::XMVECTOR Gravity = DirectX::XMVectorSet(0, -gravity, 0, 0);
			DirectX::XMVECTOR Velocity = DirectX::XMVectorSubtract(Position, OldPosition);
			// ジョイントの位置を動かす
			Velocity = DirectX::XMVectorScale(Velocity, 0.9f);
			Velocity = DirectX::XMVectorAdd(Velocity, Gravity);
			Position = DirectX::XMVectorAdd(Position, Velocity);
			// ジョイント間の長さが変わらないように位置を調整する
			DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Position, ParentPosition);
			Vec = DirectX::XMVector3Normalize(Vec);
			Position = DirectX::XMVectorAdd(ParentPosition, DirectX::XMVectorScale(Vec, jointInterval));
			// 位置情報更新
			joint.oldPosition = joint.position;
			DirectX::XMStoreFloat3(&joint.position, Position);
		}
	}
}

// 描画処理
void PhysicsRopeScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// ロープ描画
	for (const Joint& joint : joints)
	{
		primitiveRenderer->AddVertex(joint.position, { 1, 1, 0, 1 });
	}
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	// グリッド描画
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}

// GUI描画処理
void PhysicsRopeScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once);

	if (ImGui::Begin(u8"揺れもの処理(ロープ)"))
	{
		ImGui::SliderFloat("gravity", &gravity, 0, 1, "%.3f");
	}
	ImGui::End();
}
