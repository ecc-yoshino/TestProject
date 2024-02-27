#include <imgui.h>
#include <ImGuizmo.h>
#include "Graphics.h"
#include "Scene/SphereVsTriangleCollisionScene.h"

// �R���X�g���N�^
SphereVsTriangleCollisionScene::SphereVsTriangleCollisionScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// �J�����ݒ�
	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// ��p
		screenWidth / screenHeight,			// ��ʃA�X�y�N�g��
		0.1f,								// �j�A�N���b�v
		1000.0f								// �t�@�[�N���b�v
	);
	camera.SetLookAt(
		{ 0, 10, 10 },		// ���_
		{ 0, 0, 0 },		// �����_
		{ 0, 1, 0 }			// ��x�N�g��
	);
	cameraController.SyncCameraToController(camera);

	// �I�u�W�F�N�g������
	obj.position = { 0, 2, 1 };
}

// �X�V����
void SphereVsTriangleCollisionScene::Update(float elapsedTime)
{
	// �J�����X�V����
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// �I�u�W�F�N�g�ړ�����
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
	// �J�����̌������l�������ړ�����
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

// �`�揈��
void SphereVsTriangleCollisionScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();

	// �O�p�`���_
	DirectX::XMFLOAT3 v[3] = {
		{ -2, 2, 0 },
		{ 0, 1, 2 },
		{ 2, 2, 0 },
	};
	// ���ƎO�p�`�̏Փˏ���
	DirectX::XMFLOAT4 c = { 0, 1, 0, 1 };
	DirectX::XMFLOAT3 position, normal;
	if (SphereIntersectTriangle(obj.position, obj.radius, v[0], v[1], v[2], position, normal))
	{
		obj.position = position;
		c = { 1, 0, 0, 1 };
	}

	// �����_�[�X�e�[�g�ݒ�
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	// �O�p�`�|���S���`��
	primitiveRenderer->AddVertex(v[0], c);
	primitiveRenderer->AddVertex(v[1], c);
	primitiveRenderer->AddVertex(v[2], c);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// �O�p�`�G�b�W�`��
	primitiveRenderer->AddVertex(v[0], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[1], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[2], { 0, 0, 1, 1 });
	primitiveRenderer->AddVertex(v[0], { 0, 0, 1, 1 });
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	// �O���b�h�`��
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// �V�F�C�v�`��
	shapeRenderer->DrawSphere(obj.position, obj.radius, obj.color);
	shapeRenderer->Render(dc, camera.GetView(), camera.GetProjection());
}

// GUI�`�揈��
void SphereVsTriangleCollisionScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once);

	if (ImGui::Begin(u8"���ƎO�p�`�̏Փˏ���"))
	{
		ImGui::Text(u8"�O�㍶�E�ړ�����F�����L�[");
		ImGui::Text(u8"�㉺�ړ�����FZX");
	}
	ImGui::End();
}

// ���ƎO�p�`�Ƃ̌����𔻒肷��
bool SphereVsTriangleCollisionScene::SphereIntersectTriangle(
	const DirectX::XMFLOAT3& sphereCenter,
	const float sphereRadius,
	const DirectX::XMFLOAT3& triangleVertexA,
	const DirectX::XMFLOAT3& triangleVertexB,
	const DirectX::XMFLOAT3& triangleVertexC,
	DirectX::XMFLOAT3& hitPosition,
	DirectX::XMFLOAT3& hitNormal)
{
	// TODO�@:�����O�p�`�ɉ����o�����Փˏ�������������
	{
		// �O�p�`���_
		DirectX::XMVECTOR TriangleVertex[3] =
		{
			DirectX::XMLoadFloat3(&triangleVertexA),
			DirectX::XMLoadFloat3(&triangleVertexB),
			DirectX::XMLoadFloat3(&triangleVertexC),
		};

		// �ʖ@�������߂�
		DirectX::XMVECTOR Edge[3];
		Edge[0] = DirectX::XMVectorSubtract(TriangleVertex[1], TriangleVertex[0]);
		Edge[1] = DirectX::XMVectorSubtract(TriangleVertex[2], TriangleVertex[1]);

		DirectX::XMVECTOR TriangleNormal = DirectX::XMVector3Cross(Edge[0], Edge[1]);
		TriangleNormal = DirectX::XMVector3Normalize(TriangleNormal);

		// ���ʂƋ��̋��������߂�
		DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&sphereCenter);
		DirectX::XMVECTOR Dot1 = DirectX::XMVector3Dot(TriangleNormal, SphereCenter);
		DirectX::XMVECTOR Dot2 = DirectX::XMVector3Dot(TriangleNormal, TriangleVertex[0]);
		DirectX::XMVECTOR Distance = DirectX::XMVectorSubtract(Dot1, Dot2);
		float distance = DirectX::XMVectorGetX(Distance);

		// ���̒��S���ʂɐڂ��Ă��邩�A���̕����ɂ���ꍇ�͓�����Ȃ�
		if (distance < 0.0f)
		{
			return false;
		}
		// ���a�ȏ㗣��Ă���Γ�����Ȃ�
		if (distance > sphereRadius)
		{
			return false;
		}

		// �����O�p�`�����ɑ��݂��邩
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
		// �O�p�̓����Ȃ̂Ō�������
		if (!outside)
		{
			// ���˂͖ʂ̋t������(���a-����)�̒���
			float depth = sphereRadius - distance;
			// �߂荞�ݕ������o������
			DirectX::XMVECTOR Reflection = DirectX::XMVectorScale(TriangleNormal, depth);
			SphereCenter = DirectX::XMVectorAdd(SphereCenter, Reflection);

			DirectX::XMStoreFloat3(&hitPosition, SphereCenter);
			DirectX::XMStoreFloat3(&hitNormal, TriangleNormal);

			return true;
		}

		// �Z�O�����g�Ƃ̔�r
		float radiusSq = sphereRadius * sphereRadius;
		for (int i = 0; i < 3; ++i)
		{
			// �ӂ̎ˉe�l�����߂�
			float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec[i], Edge[i]));
			if (t > 0.0f)
			{
				// �ӂ̎n�_����I�_�܂ł̃x�N�g���Ǝn�_���狅�܂ł̃x�N�g��������̏ꍇ�A
				// ���ϒl���ӂ̒����̂Q��ɂȂ鐫���𗘗p���ĕӂ��狅�܂ł̍ŒZ�x�N�g�������߂�
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
			// �ӂ��狅�܂ł̍ŒZ�x�N�g���̋��������a�ȉ��Ȃ�߂荞��ł���
			float lengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vec[i]));
			if (lengthSq <= radiusSq)
			{
				// �߂荞�ݗʎZ�o
				float depth = sphereRadius - sqrtf(lengthSq);

				// �߂荞�ݕ������o������
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
