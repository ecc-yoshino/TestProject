#include <imgui.h>
#include <ImGuizmo.h>
#include "Graphics.h"
#include "Scene/SphereVsTriangleCollisionScene.h"

// コンストラクタ
SphereVsTriangleCollisionScene::SphereVsTriangleCollisionScene()
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
		{ 0, 10, 10 },		// 視点
		{ 0, 0, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
	cameraController.SyncCameraToController(camera);

	// オブジェクト初期化
	obj.position = { 0, 2, 1 };
}

// 更新処理
void SphereVsTriangleCollisionScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// オブジェクト移動操作
	const float speed = 2.0f * elapsedTime;
	DirectX::XMFLOAT3 vec = { 0, 0, 0 };
	if (GetAsyncKeyState(VK_UP) & 0x8000)
	{
		vec.z += speed;
	}
	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
	{
		vec.z -= speed;
	}
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
	{
		vec.x += speed;
	}
	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
	{
		vec.x -= speed;
	}
	if (GetAsyncKeyState('Z') & 0x8000)
	{
		vec.y += speed;
	}
	if (GetAsyncKeyState('X') & 0x8000)
	{
		vec.y -= speed;
	}
	// カメラの向きを考慮した移動処理
	const DirectX::XMFLOAT3& front = camera.GetFront();
	const DirectX::XMFLOAT3& right = camera.GetRight();
	float frontLengthXZ = sqrtf(front.x * front.x + front.z * front.z);
	float rightLengthXZ = sqrtf(right.x * right.x + right.z * right.z);
	float frontX = front.x / frontLengthXZ;
	float frontZ = front.z / frontLengthXZ;
	float rightX = right.x / rightLengthXZ;
	float rightZ = right.z / rightLengthXZ;
	obj.position.x += frontX * vec.z + rightX * vec.x;
	obj.position.z += frontZ * vec.z + rightZ * vec.x;
	obj.position.y += vec.y;
}

// 描画処理
void SphereVsTriangleCollisionScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();

	// 三角形頂点
	DirectX::XMFLOAT3 v[3] = {
		{ -2, 2, 0 },
		{ 0, 1, 2 },
		{ 2, 2, 0 },
	};
	// 球と三角形の衝突処理
	DirectX::XMFLOAT4 c = { 0, 1, 0, 1 };
	DirectX::XMFLOAT3 position, normal;
	if (SphereIntersectTriangle(obj.position, obj.radius, v[0], v[1], v[2], position, normal))
	{
		obj.position = position;
		c = { 1, 0, 0, 1 };
	}

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	// 三角形ポリゴン描画
	primitiveRenderer->AddVertex(v[0], c);
	primitiveRenderer->AddVertex(v[1], c);
	primitiveRenderer->AddVertex(v[2], c);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// 三角形エッジ描画
	primitiveRenderer->AddVertex(v[0], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[1], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[2], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[0], { 0, 0, 1, 1 });
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	// グリッド描画
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// シェイプ描画
	shapeRenderer->DrawSphere(obj.position, obj.radius, obj.color);
	shapeRenderer->Render(dc, camera.GetView(), camera.GetProjection());
}

// GUI描画処理
void SphereVsTriangleCollisionScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once);

	if (ImGui::Begin(u8"球と三角形の衝突処理"))
	{
		ImGui::Text(u8"前後左右移動操作：方向キー");
		ImGui::Text(u8"上下移動操作：ZX");
	}
	ImGui::End();
}

