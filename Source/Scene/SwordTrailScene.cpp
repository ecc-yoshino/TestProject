#include <imgui.h>
#include "Graphics.h"
#include "Scene/SwordTrailScene.h"

// コンストラクタ
SwordTrailScene::SwordTrailScene()
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
		{ 8, 3, 0 },		// 視点
		{ 0, 2, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);

	// モデル読み込み
	character = std::make_shared<Model>(device, "Data/Model/RPG-Character/RPG-Character.glb");
	character->GetNodePoses(nodePoses);
	weapon = std::make_shared<Model>(device, "Data/Model/RPG-Character/2Hand-Sword.glb");
}

// 更新処理
void SwordTrailScene::Update(float elapsedTime)
{
	const int animationIndex = character->GetAnimationIndex("Slash");
	
	// 指定時間のアニメーションの姿勢を取得
	character->ComputeAnimation(animationIndex, animationSeconds, nodePoses);

	// アニメーション時間更新
	const Model::Animation& animation = character->GetAnimations().at(animationIndex);
	animationSeconds += elapsedTime;
	if (animationSeconds > animation.secondsLength)
	{
		animationSeconds -= animation.secondsLength;
	}

	// 姿勢更新
	character->SetNodePoses(nodePoses);

	// キャラクタートランスフォーム更新
	DirectX::XMFLOAT4X4 worldTransform;
	DirectX::XMStoreFloat4x4(&worldTransform, DirectX::XMMatrixIdentity());
	character->UpdateTransform(worldTransform);

	// 武器トランスフォーム更新
	int handNodeIndex = character->GetNodeIndex("B_R_Hand");
	const Model::Node& handNode = character->GetNodes().at(handNodeIndex);
	weapon->UpdateTransform(handNode.worldTransform);

	// TODO②:頂点バッファに保存していた頂点データを１フレーム分ずらせ
	{
		// 保存していた座標をずらす
		for (int i = 0; i < 2; ++i)
		{
			for (int j = MAX_POLYGON - 1; j > 0; --j)
			{
				trailPositions[i][j] = trailPositions[i][j - 1];
			}
		}
	}

	// TODO①:剣の根本と先端の座標を取得し、頂点バッファに最新の頂点座標を保存せよ
	// trailPositions[2][MAX_POLYGON] ← 頂点バッファ
	{
		// 剣の原点から根本と先端までのオフセット値
		DirectX::XMVECTOR RootOffset = DirectX::XMVectorSet(0, 0, 0.3f, 0);
		DirectX::XMVECTOR TipOffset = DirectX::XMVectorSet(0, 0, 2.3f, 0);

		DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&handNode.worldTransform);
		DirectX::XMVECTOR V = DirectX::XMVector3Normalize(W.r[0]);
		DirectX::XMVECTOR Root = DirectX::XMVector3Transform(RootOffset, W);
		DirectX::XMVECTOR Tip = DirectX::XMVector3Transform(TipOffset, W);
		DirectX::XMStoreFloat3(&trailPositions[0][0], Root);
		DirectX::XMStoreFloat3(&trailPositions[1][0], Tip);
	}

	DirectX::XMFLOAT4 color = { 1, 0, 0, 1 };

	// ポリゴン作成
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	if (!spline)
	{
		// 保存していた頂点バッファでポリゴンを作る
		for (int i = 0; i < MAX_POLYGON; ++i)
		{
			primitiveRenderer->AddVertex(trailPositions[0][i], color);
			primitiveRenderer->AddVertex(trailPositions[1][i], color);
		}
	}
	else
	{
		// TODO③:保存していた頂点バッファを用いてスプライン補完処理を行い、
		//        滑らかなポリゴンを描画せよ
		{
			for (int i = 0; i < MAX_POLYGON - 3; ++i)
			{
				const int division = 10;
				DirectX::XMVECTOR Root0 = DirectX::XMLoadFloat3(&trailPositions[0][i + 0]);
				DirectX::XMVECTOR Root1 = DirectX::XMLoadFloat3(&trailPositions[0][i + 1]);
				DirectX::XMVECTOR Root2 = DirectX::XMLoadFloat3(&trailPositions[0][i + 2]);
				DirectX::XMVECTOR Root3 = DirectX::XMLoadFloat3(&trailPositions[0][i + 3]);
				DirectX::XMVECTOR Tip0 = DirectX::XMLoadFloat3(&trailPositions[1][i + 0]);
				DirectX::XMVECTOR Tip1 = DirectX::XMLoadFloat3(&trailPositions[1][i + 1]);
				DirectX::XMVECTOR Tip2 = DirectX::XMLoadFloat3(&trailPositions[1][i + 2]);
				DirectX::XMVECTOR Tip3 = DirectX::XMLoadFloat3(&trailPositions[1][i + 3]);
				for (int j = 1; j < division; ++j)
				{
					float t = j / static_cast<float>(division);

					DirectX::XMVECTOR Root = DirectX::XMVectorCatmullRom(Root0, Root1, Root2, Root3, t);
					DirectX::XMVECTOR Tip = DirectX::XMVectorCatmullRom(Tip0, Tip1, Tip2, Tip3, t);
					DirectX::XMFLOAT3 root, tip;
					DirectX::XMStoreFloat3(&root, Root);
					DirectX::XMStoreFloat3(&tip, Tip);
					primitiveRenderer->AddVertex(root, color);
					primitiveRenderer->AddVertex(tip, color);
				}
			}
		}
	}
}

// 描画処理
void SwordTrailScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ModelRenderer* modelRenderer = Graphics::Instance().GetModelRenderer();

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// ポリゴン描画
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// グリッド描画
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// 描画コンテキスト設定
	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.camera = &camera;
	rc.lightManager = &lightManager;

	// モデル描画
	modelRenderer->Draw(ShaderId::Basic, character);
	modelRenderer->Draw(ShaderId::Basic, weapon);
	modelRenderer->Render(rc);
}

// GUI描画処理
void SwordTrailScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once);

	if (ImGui::Begin(u8"剣の軌跡処理"))
	{
		ImGui::Checkbox(u8"スプライン補完", &spline);
	}
	ImGui::End();
}
