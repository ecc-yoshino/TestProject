#include <imgui.h>
#include "Graphics.h"
#include "Scene/RootMotionScene.h"

// �R���X�g���N�^
RootMotionScene::RootMotionScene()
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
		{ 0, 10, -10 },		// ���_
		{ 0, 0, 0 },		// �����_
		{ 0, 1, 0 }			// ��x�N�g��
	);

	// ���f���ǂݍ���
	character = std::make_shared<Model>(device, "Data/Model/RPG-Character/RPG-Character.glb");
	character->GetNodePoses(nodePoses);
}

// �X�V����
void RootMotionScene::Update(float elapsedTime)
{
	// �A�j���[�V�����؂�ւ�����
	if (animationLoop)
	{
		// ���s
		int newAnimationIndex = animationIndex;
		if (GetAsyncKeyState(VK_UP) & 0x8000)
		{
			newAnimationIndex = character->GetAnimationIndex("Walk_F");
		}
		else if (GetAsyncKeyState(VK_DOWN) & 0x8000)
		{
			newAnimationIndex = character->GetAnimationIndex("Walk_B");
		}
		else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		{
			newAnimationIndex = character->GetAnimationIndex("Walk_R");
		}
		else if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		{
			newAnimationIndex = character->GetAnimationIndex("Walk_L");
		}
		else
		{
			newAnimationIndex = character->GetAnimationIndex("Idle");
		}
		if (animationIndex != newAnimationIndex)
		{
			animationIndex = newAnimationIndex;
			animationSeconds = oldAnimationSeconds = 0;
		}
	}
	else
	{
		// ���[�����O
		if (GetAsyncKeyState(VK_UP) & 0x01)
		{
			animationIndex = character->GetAnimationIndex("Evade_F");
			animationSeconds = oldAnimationSeconds = 0;
		}
		if (GetAsyncKeyState(VK_DOWN) & 0x01)
		{
			animationIndex = character->GetAnimationIndex("Evade_B");
			animationSeconds = oldAnimationSeconds = 0;
		}
		if (GetAsyncKeyState(VK_RIGHT) & 0x01)
		{
			animationIndex = character->GetAnimationIndex("Evade_R");
			animationSeconds = oldAnimationSeconds = 0;
		}
		if (GetAsyncKeyState(VK_LEFT) & 0x01)
		{
			animationIndex = character->GetAnimationIndex("Evade_L");
			animationSeconds = oldAnimationSeconds = 0;
		}
	}

	// �A�j���[�V�����X�V����
	if (animationIndex >= 0)
	{
		const Model::Animation& animation = character->GetAnimations().at(animationIndex);

		// �w�莞�Ԃ̃A�j���[�V�����̎p�����擾
		character->ComputeAnimation(animationIndex, animationSeconds, nodePoses);

		// TODO�@:���[�g���[�V�����������s���A�L�����N�^�[���ړ�����
		{
			// ���[�g���[�V�����m�[�h�ԍ��擾
			const int rootMotionNodeIndex = character->GetNodeIndex("B_Pelvis");

			// ����A�O��A����̃��[�g���[�V�����m�[�h�̎p�����擾
			Model::NodePose beginPose, oldPose, newPose;
			character->ComputeAnimation(animationIndex, rootMotionNodeIndex, 0, beginPose);
			character->ComputeAnimation(animationIndex, rootMotionNodeIndex, oldAnimationSeconds, oldPose);
			character->ComputeAnimation(animationIndex, rootMotionNodeIndex, animationSeconds, newPose);

			// TODO�A:���[�v�A�j���[�V�����ɑΉ�����
			DirectX::XMFLOAT3 localTranslation;
			if (oldAnimationSeconds > animationSeconds)
			{
				// ���[�v������
				Model::NodePose endPose;
				character->ComputeAnimation(animationIndex, rootMotionNodeIndex, animation.secondsLength, endPose);

				// ���[�J���ړ��l���Z�o
				localTranslation.x = (endPose.position.x - oldPose.position.x) + (newPose.position.x - beginPose.position.x);
				localTranslation.y = (endPose.position.y - oldPose.position.y) + (newPose.position.y - beginPose.position.y);
				localTranslation.z = (endPose.position.z - oldPose.position.z) + (newPose.position.z - beginPose.position.z);
			}
			else
			{
				// ���[�J���ړ��l���Z�o
				localTranslation.x = newPose.position.x - oldPose.position.x;
				localTranslation.y = newPose.position.y - oldPose.position.y;
				localTranslation.z = newPose.position.z - oldPose.position.z;
			}

			// �O���[�o���ړ��l���Z�o
			Model::Node& rootMotionNode = character->GetNodes().at(rootMotionNodeIndex);
			DirectX::XMVECTOR LocalTranslation = DirectX::XMLoadFloat3(&localTranslation);
			DirectX::XMMATRIX ParentGlobalTransform = DirectX::XMLoadFloat4x4(&rootMotionNode.parent->globalTransform);
			DirectX::XMVECTOR GlobalTranslation = DirectX::XMVector3TransformNormal(LocalTranslation, ParentGlobalTransform);

			// TODO�B:���[�g���[�V������Y�ړ��͓K�p���Ȃ��悤�ɂ���
			if (bakeTranslationY)
			{
				// Y�����̈ړ��l�𔲂�
				GlobalTranslation = DirectX::XMVectorSetY(GlobalTranslation, 0);

				// ����̎p���̃O���[�o���ʒu���Z�o
				DirectX::XMVECTOR LocalPosition = DirectX::XMLoadFloat3(&newPose.position);
				DirectX::XMVECTOR GlobalPosition = DirectX::XMVector3Transform(LocalPosition, ParentGlobalTransform);
				// XZ�������폜
				GlobalPosition = DirectX::XMVectorSetX(GlobalPosition, 0);
				GlobalPosition = DirectX::XMVectorSetZ(GlobalPosition, 0);
				// ���[�J����ԕϊ�
				DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
				LocalPosition = DirectX::XMVector3Transform(GlobalPosition, InverseParentGlobalTransform);
				// ���[�g���[�V�����m�[�h�̈ʒu��ݒ�
				DirectX::XMStoreFloat3(&nodePoses[rootMotionNodeIndex].position, LocalPosition);
			}
			else
			{
				// ���[�g���[�V�����m�[�h������̎p���ɂ���
				nodePoses[rootMotionNodeIndex].position = beginPose.position;
			}

			// ���[���h�ړ��l���Z�o
			DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&worldTransform);
			DirectX::XMVECTOR WorldTranslation = DirectX::XMVector3TransformNormal(GlobalTranslation, WorldTransform);
			DirectX::XMFLOAT3 worldTranslation;
			DirectX::XMStoreFloat3(&worldTranslation, WorldTranslation);

			// �ʒu���X�V
			position.x += worldTranslation.x;
			position.y += worldTranslation.y;
			position.z += worldTranslation.z;
		}

		// �A�j���[�V�������ԍX�V
		oldAnimationSeconds = animationSeconds;
		animationSeconds += elapsedTime;
		if (animationSeconds > animation.secondsLength)
		{
			if (animationLoop)
			{
				animationSeconds -= animation.secondsLength;
			}
			else
			{
				animationSeconds = animation.secondsLength;
			}
		}

		// �p���X�V
		character->SetNodePoses(nodePoses);
	}

	// ���[���h�g�����X�t�H�[���X�V
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMMATRIX WorldTransform = S * R * T;
	DirectX::XMStoreFloat4x4(&worldTransform, WorldTransform);

	// ���f���g�����X�t�H�[���X�V����
	character->UpdateTransform(worldTransform);
}