// 球と三角形との交差を判定する
bool SphereVsTriangleCollisionScene::SphereIntersectTriangle(
	const DirectX::XMFLOAT3& sphereCenter,
	const float sphereRadius,
	const DirectX::XMFLOAT3& triangleVertexA,
	const DirectX::XMFLOAT3& triangleVertexB,
	const DirectX::XMFLOAT3& triangleVertexC,
	DirectX::XMFLOAT3& hitPosition,
	DirectX::XMFLOAT3& hitNormal)
{
	// TODO①:球が三角形に押し出される衝突処理を実装せよ
	{
		// 三角形頂点
		DirectX::XMVECTOR TriangleVertex[3] =
		{
			DirectX::XMLoadFloat3(&triangleVertexA),
			DirectX::XMLoadFloat3(&triangleVertexB),
			DirectX::XMLoadFloat3(&triangleVertexC),
		};

		// 面法線を求める
		DirectX::XMVECTOR Edge[3];
		Edge[0] = DirectX::XMVectorSubtract(TriangleVertex[1], TriangleVertex[0]);
		Edge[1] = DirectX::XMVectorSubtract(TriangleVertex[2], TriangleVertex[1]);

		DirectX::XMVECTOR TriangleNormal = DirectX::XMVector3Cross(Edge[0], Edge[1]);
		TriangleNormal = DirectX::XMVector3Normalize(TriangleNormal);

		// 平面と球の距離を求める
		DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&sphereCenter);
		DirectX::XMVECTOR Dot1 = DirectX::XMVector3Dot(TriangleNormal, SphereCenter);
		DirectX::XMVECTOR Dot2 = DirectX::XMVector3Dot(TriangleNormal, TriangleVertex[0]);
		DirectX::XMVECTOR Distance = DirectX::XMVectorSubtract(Dot1, Dot2);
		float distance = DirectX::XMVectorGetX(Distance);

		// 球の中心が面に接しているか、負の方向にある場合は当たらない
		if (distance < 0.0f)
		{
			return false;
		}
		// 半径以上離れていれば当たらない
		if (distance > sphereRadius)
		{
			return false;
		}

		// 球が三角形内部に存在するか
		Edge[2] = DirectX::XMVectorSubtract(TriangleVertex[0], TriangleVertex[2]);
		bool outside = false;
		DirectX::XMVECTOR Vec[3];
		for (int i = 0; i < 3; ++i)
		{
			Vec[i] = DirectX::XMVectorSubtract(SphereCenter, TriangleVertex[i]);
			DirectX::XMVECTOR Cross = DirectX::XMVector3Cross(Edge[i], Vec[i]);
			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(TriangleNormal, Cross);
			outside |= DirectX::XMVectorGetX(Dot) < 0.0f;
		}
		// 三角の内側なので交差する
		if (!outside)
		{
			// 反射は面の逆方向で(半径-距離)の長さ
			float depth = sphereRadius - distance;
			// めり込み分押し出し処理
			DirectX::XMVECTOR Reflection = DirectX::XMVectorScale(TriangleNormal, depth);
			SphereCenter = DirectX::XMVectorAdd(SphereCenter, Reflection);

			DirectX::XMStoreFloat3(&hitPosition, SphereCenter);
			DirectX::XMStoreFloat3(&hitNormal, TriangleNormal);

			return true;
		}

		// セグメントとの比較
		float radiusSq = sphereRadius * sphereRadius;
		for (int i = 0; i < 3; ++i)
		{
			// 辺の射影値を求める
			float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec[i], Edge[i]));
			if (t > 0.0f)
			{
				// 辺の始点から終点までのベクトルと始点から球までのベクトルが同一の場合、
				// 内積値が辺の長さの２乗になる性質を利用して辺から球までの最短ベクトルを求める
				float edgeLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Edge[i]));
				if (t >= edgeLengthSq)
				{
					Vec[i] = DirectX::XMVectorSubtract(Vec[i], Edge[i]);
				}
				else
				{
					t /= edgeLengthSq;
					Vec[i] = DirectX::XMVectorSubtract(Vec[i], DirectX::XMVectorScale(Edge[i], t));
				}
			}
			// 辺から球までの最短ベクトルの距離が半径以下ならめり込んでいる
			float lengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vec[i]));
			if (lengthSq <= radiusSq)
			{
				// めり込み量算出
				float depth = sphereRadius - sqrtf(lengthSq);

				// めり込み分押し出し処理
				const DirectX::XMVECTOR& Position = Vec[i];
				DirectX::XMVECTOR Reflection = DirectX::XMVector3Normalize(Position);
				Reflection = DirectX::XMVectorScale(Reflection, depth);
				SphereCenter = DirectX::XMVectorAdd(SphereCenter, Reflection);

				DirectX::XMStoreFloat3(&hitPosition, SphereCenter);
				DirectX::XMStoreFloat3(&hitNormal, TriangleNormal);

				return true;
			}
		}
	}
	return false;
}