// �`�揈��
void RootMotionScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ModelRenderer* modelRenderer = Graphics::Instance().GetModelRenderer();

	// �����_�[�X�e�[�g�ݒ�
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// �O���b�h�`��
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// �`��R���e�L�X�g�ݒ�
	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.camera = &camera;
	rc.lightManager = &lightManager;

	// ���f���`��
	modelRenderer->Draw(ShaderId::Basic, character);
	modelRenderer->Render(rc);

}

// GUI�`�揈��
void RootMotionScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();

	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Once);

	if (ImGui::Begin(u8"���[�g���[�V��������"))
	{
		ImGui::Text(u8"����F�����L�[");

		ImGui::Checkbox(u8"���[�v", &animationLoop);
		ImGui::Checkbox(u8"Y���ړ�����", &bakeTranslationY);

		ImGui::DragFloat3("Position", &position.x, 0.1f);

		DirectX::XMFLOAT3 degree =
		{
			DirectX::XMConvertToDegrees(angle.x),
			DirectX::XMConvertToDegrees(angle.y),
			DirectX::XMConvertToDegrees(angle.z),
		};
		if (ImGui::DragFloat3("Angle", &degree.x, 1.0f))
		{
			angle.x = DirectX::XMConvertToRadians(degree.x);
			angle.y = DirectX::XMConvertToRadians(degree.y);
			angle.z = DirectX::XMConvertToRadians(degree.z);
		}
	}
	ImGui::End();
}
