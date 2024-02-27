#include <algorithm>
#include <functional>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXCollision.h>
#include "Graphics.h"
#include "TransformUtils.h"
#include "Scene/CharacterControlScene.h"

// �R���X�g���N�^
CharacterControlScene::CharacterControlScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();

	// �J�����Z�b�g�A�b�v
	SetupCamera();

	// �X�e�[�W�Z�b�g�A�b�v
	SetupStage(device);

	// �{�[���Z�b�g�A�b�v
	SetupBalls(device);

	// ���j�e�B�����Z�b�g�A�b�v
	SetupUnityChan(device);

}

// �X�V����
void CharacterControlScene::Update(float elapsedTime)
{
	elapsedTime *= timeScale;

	// ���͍X�V����
	UpdateInput();

	// �J�����X�V����
	if (useFreeCamera)
	{
		cameraController.Update();
		cameraController.SyncControllerToCamera(camera);
	}
	else
	{
		UpdateThirdPersonCamera(elapsedTime);
	}

	// �{�[���X�V����
	UpdateBalls(elapsedTime);

	// ���j�e�B�����X�V����
	UpdateUnityChan(elapsedTime);
}

// �`�揈��
void CharacterControlScene::Render(float elapsedTime)
{
	elapsedTime *= timeScale;

	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();
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
	rc.camera = &camera;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.lightManager = &lightManager;

	// ���f���`��
	for (Ball& ball : balls)
	{
		modelRenderer->Draw(ShaderId::Lambert, ball.model);
	}
	modelRenderer->Draw(ShaderId::Lambert, stage.model);
	modelRenderer->Draw(ShaderId::Basic, unitychan.model);
	modelRenderer->Draw(ShaderId::Basic, unitychan.staff);
	modelRenderer->Render(rc);

	// �����_�[�X�e�[�g�ݒ�
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// �G�t�F�N�g�`��
	DirectX::XMFLOAT4 color = { 0.6f, 1.0f, 1.0f, 0.7f };
	const int division = static_cast<int>(10 / elapsedFrame);
	int polygonCount = static_cast<int>(8 / elapsedFrame) - 3;
	if (polygonCount > _countof(unitychan.trailPositions[0]) - 1)
	{
		polygonCount = _countof(unitychan.trailPositions[0]) - 1;
	}
	float subtract = color.w / (polygonCount * (division - 1));
	for (int i = 0; i < polygonCount; ++i)
	{
		DirectX::XMVECTOR Root0 = DirectX::XMLoadFloat3(&unitychan.trailPositions[0][i + 0]);
		DirectX::XMVECTOR Root1 = DirectX::XMLoadFloat3(&unitychan.trailPositions[0][i + 1]);
		DirectX::XMVECTOR Root2 = DirectX::XMLoadFloat3(&unitychan.trailPositions[0][i + 2]);
		DirectX::XMVECTOR Root3 = DirectX::XMLoadFloat3(&unitychan.trailPositions[0][i + 3]);
		DirectX::XMVECTOR Tip0 = DirectX::XMLoadFloat3(&unitychan.trailPositions[1][i + 0]);
		DirectX::XMVECTOR Tip1 = DirectX::XMLoadFloat3(&unitychan.trailPositions[1][i + 1]);
		DirectX::XMVECTOR Tip2 = DirectX::XMLoadFloat3(&unitychan.trailPositions[1][i + 2]);
		DirectX::XMVECTOR Tip3 = DirectX::XMLoadFloat3(&unitychan.trailPositions[1][i + 3]);
		for (int j = 1; j < division; ++j)
		{
			float t = j / static_cast<float>(division);

			DirectX::XMVECTOR Root = DirectX::XMVectorCatmullRom(Root0, Root1, Root2, Root3, t);
			DirectX::XMVECTOR Tip = DirectX::XMVectorCatmullRom(Tip0, Tip1, Tip2, Tip3, t);
			DirectX::XMFLOAT3 tail, head;
			DirectX::XMStoreFloat3(&tail, Root);
			DirectX::XMStoreFloat3(&head, Tip);
			primitiveRenderer->AddVertex(tail, color);
			primitiveRenderer->AddVertex(head, color);
			color.w -= subtract;
		}
	}
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// �����_�[�X�e�[�g�ݒ�
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// �����{�[���`��
	{
		auto drawPhysicsBones = [&](const std::vector<PhysicsBone>& bones)
		{
			for (size_t i = 0; i < bones.size() - 1; ++i)
			{
				const PhysicsBone& bone = bones[i];
				const PhysicsBone& child = bones[i + 1];

				DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&bone.worldTransform);

				// ���`��
				float length = child.localPosition.x;
				World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[0]), length);
				World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[1]), length);
				World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[2]), length);
				DirectX::XMFLOAT4X4 world;
				DirectX::XMStoreFloat4x4(&world, World);
				primitiveRenderer->DrawAxis(world, { 1, 1, 0, 1 });

				// �{�[���`��
				World = DirectX::XMLoadFloat4x4(&bone.worldTransform);
				DirectX::XMMATRIX Offset = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(90));
				World = DirectX::XMMatrixMultiply(Offset, World);
				DirectX::XMStoreFloat4x4(&world, World);
				shapeRenderer->DrawBone(world, length, { 1, 1, 0, 1 });

				DirectX::XMMATRIX ChildWorld = DirectX::XMLoadFloat4x4(&child.worldTransform);
				// �R���W�����`��
				DirectX::XMFLOAT3 position;
				DirectX::XMStoreFloat3(&position, ChildWorld.r[3]);
				shapeRenderer->DrawSphere(position, child.collisionRadius, { 0, 1, 0, 1 });
			}
		};
		if (unitychan.visibleLeftHairTailBones)
		{
			drawPhysicsBones(unitychan.leftHairTailBones);
		}
		if (unitychan.visibleRightHairTailBones)
		{
			drawPhysicsBones(unitychan.rightHairTailBones);
		}
		if (unitychan.visibleLeftSkirtFrontBones)
		{
			drawPhysicsBones(unitychan.leftSkirtFrontBones);
		}
		if (unitychan.visibleLeftSkirtBackBones)
		{
			drawPhysicsBones(unitychan.leftSkirtBackBones);
		}
		if (unitychan.visibleRightSkirtFrontBones)
		{
			drawPhysicsBones(unitychan.rightSkirtFrontBones);
		}
		if (unitychan.visibleRightSkirtBackBones)
		{
			drawPhysicsBones(unitychan.rightSkirtBackBones);
		}
	}

	// �R���W�����`��
	{
		auto drawCollisionBones = [&](const std::vector<CollisionBone>& bones)
		{
			for (const CollisionBone& bone : bones)
			{
				DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);

				// ���`��
				World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[0]), bone.radius);
				World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[1]), bone.radius);
				World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[2]), bone.radius);
				DirectX::XMFLOAT4X4 world;
				DirectX::XMStoreFloat4x4(&world, World);
				primitiveRenderer->DrawAxis(world, { 1, 1, 0, 1 });
				// ���`��
				DirectX::XMVECTOR Offset = DirectX::XMLoadFloat3(&bone.offset);
				DirectX::XMVECTOR Position = DirectX::XMVector3Transform(Offset, World);
				DirectX::XMFLOAT3 position;
				DirectX::XMStoreFloat3(&position, Position);
				shapeRenderer->DrawSphere(position, bone.radius, { 0, 0, 1, 1 });
			}
		};
		if (unitychan.visibleUpperBodyCollisionBones)
		{
			drawCollisionBones(unitychan.upperBodyCollisionBones);
		}
		if (unitychan.visibleLeftLegCollisionBones)
		{
			drawCollisionBones(unitychan.leftLegCollisionBones);
		}
		if (unitychan.visibleRightLegCollisionBones)
		{
			drawCollisionBones(unitychan.rightLegCollisionBones);
		}
		if (unitychan.visibleCharacterCollision)
		{
			DirectX::XMFLOAT3 position = unitychan.position;
			position.y += unitychan.radius;
			shapeRenderer->DrawSphere(position, unitychan.radius, { 0, 1, 1, 1 });
		}
	}

	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	shapeRenderer->Render(dc, camera.GetView(), camera.GetProjection());
}

// GUI�`�揈��
void CharacterControlScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 700), ImGuiCond_Once);

	if (ImGui::Begin(u8"�L�����N�^�[����"))
	{
		if (ImGui::CollapsingHeader(u8"�O���[�o��", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("TimeScale", &timeScale, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Gravity", &gravity, 0.01f, 0.0f);
			ImGui::DragFloat3("FieldForce", &fieldForce.x, 0.01f);
			if (ImGui::Checkbox("UseFreeCamera", &useFreeCamera))
			{
				if (useFreeCamera)
				{
					cameraController.SyncCameraToController(camera);
				}
			}
		}
		if (ImGui::CollapsingHeader(u8"�J����", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(&thirdPersonCamera);

			DirectX::XMFLOAT3 angle =
			{
				DirectX::XMConvertToDegrees(thirdPersonCamera.angle.x),
				DirectX::XMConvertToDegrees(thirdPersonCamera.angle.y),
				DirectX::XMConvertToDegrees(thirdPersonCamera.angle.z),
			};

			DirectX::XMFLOAT3 eye = camera.GetEye();
			DirectX::XMFLOAT3 focus = camera.GetFocus();
			ImGui::DragFloat3("Eye", &eye.x);
			ImGui::DragFloat3("Focus", &focus.x);
			if (ImGui::DragFloat3("Angle", &angle.x, 1.0f))
			{
				thirdPersonCamera.angle.x = DirectX::XMConvertToRadians(angle.x);
				thirdPersonCamera.angle.y = DirectX::XMConvertToRadians(angle.y);
				thirdPersonCamera.angle.z = DirectX::XMConvertToRadians(angle.z);
			}
			ImGui::DragFloat("MoveSpeed", &thirdPersonCamera.moveSpeed);
			ImGui::DragFloat("RotateSpeed", &thirdPersonCamera.rotateSpeed);
			ImGui::DragFloat("FocusRange", &thirdPersonCamera.focusRange);

			float minPitch = DirectX::XMConvertToDegrees(thirdPersonCamera.minPitch);
			if (ImGui::DragFloat("MinPitch", &minPitch))
			{
				thirdPersonCamera.minPitch = minPitch;
			}
			float maxPitch = DirectX::XMConvertToDegrees(thirdPersonCamera.maxPitch);
			if (ImGui::DragFloat("MaxPitch", &maxPitch))
			{
				thirdPersonCamera.maxPitch = maxPitch;
			}

			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader(u8"���j�e�B�����", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(&unitychan);

			ImGui::Separator();
			ImGui::Text(u8"��{");
			ImGui::Separator();
			int state = static_cast<int>(unitychan.state);
			ImGui::DragFloat3("Position", &unitychan.position.x);

			DirectX::XMFLOAT3 angle =
			{
				DirectX::XMConvertToDegrees(unitychan.angle.x),
				DirectX::XMConvertToDegrees(unitychan.angle.y),
				DirectX::XMConvertToDegrees(unitychan.angle.z),
			};
			if (ImGui::DragFloat3("Angle", &angle.x, 1.0f))
			{
				unitychan.angle.x = DirectX::XMConvertToRadians(angle.x);
				unitychan.angle.y = DirectX::XMConvertToRadians(angle.y);
				unitychan.angle.z = DirectX::XMConvertToRadians(angle.z);
			}
			ImGui::DragFloat("Radius", &unitychan.radius, 0.01f, 0.0f);
			ImGui::Checkbox("VisibleCollision", &unitychan.visibleCharacterCollision);

			ImGui::Separator();
			ImGui::Text(u8"�ړ��֘A");
			ImGui::Separator();
			ImGui::DragFloat("JumpSpeed", &unitychan.jumpSpeed, 0.01f, 0.0f);
			ImGui::DragFloat("MoveSpeed", &unitychan.moveSpeed, 0.01f, 0.0f);
			ImGui::DragFloat("TurnSpeed", &unitychan.turnSpeed, 0.01f, 0.0f);
			ImGui::DragFloat("Friction", &unitychan.friction, 0.01f, 0.0f);
			ImGui::DragFloat("Acceleration", &unitychan.acceleration, 0.01f, 0.0f);
			ImGui::DragFloat("GroundAdjust", &unitychan.groundAdjust, 0.01f, 0.0f);
			ImGui::DragFloat("SlopeLimit", &unitychan.slopeLimit, 1.0f, 0, 90);
			ImGui::DragFloat("MaxPhysicsBoneVelocity", &unitychan.maxPhysicsBoneVelocity, 0.01f, 0.0f, 3.0f);

			ImGui::Separator();
			ImGui::Text(u8"���j�^�����O");
			ImGui::Separator();
			ImGui::DragInt("State", &state);
			ImGui::DragFloat3("Velocity", &unitychan.velocity.x);
			ImGui::DragFloat3("DeltaMove", &unitychan.deltaMove.x);
			float slope = DirectX::XMConvertToDegrees(acosf(unitychan.groundNormal.y));
			ImGui::InputFloat("SlopeAngle", &slope);
			ImGui::DragFloat3("GroundNormal", &unitychan.groundNormal.x);
			ImGui::Checkbox("OnGround", &unitychan.onGround);

			ImGui::Separator();
			bool visiblePhysicsBone	= unitychan.visibleLeftHairTailBones
									| unitychan.visibleRightHairTailBones
									| unitychan.visibleLeftSkirtFrontBones
									| unitychan.visibleLeftSkirtBackBones
									| unitychan.visibleRightSkirtFrontBones
									| unitychan.visibleRightSkirtBackBones;
			if (ImGui::Checkbox(u8"�����{�[��", &visiblePhysicsBone))
			{
				unitychan.visibleLeftHairTailBones = visiblePhysicsBone;
				unitychan.visibleRightHairTailBones = visiblePhysicsBone;
				unitychan.visibleLeftSkirtFrontBones = visiblePhysicsBone;
				unitychan.visibleLeftSkirtBackBones = visiblePhysicsBone;
				unitychan.visibleRightSkirtFrontBones = visiblePhysicsBone;
				unitychan.visibleRightSkirtBackBones = visiblePhysicsBone;
			}
			ImGui::Separator();
			{
				auto drawPhysicsBoneInfo = [&](const char* name, std::vector<PhysicsBone>& bones, bool& visible)
				{
					ImGui::Checkbox(name, &visible);
					if (visible)
					{
						// �\�̕`��
						ImGui::Columns(2, "columns");
						ImGui::SetColumnWidth(0, 150);
						ImGui::SetColumnWidth(1, 60);
						ImGui::Separator();

						ImGui::Text("Name"); ImGui::NextColumn();
						ImGui::Text("Radius"); ImGui::NextColumn();
						ImGui::Separator();

						for (PhysicsBone& bone : bones)
						{
							ImGui::PushID(&bone);
							ImGui::Text(bone.node->name.c_str()); ImGui::NextColumn();
							ImGui::DragFloat("", &bone.collisionRadius, 0.01f, 0, 1.0f); ImGui::NextColumn();
							ImGui::PopID();
						}
						ImGui::Columns(1);
					}
				};
				drawPhysicsBoneInfo("LeftHairTail", unitychan.leftHairTailBones, unitychan.visibleLeftHairTailBones);
				drawPhysicsBoneInfo("RightHairTail", unitychan.rightHairTailBones, unitychan.visibleRightHairTailBones);
				drawPhysicsBoneInfo("LeftSkirtFront", unitychan.leftSkirtFrontBones, unitychan.visibleLeftSkirtFrontBones);
				drawPhysicsBoneInfo("LeftSkirtBack", unitychan.leftSkirtBackBones, unitychan.visibleLeftSkirtBackBones);
				drawPhysicsBoneInfo("RightSkirtFront", unitychan.rightSkirtFrontBones, unitychan.visibleRightSkirtFrontBones);
				drawPhysicsBoneInfo("RightSkirtBack", unitychan.rightSkirtBackBones, unitychan.visibleRightSkirtBackBones);
			}

			ImGui::Separator();
			bool visibleCollisionBone	= unitychan.visibleUpperBodyCollisionBones
										| unitychan.visibleLeftLegCollisionBones
										| unitychan.visibleRightLegCollisionBones;
			if (ImGui::Checkbox(u8"�R���W�����{�[��", &visibleCollisionBone))
			{
				unitychan.visibleUpperBodyCollisionBones = visibleCollisionBone;
				unitychan.visibleLeftLegCollisionBones = visibleCollisionBone;
				unitychan.visibleRightLegCollisionBones = visibleCollisionBone;
			}
			ImGui::Separator();
			{
				auto drawCollisionBoneInfo = [&](const char* name, std::vector<CollisionBone>& bones, bool& visible)
				{
					ImGui::Checkbox(name, &visible);

					if (visible)
					{
						// �\�̕`��
						ImGui::Columns(3, "columns");
						ImGui::SetColumnWidth(0, 130);
						ImGui::SetColumnWidth(1, 60);
						ImGui::SetColumnWidth(2, 200);
						ImGui::Separator();

						ImGui::Text("Name"); ImGui::NextColumn();
						ImGui::Text("Radius"); ImGui::NextColumn();
						ImGui::Text("Offset"); ImGui::NextColumn();
						ImGui::Separator();

						for (CollisionBone& bone : bones)
						{
							ImGui::Text(bone.node->name.c_str()); ImGui::NextColumn();
							ImGui::PushID(&bone.radius);
							ImGui::DragFloat("", &bone.radius, 0.01f, 0, 1.0f); ImGui::NextColumn();
							ImGui::PopID();
							ImGui::PushID(&bone.offset);
							ImGui::DragFloat3("", &bone.offset.x, 0.01f); ImGui::NextColumn();
							ImGui::PopID();
						}
						ImGui::Columns(1);
					}
				};
				drawCollisionBoneInfo("UpperBody", unitychan.upperBodyCollisionBones, unitychan.visibleUpperBodyCollisionBones);
				drawCollisionBoneInfo("LeftLeg", unitychan.leftLegCollisionBones, unitychan.visibleLeftLegCollisionBones);
				drawCollisionBoneInfo("RightLeg", unitychan.rightLegCollisionBones, unitychan.visibleRightLegCollisionBones);
			}

			ImGui::Separator();
			ImGui::Text(u8"�m�[�h�c���[");
			ImGui::Separator();
			{
				// �m�[�h�c���[���ċA�I�ɕ`�悷��֐�
				std::function<void(Model::Node*)> drawNodeTree = [&](Model::Node* node)
				{
					// �����N���b�N�A�܂��̓m�[�h���_�u���N���b�N�ŊK�w���J��
					ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
						| ImGuiTreeNodeFlags_OpenOnDoubleClick;

					// �q�����Ȃ��ꍇ�͖������Ȃ�
					size_t childCount = node->children.size();
					if (childCount == 0)
					{
						nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					}

					// �I���t���O
					if (selectionNode == node)
					{
						nodeFlags |= ImGuiTreeNodeFlags_Selected;
					}

					// �c���[�m�[�h��\��
					bool opened = ImGui::TreeNodeEx(node, nodeFlags, node->name.c_str());

					// �t�H�[�J�X���ꂽ�m�[�h��I������
					if (ImGui::IsItemFocused())
					{
						selectionNode = node;
					}

					if (selectionNode == node)
					{
						// �ʒu
						ImGui::DragFloat3("Position", &selectionNode->position.x, 0.1f);

						// ��]
						DirectX::XMFLOAT3 angle;
						TransformUtils::QuaternionToRollPitchYaw(selectionNode->rotation, angle.x, angle.y, angle.z);
						angle.x = DirectX::XMConvertToDegrees(angle.x);
						angle.y = DirectX::XMConvertToDegrees(angle.y);
						angle.z = DirectX::XMConvertToDegrees(angle.z);
						if (ImGui::DragFloat3("Rotation", &angle.x, 1.0f))
						{
							angle.x = DirectX::XMConvertToRadians(angle.x);
							angle.y = DirectX::XMConvertToRadians(angle.y);
							angle.z = DirectX::XMConvertToRadians(angle.z);
							DirectX::XMVECTOR Rotation = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
							DirectX::XMStoreFloat4(&selectionNode->rotation, Rotation);
						}

						// �X�P�[��
						ImGui::DragFloat3("Scale", &selectionNode->scale.x, 0.01f);
					}

					// �J����Ă���ꍇ�A�q�K�w�������������s��
					if (opened && childCount > 0)
					{
						for (Model::Node* child : node->children)
						{
							drawNodeTree(child);
						}
						ImGui::TreePop();
					}
				};
				// �ċA�I�Ƀm�[�h��`��
				if (unitychan.model != nullptr)
				{
					drawNodeTree(unitychan.model->GetRootNode());
				}
			}
			ImGui::PopID();
		}
		if (ImGui::CollapsingHeader(u8"�{�[��", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Columns(2, "columns");
			ImGui::SetColumnWidth(0, 50);
			ImGui::SetColumnWidth(1, 200);
			ImGui::Separator();

			ImGui::Text("Index"); ImGui::NextColumn();
			ImGui::Text("Position"); ImGui::NextColumn();

			for (size_t i = 0; i < balls.size(); ++i)
			{
				Ball& ball = balls.at(i);

				ImGui::PushID(&ball);

				ImGui::Separator();
				ImGui::Text("%zd", i); ImGui::NextColumn();
				ImGui::DragFloat3("", &ball.position.x, 0.01f); ImGui::NextColumn();

				ImGui::PopID();
			}
			ImGui::Columns(1);
		}
	}
	ImGui::End();
}

// ���͍X�V����
void CharacterControlScene::UpdateInput()
{
	// ���X�e�B�b�N����
	{
		float axisX = 0, axisY = 0;
		if (GetAsyncKeyState('W') & 0x8000) axisY =  1.0f;
		if (GetAsyncKeyState('A') & 0x8000) axisX = -1.0f;
		if (GetAsyncKeyState('S') & 0x8000) axisY = -1.0f;
		if (GetAsyncKeyState('D') & 0x8000) axisX =  1.0f;
		if (GetAsyncKeyState(VK_UP)    & 0x8000) axisY =  1.0f;
		if (GetAsyncKeyState(VK_LEFT)  & 0x8000) axisX = -1.0f;
		if (GetAsyncKeyState(VK_DOWN)  & 0x8000) axisY = -1.0f;
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) axisX =  1.0f;
		float axisLength = sqrtf(axisX * axisX + axisY * axisY);
		if (axisLength > 1.0f)
		{
			axisX /= axisLength;
			axisY /= axisLength;
			axisLength = 1.0f;
		}
		unitychan.inputAxisX = axisX;
		unitychan.inputAxisY = axisY;
		unitychan.inputAxisLength = axisLength;
	}
	// �E�X�e�B�b�N
	{
		float axisX = 0, axisY = 0;
		if (GetAsyncKeyState('I') & 0x8000) axisY = 1.0f;
		if (GetAsyncKeyState('J') & 0x8000) axisX = -1.0f;
		if (GetAsyncKeyState('K') & 0x8000) axisY = -1.0f;
		if (GetAsyncKeyState('L') & 0x8000) axisX = 1.0f;
		float axisLength = sqrtf(axisX * axisX + axisY * axisY);
		if (axisLength > 1.0f)
		{
			axisX /= axisLength;
			axisY /= axisLength;
			axisLength = 1.0f;
		}
		thirdPersonCamera.inputAxisX = axisX;
		thirdPersonCamera.inputAxisY = axisY;
		thirdPersonCamera.inputAxisLength = axisLength;
	}

	// �L�[����
	{
		uint32_t inputKey = 0;
		if (GetAsyncKeyState(VK_SPACE) & 0x8000) inputKey |= UnityChan::KeyJump;
		if (GetAsyncKeyState('Z') & 0x8000) inputKey |= UnityChan::KeyAttack;

		unitychan.inputKeyOld = unitychan.inputKeyNew;
		unitychan.inputKeyNew = inputKey;
		unitychan.inputKeyDown = ~unitychan.inputKeyOld & unitychan.inputKeyNew;
		unitychan.inputKeyUp = ~unitychan.inputKeyNew & unitychan.inputKeyOld;
	}
}

// �J�����Z�b�g�A�b�v
void CharacterControlScene::SetupCamera()
{
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	thirdPersonCamera.eye = { 20, 2, 20 };
	thirdPersonCamera.focus = { 15, 1, 15 };
	thirdPersonCamera.angle.x = DirectX::XMConvertToRadians(20);

	// �J�����ݒ�
	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// ��p
		screenWidth / screenHeight,			// ��ʃA�X�y�N�g��
		0.1f,								// �j�A�N���b�v
		1000.0f								// �t�@�[�N���b�v
	);
	camera.SetLookAt(
		thirdPersonCamera.eye,		// ���_
		thirdPersonCamera.focus,	// �����_
		{ 0, 1, 0 }					// ��x�N�g��
	);
	cameraController.SyncCameraToController(camera);
}

// �O�l�̎��_�J�����X�V����
void CharacterControlScene::UpdateThirdPersonCamera(float elapsedTime)
{
	thirdPersonCamera.focus = unitychan.position;
	thirdPersonCamera.focus.y += 1.1f;

	// �����_
	DirectX::XMFLOAT3 cameraFocus = camera.GetFocus();
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&thirdPersonCamera.focus);
	DirectX::XMVECTOR CameraFocus = DirectX::XMLoadFloat3(&cameraFocus);
	float moveSpeed = thirdPersonCamera.moveSpeed * elapsedTime;
	CameraFocus = DirectX::XMVectorLerp(CameraFocus, Focus, std::clamp(moveSpeed, 0.0f, 1.0f));
	DirectX::XMStoreFloat3(&cameraFocus, CameraFocus);

	// Y���p�x�Z�o
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&thirdPersonCamera.eye);
	DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Focus, Eye);
	Vec = DirectX::XMVectorSetY(Vec, 0);
	Vec = DirectX::XMVector3Normalize(Vec);
	DirectX::XMFLOAT3 vec;
	DirectX::XMStoreFloat3(&vec, Vec);
	thirdPersonCamera.angle.y = atan2f(vec.x, vec.z);

	// ��]
	float rotateSpeed = thirdPersonCamera.rotateSpeed * elapsedTime;
	thirdPersonCamera.angle.x += thirdPersonCamera.inputAxisY * rotateSpeed;
	thirdPersonCamera.angle.y += thirdPersonCamera.inputAxisX * rotateSpeed;

	// �p�x����
	thirdPersonCamera.angle.x = std::clamp(thirdPersonCamera.angle.x, thirdPersonCamera.minPitch, thirdPersonCamera.maxPitch);

	// ���_
	DirectX::XMMATRIX Transform = DirectX::XMMatrixRotationRollPitchYaw(thirdPersonCamera.angle.x, thirdPersonCamera.angle.y, thirdPersonCamera.angle.z);
	DirectX::XMVECTOR Front = Transform.r[2];
	Eye = DirectX::XMVectorSubtract(Focus, DirectX::XMVectorScale(Front, thirdPersonCamera.focusRange));
	DirectX::XMStoreFloat3(&thirdPersonCamera.eye, Eye);

	DirectX::XMFLOAT3 cameraEye = camera.GetEye();
	DirectX::XMVECTOR CameraEye = DirectX::XMLoadFloat3(&cameraEye);
	CameraEye = DirectX::XMVectorLerp(CameraEye, Eye, 0.1f);
	DirectX::XMStoreFloat3(&cameraEye, CameraEye);

	// �R���W����
	HitResult hit;
	if (RayIntersectModel(cameraFocus, cameraEye, stage.model.get(), hit))
	{
		cameraEye = hit.position;
	}

	camera.SetLookAt(cameraEye, cameraFocus, { 0, 1, 0 });
}

// �X�e�[�W�Z�b�g�A�b�v
void CharacterControlScene::SetupStage(ID3D11Device* device)
{
	// ���f���ǂݍ���
	stage.model = std::make_shared<Model>(device, "Data/Model/Greybox/Greybox.glb", 1.0f);
}

// �{�[���Z�b�g�A�b�v
void CharacterControlScene::SetupBalls(ID3D11Device* device)
{
	struct BallParam
	{
		DirectX::XMFLOAT3	position;
	};
	const BallParam params[] =
	{
		{ { 12.0f, 2.0f, 18.0f } },
		{ {  8.0f, 4.0f, 18.0f } },
		{ { 10.0f, 5.0f, 14.0f } },
	};
	for (const BallParam& param : params)
	{
		Ball& ball = balls.emplace_back();
		ball.model = std::make_unique<Model>(device, "Data/Model/Shape/Sphere.glb", 0.3f);
		ball.position = param.position;
	}
}

// �{�[���s��X�V����
void CharacterControlScene::UpdateBalls(float elapsedTime)
{
	for (Ball& ball : balls)
	{
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(ball.scale, ball.scale, ball.scale);
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(ball.position.x, ball.position.y, ball.position.z);
		DirectX::XMFLOAT4X4 worldTransform;
		DirectX::XMStoreFloat4x4(&worldTransform, S * T);
		ball.model->UpdateTransform(worldTransform);
	}
}

// ���j�e�B�����Z�b�g�A�b�v
void CharacterControlScene::SetupUnityChan(ID3D11Device* device)
{
	// ���f���ǂݍ���
	unitychan.model = std::make_shared<Model>(device, "Data/Model/unitychan/unitychan.glb");
	unitychan.model->GetNodePoses(unitychan.nodePoses);
	unitychan.model->GetNodePoses(unitychan.cacheNodePoses);
	unitychan.rootMotionNodeIndex = unitychan.model->GetNodeIndex("Character1_Hips");
	unitychan.hipsNodeIndex = unitychan.model->GetNodeIndex("Character1_Hips");
	unitychan.position = { 15, 0.5f, 15 };
	PlayUnityChanAnimation("Idle", 0, true, 0);

	unitychan.staff = std::make_shared<Model>(device, "Data/Model/Weapon/Staff.glb");

	// �m�[�h����
	auto findNode = [this](const char* name) -> Model::Node*
	{
		for (Model::Node& node : unitychan.model->GetNodes())
		{
			if (node.name == name)
			{
				return &node;
			}
		}
		return nullptr;
	};

	// ���b�N�A�b�g�{�[��
	{
		int headNodeIndex = unitychan.model->GetNodeIndex("Character1_Head");
		Model::Node& headNode = unitychan.model->GetNodes().at(headNodeIndex);
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&headNode.worldTransform);
		DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
		DirectX::XMVECTOR RootForward = DirectX::XMVectorSet(0, 0, 1, 0);
		DirectX::XMVECTOR Forward = DirectX::XMVector3TransformNormal(RootForward, InverseWorldTransform);
		Forward = DirectX::XMVector3Normalize(Forward);
		DirectX::XMStoreFloat3(&unitychan.lookAtBone.forward, Forward);
		unitychan.lookAtBone.node = &headNode;
	}

	// �����{�[���Z�b�g�A�b�v
	struct PhysicsBoneParam
	{
		const char* name;
		float collisionRadius;
	};
	const PhysicsBoneParam leftHairTailBoneParams[] =
	{
		{ "J_L_HairTail_00", 0.08f },
		{ "J_L_HairTail_01", 0.08f },
		{ "J_L_HairTail_02", 0.08f },
		{ "J_L_HairTail_03", 0.08f },
		{ "J_L_HairTail_04", 0.08f },
		{ "J_L_HairTail_05", 0.08f },
		{ "J_L_HairTail_06", 0.08f },
	};
	const PhysicsBoneParam rightHairTailBoneParams[] =
	{
		{ "J_R_HairTail_00", 0.08f },
		{ "J_R_HairTail_01", 0.08f },
		{ "J_R_HairTail_02", 0.08f },
		{ "J_R_HairTail_03", 0.08f },
		{ "J_R_HairTail_04", 0.08f },
		{ "J_R_HairTail_05", 0.08f },
		{ "J_R_HairTail_06", 0.08f },
	};
	const PhysicsBoneParam leftSkirtFrontBoneParams[] =
	{
		{ "J_L_Skirt_00", 0.03f },
		{ "J_L_Skirt_01", 0.03f },
		{ "J_L_Skirt_02", 0.02f },
	};
	const PhysicsBoneParam leftSkirtBackBoneParams[] =
	{
		{ "J_L_SkirtBack_00", 0.02f },
		{ "J_L_SkirtBack_01", 0.02f },
		{ "J_L_SkirtBack_02", 0.02f },
	};
	const PhysicsBoneParam rightSkirtFrontBoneParams[] =
	{
		{ "J_R_Skirt_00", 0.03f },
		{ "J_R_Skirt_01", 0.03f },
		{ "J_R_Skirt_02", 0.02f },
	};
	const PhysicsBoneParam rightSkirtBackBoneParams[] =
	{
		{ "J_R_SkirtBack_00", 0.02f },
		{ "J_R_SkirtBack_01", 0.02f },
		{ "J_R_SkirtBack_02", 0.02f },
	};
	auto setupPhysicsBones = [&](std::vector<PhysicsBone>& bones, const PhysicsBoneParam boneParams[], int boneCount)
	{
		bones.resize(boneCount);
		for (int i = 0; i < boneCount; ++i)
		{
			const PhysicsBoneParam& param = boneParams[i];
			PhysicsBone& bone = bones.at(i);
			bone.node = findNode(param.name);
			bone.collisionRadius = param.collisionRadius;
			bone.localPosition = bone.node->position;
			bone.localRotation = bone.node->rotation;
			bone.defaultLocalRotation = bone.node->rotation;
			bone.defaultLocalTransform = bone.node->localTransform;
			bone.worldTransform = bone.node->worldTransform;
			bone.oldWorldPosition.x = bone.node->worldTransform._41;
			bone.oldWorldPosition.y = bone.node->worldTransform._42;
			bone.oldWorldPosition.z = bone.node->worldTransform._43;
		}
	};
	setupPhysicsBones(unitychan.leftHairTailBones, leftHairTailBoneParams, _countof(leftHairTailBoneParams));
	setupPhysicsBones(unitychan.rightHairTailBones, rightHairTailBoneParams, _countof(rightHairTailBoneParams));
	setupPhysicsBones(unitychan.leftSkirtFrontBones, leftSkirtFrontBoneParams, _countof(leftSkirtFrontBoneParams));
	setupPhysicsBones(unitychan.leftSkirtBackBones, leftSkirtBackBoneParams, _countof(leftSkirtBackBoneParams));
	setupPhysicsBones(unitychan.rightSkirtFrontBones, rightSkirtFrontBoneParams, _countof(rightSkirtFrontBoneParams));
	setupPhysicsBones(unitychan.rightSkirtBackBones, rightSkirtBackBoneParams, _countof(rightSkirtBackBoneParams));

	// �R���W�����{�[���Z�b�g�A�b�v
	struct CollisionBoneParam
	{
		const char* name;
		float				radius;
		DirectX::XMFLOAT3	offset;
	};
	const CollisionBoneParam upperBodyCollisionBoneParams[] =
	{
		{ "Character1_Hips",          0.10f, { 0.00f, 0.0f, 0.0f } },
		{ "Character1_Spine2",        0.10f, { 0.00f, 0.0f, 0.0f } },
		{ "Character1_Neck",          0.07f, { 0.00f, 0.0f, 0.0f } },
		{ "Character1_Head",          0.12f, {-0.10f, 0.0f, 0.0f } },
		{ "Character1_LeftShoulder",  0.06f, {-0.10f, 0.0f, 0.0f } },
		{ "Character1_LeftArm",       0.06f, {-0.17f, 0.0f, 0.0f } },
		{ "Character1_LeftForeArm",   0.06f, {-0.05f, 0.0f, 0.0f } },
		{ "Character1_LeftHand",      0.10f, {-0.05f, 0.0f, 0.0f } },
		{ "Character1_RightShoulder", 0.06f, {-0.10f, 0.0f, 0.0f } },
		{ "Character1_RightArm",      0.06f, {-0.17f, 0.0f, 0.0f } },
		{ "Character1_RightForeArm",  0.06f, {-0.05f, 0.0f, 0.0f } },
		{ "Character1_RightHand",     0.10f, {-0.05f, 0.0f, 0.0f } },
	};
	const CollisionBoneParam leftLegCollisionBoneParams[] =
	{
		{ "Character1_Hips",          0.10f, {  0.10f, 0.0f, 0.0f } },
		{ "Character1_Hips",          0.12f, {  0.15f, 0.0f, 0.0f } },
		{ "Character1_LeftUpLeg",     0.08f, { -0.08f, 0.0f, 0.0f } },
	};
	const CollisionBoneParam rightLegCollisionBoneParams[] =
	{
		{ "Character1_Hips",          0.10f, {  0.10f, 0.0f, 0.0f } },
		{ "Character1_Hips",          0.12f, {  0.15f, 0.0f, 0.0f } },
		{ "Character1_RightUpLeg",    0.08f, { -0.08f, 0.0f, 0.0f } },
	};
	auto setupCollisionBones = [&](std::vector<CollisionBone>& bones, const CollisionBoneParam boneParams[], int boneCount)
	{
		bones.resize(boneCount);
		for (int i = 0; i < boneCount; ++i)
		{
			const CollisionBoneParam& param = boneParams[i];
			CollisionBone& bone = bones.at(i);
			bone.node = findNode(param.name);
			bone.offset = param.offset;
			bone.radius = param.radius;
		}
	};
	setupCollisionBones(unitychan.upperBodyCollisionBones, upperBodyCollisionBoneParams, _countof(upperBodyCollisionBoneParams));
	setupCollisionBones(unitychan.leftLegCollisionBones, leftLegCollisionBoneParams, _countof(leftLegCollisionBoneParams));
	setupCollisionBones(unitychan.rightLegCollisionBones, rightLegCollisionBoneParams, _countof(rightLegCollisionBoneParams));

	// IK�{�[���Z�b�g�A�b�v
	unitychan.leftFootIKBone.thighNode = findNode("Character1_LeftUpLeg");
	unitychan.leftFootIKBone.legNode = findNode("Character1_LeftLeg");
	unitychan.leftFootIKBone.footNode = findNode("Character1_LeftFoot");
	unitychan.leftFootIKBone.toeNode = findNode("Character1_LeftToeBase");
	unitychan.rightFootIKBone.thighNode = findNode("Character1_RightUpLeg");
	unitychan.rightFootIKBone.legNode = findNode("Character1_RightLeg");
	unitychan.rightFootIKBone.footNode = findNode("Character1_RightFoot");
	unitychan.rightFootIKBone.toeNode = findNode("Character1_RightToeBase");
}

// ���j�e�B�����X�V����
void CharacterControlScene::UpdateUnityChan(float elapsedTime)
{
	// �A�j���[�V�����X�V����
	UpdateUnityChanAnimation(elapsedTime);

	// �X�e�[�g�}�V���X�V����
	UpdateUnityChanStateMachine(elapsedTime);

	// �x���V�e�B�X�V����
	UpdateUnityChanVelocity(elapsedTime);

	// �ʒu�X�V����
	UpdateUnityChanPosition(elapsedTime);

	// �s��X�V����
	UpdateUnityChanTransform();

	// �G�t�F�N�g�X�V����
	UpdateUnityChanEffect();

	// ���b�N�A�b�g�X�V����
	UpdateUnityChanLookAt(elapsedTime);

	// IK�{�[���X�V����
	UpdateUnityChanIKBones();

	// �����{�[���X�V����
	UpdateUnityChanPhysicsBones(elapsedTime);
}

// ���j�e�B�����A�j���[�V�����X�V����
void CharacterControlScene::UpdateUnityChanAnimation(float elapsedTime)
{
	// �w�莞�Ԃ̃A�j���[�V����
	if (unitychan.animationIndex >= 0)
	{
		// �A�j���[�V�����؂�ւ����Ɍ��݂̎p�����L���b�V��
		if (unitychan.animationChange)
		{
			unitychan.animationChange = false;

			const std::vector<Model::Node>& nodes = unitychan.model->GetNodes();
			for (size_t i = 0; i < nodes.size(); ++i)
			{
				const Model::Node& src = nodes.at(i);
				Model::NodePose& dst = unitychan.cacheNodePoses.at(i);
				
				dst.position = src.position;
				dst.rotation = src.rotation;
				dst.scale = src.scale;
			}
		}

		// �A�j���[�V�����v�Z
		unitychan.model->ComputeAnimation(unitychan.animationIndex, unitychan.animationSeconds, unitychan.nodePoses);

		// ���[�g���[�V�����v�Z
		if (unitychan.computeRootMotion)
		{
			ComputeRootMotion(unitychan.model.get(), unitychan.rootMotionNodeIndex,
				unitychan.animationIndex, unitychan.oldAnimationSeconds, unitychan.animationSeconds,
				unitychan.rootMotionTranslation,
				unitychan.nodePoses.at(unitychan.rootMotionNodeIndex).position,
				false, true, false);
		}
		else
		{
			unitychan.rootMotionTranslation = { 0, 0, 0 };
		}

		// �A�j���[�V�������ԍX�V
		const Model::Animation& animation = unitychan.model->GetAnimations().at(unitychan.animationIndex);
		unitychan.oldAnimationSeconds = unitychan.animationSeconds;
		unitychan.animationSeconds += elapsedTime;
		if (unitychan.animationSeconds >= animation.secondsLength)
		{
			if (unitychan.animationLoop)
			{
				unitychan.animationSeconds -= animation.secondsLength;
			}
			else
			{
				unitychan.animationSeconds = animation.secondsLength;
				unitychan.animationPlaying = false;
			}
		}

		// �u�����h���̌v�Z
		if (unitychan.animationBlendSecondsLength > 0.0f)
		{
			float blendRate = unitychan.animationBlendSeconds / unitychan.animationBlendSecondsLength;

			// �u�����h�v�Z
			for (size_t i = 0; i < unitychan.nodePoses.size(); ++i)
			{
				const Model::NodePose& cache = unitychan.cacheNodePoses.at(i);
				Model::NodePose& pose = unitychan.nodePoses.at(i);

				DirectX::XMVECTOR S0 = DirectX::XMLoadFloat3(&cache.scale);
				DirectX::XMVECTOR S1 = DirectX::XMLoadFloat3(&pose.scale);
				DirectX::XMVECTOR R0 = DirectX::XMLoadFloat4(&cache.rotation);
				DirectX::XMVECTOR R1 = DirectX::XMLoadFloat4(&pose.rotation);
				DirectX::XMVECTOR T0 = DirectX::XMLoadFloat3(&cache.position);
				DirectX::XMVECTOR T1 = DirectX::XMLoadFloat3(&pose.position);

				DirectX::XMVECTOR S = DirectX::XMVectorLerp(S0, S1, blendRate);
				DirectX::XMVECTOR R = DirectX::XMQuaternionSlerp(R0, R1, blendRate);
				DirectX::XMVECTOR T = DirectX::XMVectorLerp(T0, T1, blendRate);

				DirectX::XMStoreFloat3(&pose.scale, S);
				DirectX::XMStoreFloat4(&pose.rotation, R);
				DirectX::XMStoreFloat3(&pose.position, T);
			}
			// �u�����h���ԍX�V
			unitychan.animationBlendSeconds += elapsedTime;
			if (unitychan.animationBlendSeconds >= unitychan.animationBlendSecondsLength)
			{
				unitychan.animationBlendSecondsLength = 0.0f;
			}
		}
	}

	// �m�[�h
	unitychan.model->SetNodePoses(unitychan.nodePoses);
}

// ���j�e�B�����X�e�[�g�}�V���X�V����
void CharacterControlScene::UpdateUnityChanStateMachine(float elapsedTime)
{
	// �X�e�[�g�؂�ւ�����
	if (unitychan.state != unitychan.nextState)
	{
		UnityChanStateExit(elapsedTime);

		unitychan.state = unitychan.nextState;

		UnityChanStateEnter(elapsedTime);
	}
	// �X�e�[�g�X�V����
	UnityChanStateUpdate(elapsedTime);
}

// ���j�e�B�����x���V�e�B�X�V����
void CharacterControlScene::UpdateUnityChanVelocity(float elapsedTime)
{
	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	// �d�͏���
	if (unitychan.onGround)
	{
		unitychan.velocity.y -= 0.001f;
	}
	else
	{
		unitychan.velocity.y -= gravity * elapsedFrame;
	}

	// �������͂̌���
	{
		float length = sqrtf(unitychan.velocity.x * unitychan.velocity.x + unitychan.velocity.z * unitychan.velocity.z);
		if (length > 0.0f)
		{
			// ���C��
			float friction = unitychan.friction * elapsedFrame;
			if (!unitychan.onGround)
			{
				friction *= unitychan.airControl;
			}
			if (length > friction)
			{
				float vx = unitychan.velocity.x / length;
				float vz = unitychan.velocity.z / length;

				unitychan.velocity.x -= vx * friction;
				unitychan.velocity.z -= vz * friction;
			}
			else
			{
				unitychan.velocity.x = 0.0f;
				unitychan.velocity.z = 0.0f;
			}
		}
	}


#if 0
	DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&unitychan.velocity);
	DirectX::XMVECTOR Length = DirectX::XMVector3Length(Velocity);
	if (DirectX::XMVectorGetX(Length) > unitychan.maxVelocity)
	{
		Velocity = DirectX::XMVectorDivide(Velocity, Length);
		Velocity = DirectX::XMVectorScale(Velocity, unitychan.maxVelocity);
		DirectX::XMStoreFloat3(&unitychan.velocity, Velocity);
	}
#endif
	unitychan.deltaMove.x += unitychan.velocity.x * elapsedTime;
	unitychan.deltaMove.y += unitychan.velocity.y * elapsedTime;
	unitychan.deltaMove.z += unitychan.velocity.z * elapsedTime;
}

// ���j�e�B�����ʒu�X�V����
void CharacterControlScene::UpdateUnityChanPosition(float elapsedTime)
{
	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	// �ړ�����
	{
		// ���C�L���X�g�ŕǔ������������Ȃ��悤�ɂ���
		float range = unitychan.radius * 0.5f;
		DirectX::XMFLOAT3 vec;
		DirectX::XMStoreFloat3(&vec, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&unitychan.deltaMove)));

		DirectX::XMFLOAT3 rayStart =
		{
			unitychan.position.x,
			unitychan.position.y + unitychan.radius,
			unitychan.position.z
		};
		DirectX::XMFLOAT3 rayEnd =
		{
			rayStart.x + vec.x * range + unitychan.deltaMove.x,
			rayStart.y + vec.y * range + unitychan.deltaMove.y,
			rayStart.z + vec.z * range + unitychan.deltaMove.z
		};
		HitResult hit;
		if (RayIntersectModel(rayStart, rayEnd, stage.model.get(), hit))
		{
			unitychan.position.x = hit.position.x + hit.normal.x * range;
			unitychan.position.y = hit.position.y + hit.normal.y * range - unitychan.radius;
			unitychan.position.z = hit.position.z + hit.normal.z * range;
		}
		else
		{
			unitychan.position.x += unitychan.deltaMove.x;
			unitychan.position.y += unitychan.deltaMove.y;
			unitychan.position.z += unitychan.deltaMove.z;
		}

		// �ړ��l���Z�b�g
		unitychan.deltaMove = { 0, 0, 0 };
	}

	// �X�e�[�W�Ƃ̏Փˏ���
	{
		bool onGround = unitychan.onGround;
		unitychan.onGround = false;

		DirectX::XMFLOAT3 position = unitychan.position;
		position.y += unitychan.radius;

		float groundAdjust = unitychan.groundAdjust * elapsedFrame;
		int n = 2;
		for (int i = 0; i < n; ++i)
		{
			if (SphereIntersectModel(position, unitychan.radius, stage.model.get(), unitychan.hits))
			{
				// ���ׂĂ̏Փˌ��ʂ𕽋ω������l���̗p����
				DirectX::XMFLOAT3 hitPosition = { 0, 0, 0 };
				DirectX::XMFLOAT3 hitGroundNormal = { 0, 0, 0 };
				int hitGroundCount = 0;
				for (HitResult& hit : unitychan.hits)
				{
					// �X�΂��S�T�x�ȉ��̃|���S���ƏՓ˂����ꍇ�͒n�ʂƂ���
					if (unitychan.velocity.y < 0)
					{
						float degree = DirectX::XMConvertToDegrees(acosf(hit.normal.y));
						if (degree <= unitychan.slopeLimit)
						{
							unitychan.onGround = true;
							unitychan.velocity.y = 0;
							hitGroundNormal.x += hit.normal.x;
							hitGroundNormal.y += hit.normal.y;
							hitGroundNormal.z += hit.normal.z;
							hitGroundCount++;
						}
					}
					hitPosition.x += hit.position.x;
					hitPosition.y += hit.position.y;
					hitPosition.z += hit.position.z;
				}
				// �Փ˂������W�𕽋ω�
				float f = 1.0f / static_cast<float>(unitychan.hits.size());
				unitychan.position.x = (hitPosition.x * f);
				unitychan.position.y = (hitPosition.y * f) - unitychan.radius;
				unitychan.position.z = (hitPosition.z * f);

				// �Փ˂����n�ʖ@���𕽋ω�
				if (hitGroundCount > 0)
				{
					f = 1.0f / static_cast<float>(hitGroundCount);
					unitychan.groundNormal.x = hitGroundNormal.x * f;
					unitychan.groundNormal.y = hitGroundNormal.y * f;
					unitychan.groundNormal.z = hitGroundNormal.z * f;
				}
			}
			// �O�t���[���ɐڒn��Ԃō��t���[���ŕ����Ă���ꍇ�͔���������ă`�F�b�N����
			// ���X���[�v��K�i�ŕ����Ȃ��悤�ɂ��邽��
			if (!onGround || unitychan.onGround || unitychan.velocity.y > 0)
			{
				break;
			}
			position.y -= groundAdjust / (n - 1);
		}

		// �ڒn���Ă��Ȃ��ꍇ�͒n�ʖ@����^������ɂ���
		if (!unitychan.onGround)
		{
			unitychan.groundNormal = { 0, 1, 0 };
		}
	}
}

// ���j�e�B�����s��X�V����
void CharacterControlScene::UpdateUnityChanTransform()
{
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(unitychan.angle.x, unitychan.angle.y, unitychan.angle.z);
	DirectX::XMMATRIX Translation = DirectX::XMMatrixTranslation(unitychan.position.x, unitychan.position.y, unitychan.position.z);
	DirectX::XMStoreFloat4x4(&unitychan.transform, Rotation * Translation);
	unitychan.model->UpdateTransform(unitychan.transform);

	float scale = 0.0f;
	switch (unitychan.state)
	{
		case UnityChan::State::Combo1:
		case UnityChan::State::Combo2:
		case UnityChan::State::Combo3:
		case UnityChan::State::Combo4:
			scale = 0.9f;
			break;
	}
	int leftHandNodeIndex = unitychan.model->GetNodeIndex("Character1_LeftHand");
	const Model::Node& leftHandNode = unitychan.model->GetNodes().at(leftHandNodeIndex);
	DirectX::XMMATRIX LeftHandWorldTransform = DirectX::XMLoadFloat4x4(&leftHandNode.worldTransform);
	DirectX::XMMATRIX StaffRotation = DirectX::XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(-76.0f), 0, 0);
	DirectX::XMMATRIX StaffScale = DirectX::XMMatrixScaling(scale, scale, scale);
	DirectX::XMMATRIX StaffTranslation = DirectX::XMMatrixTranslation(-0.057f, 0.026f, 0.026f);
	DirectX::XMMATRIX StaffLocalTransform = StaffScale * StaffRotation * StaffTranslation;
	DirectX::XMMATRIX StaffWorldTransform = StaffLocalTransform * LeftHandWorldTransform;
	DirectX::XMFLOAT4X4 staffWorldTransform;
	DirectX::XMStoreFloat4x4(&staffWorldTransform, StaffWorldTransform);
	unitychan.staff->UpdateTransform(staffWorldTransform);
}

// ���j�e�B�����G�t�F�N�g�X�V����
void CharacterControlScene::UpdateUnityChanEffect()
{
	UnityChanStaffTrail();
}

// ���j�e�B����񃋃b�N�A�b�g�X�V����
void CharacterControlScene::UpdateUnityChanLookAt(float elapsedTime)
{
	// �^�[�Q�b�g���W���Z�o�i�f�t�H���g�͑O��100���[�g����j
	DirectX::XMMATRIX RootWorldTransform = DirectX::XMLoadFloat4x4(&unitychan.transform);
	DirectX::XMVECTOR RootWorldForward = RootWorldTransform.r[2];
	DirectX::XMMATRIX HeadWorldTransform = DirectX::XMLoadFloat4x4(&unitychan.lookAtBone.node->worldTransform);
	DirectX::XMVECTOR HeadWorldPosition = HeadWorldTransform.r[3];
	DirectX::XMVECTOR TargetWorldPosition = DirectX::XMVectorAdd(HeadWorldPosition, DirectX::XMVectorScale(RootWorldForward, 100));
	// ��ԋ߂��{�[��������
	float minLength = 5.0f;
	for (const Ball& ball : balls)
	{
		DirectX::XMVECTOR BallWorldPosition = DirectX::XMLoadFloat3(&ball.position);
		DirectX::XMVECTOR WorldVec = DirectX::XMVectorSubtract(BallWorldPosition, HeadWorldPosition);
		DirectX::XMVECTOR Length = DirectX::XMVector3Length(WorldVec);
		float length = DirectX::XMVectorGetX(Length);
		if (length < minLength)
		{
			// �O�㔻��
			DirectX::XMVECTOR WorldDirection = DirectX::XMVector3Normalize(WorldVec);
			DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(RootWorldForward, WorldDirection);
			float dot = DirectX::XMVectorGetX(Dot);
			if (dot > 0.0f)
			{
				minLength = length;
				TargetWorldPosition = BallWorldPosition;
			}
		}
	}
	// �⊮����
	float elapsedFrame = ConvertToGameFrame(elapsedTime);
	DirectX::XMVECTOR WorldLookPosition = DirectX::XMLoadFloat3(&unitychan.lookAtBone.target);
	WorldLookPosition = DirectX::XMVectorLerp(WorldLookPosition, TargetWorldPosition, elapsedFrame * 0.2f);
	DirectX::XMStoreFloat3(&unitychan.lookAtBone.target, WorldLookPosition);
	// ���[�J����ԕϊ�
	DirectX::XMMATRIX InverseHeadWorldTransform = DirectX::XMMatrixInverse(nullptr, HeadWorldTransform);
	DirectX::XMVECTOR LocalVec = DirectX::XMVector3Transform(WorldLookPosition, InverseHeadWorldTransform);
	DirectX::XMVECTOR LocalDirection = DirectX::XMVector3Normalize(LocalVec);
	DirectX::XMVECTOR LocalForward = DirectX::XMLoadFloat3(&unitychan.lookAtBone.forward);

	// ���[�J����Ԃŉ�]����
	DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(LocalForward, LocalDirection);
	DirectX::XMVECTOR LocalAxis = DirectX::XMVector3Cross(LocalForward, LocalDirection);
	if (DirectX::XMVector3NotEqual(LocalAxis, DirectX::XMVectorZero()))
	{
		LocalAxis = DirectX::XMVector3Normalize(LocalAxis);
		float dot = DirectX::XMVectorGetX(Dot);
		if (dot > 0)
		{
			float angle = acosf(DirectX::XMVectorGetX(Dot));
			if (angle > 0)
			{
				DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
				DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&unitychan.lookAtBone.node->rotation);
				LocalRotation = DirectX::XMQuaternionMultiply(LocalRotationAxis, LocalRotation);
				DirectX::XMStoreFloat4(&unitychan.lookAtBone.node->rotation, LocalRotation);

				ComputeWorldTransform(unitychan.lookAtBone.node);
			}
		}
	}
}

// ���j�e�B�����IK�{�[���X�V����
void CharacterControlScene::UpdateUnityChanIKBones()
{
	// ��IK����
	bool processedFootIK = false;
	if (unitychan.applyFootIK)
	{
		float maxOffset = 0.25f;	// ������������ő啝
		// �n�ʂɌ����ă��C�L���X�g
		DirectX::XMFLOAT3 rayStart, rayEnd;
		rayStart.x = unitychan.position.x;
		rayStart.y = unitychan.position.y + unitychan.radius;
		rayStart.z = unitychan.position.z;

		rayEnd.x = unitychan.position.x;
		rayEnd.y = unitychan.position.y - maxOffset;
		rayEnd.z = unitychan.position.z;

		HitResult hit;
		if (RayIntersectModel(rayStart, rayEnd, stage.model.get(), hit))
		{
			float floorPositionY = hit.position.y;

			// ������n�ʂɌ����ă��C�L���X�g
			auto raycastLegs = [this](FootIKBone& bone, float heightOffset)
			{
				const float footHeightLength = 0.1f;

				bone.anklePosition.x = bone.footNode->worldTransform._41;
				bone.anklePosition.y = bone.footNode->worldTransform._42;
				bone.anklePosition.z = bone.footNode->worldTransform._43;

				bone.rayStart.x = bone.anklePosition.x;
				bone.rayStart.y = bone.anklePosition.y + heightOffset;
				bone.rayStart.z = bone.anklePosition.z;

				bone.rayEnd.x = bone.rayStart.x;
				bone.rayEnd.y = bone.rayStart.y - 100.0f;
				bone.rayEnd.z = bone.rayStart.z;

				bone.hit = RayIntersectModel(bone.rayStart, bone.rayEnd, stage.model.get(), bone.hitResult);
			};
			raycastLegs(unitychan.leftFootIKBone, 0.1f);
			raycastLegs(unitychan.rightFootIKBone, 0.1f);

			// ���̍ŏI�I�Ȉʒu���v�Z����
			auto computeAnkleTarget = [](FootIKBone& bone, float footHeight)
			{
				if (!bone.hit) return;

				DirectX::XMVECTOR RayStart = DirectX::XMLoadFloat3(&bone.rayStart);
				DirectX::XMVECTOR HitPosition = DirectX::XMLoadFloat3(&bone.hitResult.position);
				DirectX::XMVECTOR HitNormal = DirectX::XMLoadFloat3(&bone.hitResult.normal);
				DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(RayStart, HitPosition);
				DirectX::XMVECTOR ABLength = DirectX::XMVector3Dot(Vec, HitNormal);
				float abLength = DirectX::XMVectorGetX(ABLength);
				if (abLength == 0.f) return;

				DirectX::XMVECTOR B = DirectX::XMVectorSubtract(RayStart, DirectX::XMVectorMultiply(HitNormal, ABLength));
				DirectX::XMVECTOR IB = DirectX::XMVectorSubtract(RayStart, HitPosition);
				DirectX::XMVECTOR IBLength = DirectX::XMVector3Length(IB);
				float ibLength = DirectX::XMVectorGetX(IBLength);
				if (ibLength <= 0.0f)
				{
					DirectX::XMVECTOR C = DirectX::XMVectorAdd(HitPosition, DirectX::XMVectorScale(HitNormal, footHeight));
					DirectX::XMStoreFloat3(&bone.ankleTarget, C);
				}
				else
				{
					float ihLength = ibLength * footHeight / abLength;
					DirectX::XMVECTOR IH = DirectX::XMVectorScale(IB, ihLength / ibLength);
					DirectX::XMVECTOR H = DirectX::XMVectorAdd(HitPosition, IH);
					DirectX::XMVECTOR C = DirectX::XMVectorAdd(H, DirectX::XMVectorScale(HitNormal, footHeight));
					DirectX::XMStoreFloat3(&bone.ankleTarget, C);
				}
			};
			computeAnkleTarget(unitychan.leftFootIKBone, 0.04f);
			computeAnkleTarget(unitychan.rightFootIKBone, 0.04f);

			// ���������������v�Z����
			auto computeHipsOffset = [](FootIKBone& bone, DirectX::XMFLOAT3& offset, float& maxDot)
			{
				if (!bone.hit) return;

				DirectX::XMVECTOR AnkleTarget = DirectX::XMLoadFloat3(&bone.ankleTarget);
				DirectX::XMVECTOR AnklePosition = DirectX::XMLoadFloat3(&bone.anklePosition);
				DirectX::XMVECTOR AnkleToTarget = DirectX::XMVectorSubtract(AnkleTarget, AnklePosition);
				DirectX::XMVECTOR Down = DirectX::XMVectorSet(0, -1, 0, 0);
				DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(AnkleToTarget, Down);
				float dot = DirectX::XMVectorGetX(Dot);
				if (dot > maxDot)
				{
					maxDot = dot;
					DirectX::XMVECTOR Offset = DirectX::XMVectorScale(Down, dot);
					DirectX::XMStoreFloat3(&offset, Offset);
				}
			};
			DirectX::XMFLOAT3 offset = { 0, 0, 0 };
			float maxDot = -FLT_MAX;
			computeHipsOffset(unitychan.leftFootIKBone, offset, maxDot);
			computeHipsOffset(unitychan.rightFootIKBone, offset, maxDot);

			// �����̈ʒu�𒲐�����
			DirectX::XMVECTOR HipsOffset = DirectX::XMLoadFloat3(&unitychan.hipsOffset);
			DirectX::XMVECTOR Offset = DirectX::XMLoadFloat3(&offset);
			HipsOffset = DirectX::XMVectorLerp(HipsOffset, Offset, 0.1f);
			DirectX::XMStoreFloat3(&unitychan.hipsOffset, HipsOffset);

			Model::Node& hipNode = unitychan.model->GetNodes().at(unitychan.hipsNodeIndex);
			DirectX::XMMATRIX HipWorldTransform = DirectX::XMLoadFloat4x4(&hipNode.worldTransform);
			DirectX::XMMATRIX HipParentWorldTransform = DirectX::XMLoadFloat4x4(&hipNode.parent->worldTransform);
			DirectX::XMMATRIX InverseHipParentWorldTransform = DirectX::XMMatrixInverse(nullptr, HipParentWorldTransform);
			DirectX::XMVECTOR HipWorldPosition = DirectX::XMVectorAdd(HipWorldTransform.r[3], HipsOffset);
			DirectX::XMVECTOR HipLocalPosition = DirectX::XMVector3Transform(HipWorldPosition, InverseHipParentWorldTransform);
			DirectX::XMStoreFloat3(&hipNode.position, HipLocalPosition);
			ComputeWorldTransform(&hipNode);

			// IK����
			auto computeFootIK = [](FootIKBone& bone)
			{
				if (!bone.hit) return;

				// �|�[���^�[�Q�b�g�ʒu���Z�o
				DirectX::XMMATRIX LegWorldTransform = DirectX::XMLoadFloat4x4(&bone.legNode->worldTransform);
				DirectX::XMVECTOR PoleWorldPosition = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0.1f, 0, 0), LegWorldTransform);
				DirectX::XMFLOAT3 poleWorldPosition;
				DirectX::XMStoreFloat3(&poleWorldPosition, PoleWorldPosition);

				// IK����
				ComputeTwoBoneIK(bone.thighNode, bone.legNode, bone.footNode, bone.ankleTarget, poleWorldPosition);

				// ���̉�]��n�ʂɍ��킹��
				DirectX::XMMATRIX Leg = DirectX::XMLoadFloat4x4(&bone.legNode->worldTransform);
				DirectX::XMMATRIX Foot = DirectX::XMLoadFloat4x4(&bone.footNode->worldTransform);
				DirectX::XMMATRIX Toe = DirectX::XMLoadFloat4x4(&bone.toeNode->worldTransform);

				// �n�ʂ̖@���x�N�g�����Q�l�ɑ��̃��[���h�s��𒼐ڕҏW����
				Foot.r[1] = DirectX::XMLoadFloat3(&bone.hitResult.normal);
				Foot.r[0] = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Foot.r[3], Toe.r[3]));
				Foot.r[2] = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Foot.r[0], Foot.r[1]));
				Foot.r[0] = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Foot.r[1], Foot.r[2]));

				// �I�t�Z�b�g��]
				DirectX::XMMATRIX Offset = DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(60.0f));
				Foot = DirectX::XMMatrixMultiply(Offset, Foot);

				// ���[�J����ԕϊ�
				DirectX::XMMATRIX InverseLegWorldTransform = DirectX::XMMatrixInverse(nullptr, Leg);
				DirectX::XMMATRIX FootLocalTransform = DirectX::XMMatrixMultiply(Foot, InverseLegWorldTransform);
				FootLocalTransform.r[0] = DirectX::XMVector3Normalize(FootLocalTransform.r[0]);
				FootLocalTransform.r[1] = DirectX::XMVector3Normalize(FootLocalTransform.r[1]);
				FootLocalTransform.r[2] = DirectX::XMVector3Normalize(FootLocalTransform.r[2]);
				DirectX::XMVECTOR FootLocalRotation = DirectX::XMQuaternionRotationMatrix(FootLocalTransform);
				DirectX::XMStoreFloat4(&bone.footNode->rotation, FootLocalRotation);

				// ���[���h�s��v�Z
				ComputeWorldTransform(bone.footNode);

			};
			computeFootIK(unitychan.leftFootIKBone);
			computeFootIK(unitychan.rightFootIKBone);
			processedFootIK = true;
		}
	}
	// IK�������Ă��Ȃ��Ƃ��͍��������l�����Ƃɖ߂�
	if (!processedFootIK)
	{
		DirectX::XMVECTOR HipsOffset = DirectX::XMLoadFloat3(&unitychan.hipsOffset);
		HipsOffset = DirectX::XMVectorLerp(HipsOffset, DirectX::XMVectorZero(), 0.1f);
		DirectX::XMStoreFloat3(&unitychan.hipsOffset, HipsOffset);
	}
}

// ���j�e�B����񕨗��{�[���X�V����
void CharacterControlScene::UpdateUnityChanPhysicsBones(float elapsedTime)
{
	DirectX::XMFLOAT3 force = fieldForce;
	force.y -= gravity;
	force.x *= elapsedTime;
	force.y *= elapsedTime;
	force.z *= elapsedTime;

	ComputePhysicsBones(unitychan.leftHairTailBones, unitychan.upperBodyCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
	ComputePhysicsBones(unitychan.rightHairTailBones, unitychan.upperBodyCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
	ComputePhysicsBones(unitychan.leftSkirtFrontBones, unitychan.leftLegCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
	ComputePhysicsBones(unitychan.leftSkirtBackBones, unitychan.leftLegCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
	ComputePhysicsBones(unitychan.rightSkirtFrontBones, unitychan.rightLegCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
	ComputePhysicsBones(unitychan.rightSkirtBackBones, unitychan.rightLegCollisionBones, force, unitychan.maxPhysicsBoneVelocity);
}

// ���j�e�B�����A�j���[�V�����Đ�
void CharacterControlScene::PlayUnityChanAnimation(const char* name, float blendSeconds, bool loop, bool rootMotion)
{
	int animationIndex = unitychan.model->GetAnimationIndex(name);
	if (loop && animationIndex == unitychan.animationIndex)
	{
		return;
	}
	unitychan.animationIndex = unitychan.model->GetAnimationIndex(name);
	unitychan.animationSeconds = unitychan.oldAnimationSeconds = 0;
	unitychan.animationLoop = loop;
	unitychan.animationBlendSeconds = 0;
	unitychan.animationBlendSecondsLength = blendSeconds;
	unitychan.animationChange = true;
	unitychan.computeRootMotion = rootMotion;
	unitychan.animationPlaying = true;
}

// ���j�e�B�����X�e�[�g�؂�ւ�
void CharacterControlScene::ChangeUnityChanState(UnityChan::State state)
{
	unitychan.nextState = state;
}

// ���j�e�B�����X�e�[�g�J�n����
void CharacterControlScene::UnityChanStateEnter(float elapsedTime)
{
	switch (unitychan.state)
	{
		case UnityChan::State::Idle:
		{
			PlayUnityChanAnimation("Idle", 0.1f, true, false);
			break;
		}

		case UnityChan::State::Move:
		{
			//PlayUnityChanAnimation("RunForward", 0.1f, true, true);
			PlayUnityChanAnimation("RunForwardInPlace", 0.1f, true, false);
			break;
		}

		case UnityChan::State::Jump:
		{
			unitychan.velocity.x *= 0.8f;
			unitychan.velocity.z *= 0.8f;
			unitychan.velocity.y = unitychan.jumpSpeed;
			ChangeUnityChanState(UnityChan::State::Air);
			break;
		}

		case UnityChan::State::Combo1:
		{
			PlayUnityChanAnimation("Combo1", 0.1f, false, true);
			break;
		}

		case UnityChan::State::Combo2:
		{
			PlayUnityChanAnimation("Combo2", 0.1f, false, true);
			break;
		}

		case UnityChan::State::Combo3:
		{
			PlayUnityChanAnimation("Combo3", 0.1f, false, true);
			break;
		}

		case UnityChan::State::Combo4:
		{
			PlayUnityChanAnimation("Combo4", 0.1f, false, true);
			break;
		}

		case UnityChan::State::Landing:
		{
			if (unitychan.inputAxisLength > 0.5f)
			{
				if (unitychan.velocity.y < -5.0f)
				{
					PlayUnityChanAnimation("RunForwardLandingFast", 0.3f, false, true);
				}
				else
				{
					PlayUnityChanAnimation("RunForwardLanding", 0.2f, false, true);
				}
			}
			else
			{
				if (unitychan.velocity.y < -5.0f)
				{
					PlayUnityChanAnimation("IdleLandFast", 0.2f, false, true);
				}
				else
				{
					PlayUnityChanAnimation("IdleLand", 0.2f, false, true);
				}
			}
			break;
		}
	}
}

// ���j�e�B�����X�e�[�g�J�n����
void CharacterControlScene::UnityChanStateUpdate(float elapsedTime)
{
	unitychan.applyFootIK = false;

	switch (unitychan.state)
	{
		case UnityChan::State::Idle:
		{
			unitychan.applyFootIK = true;

			if (unitychan.inputAxisLength > 0.5f)
			{
				ChangeUnityChanState(UnityChan::State::Move);
			}
			if (unitychan.inputKeyDown & UnityChan::KeyJump)
			{
				ChangeUnityChanState(UnityChan::State::Jump);
			}

			UnityChanInputCombo();

			break;
		}

		case UnityChan::State::Move:
		{
			UnityChanInputMovement(elapsedTime);

			if (unitychan.inputAxisLength <= 0.5f)
			{
				ChangeUnityChanState(UnityChan::State::Idle);
			}
			if (!unitychan.onGround)
			{
				ChangeUnityChanState(UnityChan::State::Air);
			}
			if (unitychan.inputKeyDown & UnityChan::KeyJump)
			{
				ChangeUnityChanState(UnityChan::State::Jump);
			}

			UnityChanInputCombo();

			break;
		}

		case UnityChan::State::Jump:
		case UnityChan::State::Air:
		{
			UnityChanInputMovement(elapsedTime);

			UnityChanAirControl(elapsedTime);

			break;
		}

		case UnityChan::State::Landing:
		{
			UnityChanInputMovement(elapsedTime);

			if (!unitychan.animationPlaying)
			{
				if (unitychan.inputAxisLength > 0.5f)
				{
					ChangeUnityChanState(UnityChan::State::Move);
				}
				else
				{
					ChangeUnityChanState(UnityChan::State::Idle);
				}
			}

			break;
		}

		case UnityChan::State::Combo1:
		case UnityChan::State::Combo2:
		case UnityChan::State::Combo3:
		case UnityChan::State::Combo4:
		{
			UnityChanInputMovement(elapsedTime);
			UnityChanInputCombo();
			break;
		}
	}
}

// ���j�e�B�����X�e�[�g�I������
void CharacterControlScene::UnityChanStateExit(float elapsedTime)
{
	switch (unitychan.state)
	{
		case UnityChan::State::Idle:
		{
			break;
		}

		case UnityChan::State::Move:
		{
			break;
		}

		case UnityChan::State::Jump:
		{
			break;
		}

		case UnityChan::State::Landing:
		{
			unitychan.velocity.x = unitychan.desiredDirection.x * unitychan.moveSpeed * unitychan.inputAxisLength;
			unitychan.velocity.z = unitychan.desiredDirection.z * unitychan.moveSpeed * unitychan.inputAxisLength;
			break;
		}
		case UnityChan::State::Combo1:
		case UnityChan::State::Combo2:
		case UnityChan::State::Combo3:
		case UnityChan::State::Combo4:
		{
			unitychan.combo = false;
			break;
		}
	}
}

// ���j�e�B�������͈ړ�����
void CharacterControlScene::UnityChanInputMovement(float elapsedTime)
{
	// �J�������
	const DirectX::XMFLOAT3& cameraFront = camera.GetFront();
	const DirectX::XMFLOAT3& cameraRight = camera.GetRight();
	float cameraFrontLengthXZ = sqrtf(cameraFront.x * cameraFront.x + cameraFront.z * cameraFront.z);
	float cameraRightLengthXZ = sqrtf(cameraRight.x * cameraRight.x + cameraRight.z * cameraRight.z);
	float cameraFrontX = cameraFront.x / cameraFrontLengthXZ;
	float cameraFrontZ = cameraFront.z / cameraFrontLengthXZ;
	float cameraRightX = cameraRight.x / cameraRightLengthXZ;
	float cameraRightZ = cameraRight.z / cameraRightLengthXZ;

	// ����
	float moveX = cameraFrontX * unitychan.inputAxisY + cameraRightX * unitychan.inputAxisX;
	float moveZ = cameraFrontZ * unitychan.inputAxisY + cameraRightZ * unitychan.inputAxisX;
	float moveLength = sqrtf(moveX * moveX + moveZ * moveZ);
	if (moveLength > 0)
	{
		float vecX = moveX / moveLength;
		float vecZ = moveZ / moveLength;
		float dirX = sinf(unitychan.angle.y);
		float dirZ = cosf(unitychan.angle.y);
		float cross = dirX * vecZ - dirZ * vecX;
		float dot = dirX * vecX + dirZ * vecZ;
		float rot = 1.0f - dot;
		float turnSpeed = unitychan.turnSpeed * elapsedTime;
		// �󒆂ɂ���Ƃ��͉����͂����炷
		if (!unitychan.onGround)
		{
			turnSpeed *= unitychan.airControl;
		}

		if (rot > turnSpeed)
		{
			rot = turnSpeed;
		}
		if (cross < 0)
		{
			unitychan.angle.y += rot;
		}
		else
		{
			unitychan.angle.y -= rot;
		}
	}

	// �ړ�
	if (unitychan.computeRootMotion)
	{
		DirectX::XMVECTOR Translation = DirectX::XMLoadFloat3(&unitychan.rootMotionTranslation);
		DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&unitychan.transform);
		DirectX::XMVECTOR Move = DirectX::XMVector3TransformNormal(Translation, Transform);
		DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&unitychan.groundNormal);
		Move = DirectX::XMVector3Cross(Normal, DirectX::XMVector3Cross(Move, Normal));

		DirectX::XMFLOAT3 move = { 0, 0, 0 };
		DirectX::XMStoreFloat3(&move, Move);

		unitychan.velocity.x *= 0.8f;
		unitychan.velocity.z *= 0.8f;
		unitychan.deltaMove.x += move.x;
		unitychan.deltaMove.y += move.y;
		unitychan.deltaMove.z += move.z;
	}
	else if (moveLength > 0.0f)
	{
		// ��������
		float length = sqrtf(unitychan.velocity.x * unitychan.velocity.x + unitychan.velocity.z * unitychan.velocity.z);
		if (length <= unitychan.moveSpeed)
		{
			float elapsedFrame = elapsedTime * 60.0f;
			float acceleration = unitychan.acceleration * elapsedFrame;
			float moveSpeed = unitychan.moveSpeed;
			// �󒆂ɂ���Ƃ��͉����͂����炷
			if (!unitychan.onGround)
			{
				acceleration *= unitychan.airControl;
				moveSpeed *= 0.8f;
			}
			// ���x����
			DirectX::XMFLOAT3 vec;
			vec.x = unitychan.velocity.x + moveX * acceleration;
			vec.y = 0.0f;
			vec.z = unitychan.velocity.z + moveZ * acceleration;
			length = sqrtf(vec.x * vec.x + vec.z * vec.z);
			if (length > moveSpeed)
			{
				float scale = moveSpeed / length;
				vec.x *= scale;
				vec.z *= scale;
			}
			vec.x -= unitychan.velocity.x;
			vec.z -= unitychan.velocity.z;

			// �X�ΑΉ�
			DirectX::XMVECTOR Vec = DirectX::XMLoadFloat3(&vec);
			DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&unitychan.groundNormal);
			Vec = DirectX::XMVector3Cross(Normal, DirectX::XMVector3Cross(Vec, Normal));
			DirectX::XMStoreFloat3(&vec, Vec);

			unitychan.velocity.x += vec.x;
			//unitychan.velocity.y += vec.y;
			unitychan.velocity.z += vec.z;
			unitychan.deltaMove.y += vec.y * elapsedTime;
		}
	}
	unitychan.desiredDirection.x = moveX;
	unitychan.desiredDirection.y = 0.0f;
	unitychan.desiredDirection.z = moveZ;
}

// ���j�e�B�������͈ړ�����
void CharacterControlScene::UnityChanInputJump()
{
}

// ���j�e�B�����󒆏���
void CharacterControlScene::UnityChanAirControl(float elapsedTime)
{
	if (unitychan.velocity.y > 10.0f)
	{
		PlayUnityChanAnimation("JumpTakeOff", 0.1f, true, false);
	}
	else if (unitychan.velocity.y > 5.0f)
	{
		PlayUnityChanAnimation("JumpGoesUp2", 0.1f, true, false);
	}
	else if (unitychan.velocity.y > 0.5f)
	{
		PlayUnityChanAnimation("JumpGoesUp", 0.1f, true, false);
	}
	else if (unitychan.velocity.y < -5.f)
	{
		PlayUnityChanAnimation("JumpGoesDown2", 0.1f, true, false);
	}
	else if (unitychan.velocity.y < -0.5f)
	{
		PlayUnityChanAnimation("JumpGoesDown", 0.1f, true, false);
	}
	else
	{
		PlayUnityChanAnimation("JumpPeak", 0.1f, true, false);
	}

	if (unitychan.velocity.y <= 0.0f)
	{
		if (unitychan.onGround)
		{
			ChangeUnityChanState(UnityChan::State::Idle);
		}
		else
		{
			// �n�ʂɌ����ă��C�L���X�g
			DirectX::XMFLOAT3 rayStart, rayEnd;
			rayStart.x = unitychan.position.x;
			rayStart.y = unitychan.position.y + unitychan.radius;
			rayStart.z = unitychan.position.z;

			rayEnd.x = unitychan.position.x;
			rayEnd.y = unitychan.position.y + (unitychan.velocity.y * elapsedTime) - 0.5f;
			rayEnd.z = unitychan.position.z;

			HitResult hit;
			if (RayIntersectModel(rayStart, rayEnd, stage.model.get(), hit))
			{
				ChangeUnityChanState(UnityChan::State::Landing);
			}
		}
	}
}

// ���j�e�B�����R���{���͏���
void CharacterControlScene::UnityChanInputCombo()
{
	// �e�X�e�[�g���̎��̃R���{�փV�t�g����t���[�����擾
	float shiftFrame = 10000;
	switch (unitychan.state)
	{
		case UnityChan::State::Combo1: shiftFrame = 15; break;
		case UnityChan::State::Combo2: shiftFrame = 17; break;
		case UnityChan::State::Combo3: shiftFrame = 35; break;
		case UnityChan::State::Combo4: shiftFrame = -1; break;
	}
	float frame = unitychan.animationSeconds * 60.0f;

	if (!unitychan.combo)
	{
		// �V�t�g�t���[���܂łɃL�[���͂�����΃R���{
		if (unitychan.inputKeyDown & UnityChan::KeyAttack)
		{
			if (frame < shiftFrame)
			{
				unitychan.combo = true;
			}
		}
	}

	if (unitychan.combo)
	{
		// ���̃X�e�[�g��
		if (frame >= shiftFrame || shiftFrame > 9999)
		{
			switch (unitychan.state)
			{				
				default: ChangeUnityChanState(UnityChan::State::Combo1);	break;
				case UnityChan::State::Combo1: ChangeUnityChanState(UnityChan::State::Combo2);	break;
				case UnityChan::State::Combo2: ChangeUnityChanState(UnityChan::State::Combo3);	break;
				case UnityChan::State::Combo3: ChangeUnityChanState(UnityChan::State::Combo4);	break;
			}
			unitychan.combo = false;
		}
	}

	if (!unitychan.animationPlaying)
	{
		unitychan.combo = false;
		ChangeUnityChanState(UnityChan::State::Idle);
	}

}

// ���j�e�B�����X�^�b�t�g���[������
void CharacterControlScene::UnityChanStaffTrail()
{
	// �ۑ����Ă������W�����炷
	for (int i = 0; i < 2; ++i)
	{
		for (int j = _countof(unitychan.trailPositions[i]) - 1; j > 0; --j)
		{
			unitychan.trailPositions[i][j] = unitychan.trailPositions[i][j - 1];
		}
	}

	// ���̍��{�Ɛ�[������W���擾
	DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&unitychan.staff->GetNodes().at(0).worldTransform);
	DirectX::XMVECTOR Root = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, 0.6f, 0), WorldTransform);
	DirectX::XMVECTOR Tip = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, 0.9f, 0), WorldTransform);
	DirectX::XMStoreFloat3(&unitychan.trailPositions[0][0], Root);
	DirectX::XMStoreFloat3(&unitychan.trailPositions[1][0], Tip);
}

// ���[�g���[�V��������
void CharacterControlScene::ComputeRootMotion(
	const Model* model,
	int rootMotionNodeIndex,
	int animationIndex,
	float oldAnimationSeconds,
	float newAnimationSeconds,
	DirectX::XMFLOAT3& translation,
	DirectX::XMFLOAT3& rootMotionNodePosition,
	bool bakeTranslationX,
	bool bakeTranslationY,
	bool bakeTranslationZ)
{
	// ����A�O��A����̃��[�g���[�V�����m�[�h�̎p�����擾
	Model::NodePose beginPose, oldPose, newPose;
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, 0, beginPose);
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, oldAnimationSeconds, oldPose);
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, newAnimationSeconds, newPose);

	// ���[�J���ړ��l���Z�o
	DirectX::XMFLOAT3 localTranslation;
	if (oldAnimationSeconds > newAnimationSeconds)
	{
		// ���[�v������
		const Model::Animation& animation = model->GetAnimations().at(animationIndex);
		Model::NodePose endPose;
		model->ComputeAnimation(animationIndex, rootMotionNodeIndex, animation.secondsLength, endPose);

		localTranslation.x = (endPose.position.x - oldPose.position.x) + (newPose.position.x - beginPose.position.x);
		localTranslation.y = (endPose.position.y - oldPose.position.y) + (newPose.position.y - beginPose.position.y);
		localTranslation.z = (endPose.position.z - oldPose.position.z) + (newPose.position.z - beginPose.position.z);
	}
	else
	{
		// �ʏ폈��
		localTranslation.x = newPose.position.x - oldPose.position.x;
		localTranslation.y = newPose.position.y - oldPose.position.y;
		localTranslation.z = newPose.position.z - oldPose.position.z;
	}

	// �O���[�o���ړ��l���Z�o
	const Model::Node& rootMotionNode = model->GetNodes().at(rootMotionNodeIndex);
	DirectX::XMVECTOR LocalTranslation = DirectX::XMLoadFloat3(&localTranslation);
	DirectX::XMMATRIX ParentGlobalTransform = DirectX::XMLoadFloat4x4(&rootMotionNode.parent->globalTransform);
	DirectX::XMVECTOR GlobalTranslation = DirectX::XMVector3TransformNormal(LocalTranslation, ParentGlobalTransform);
	DirectX::XMStoreFloat3(&translation, GlobalTranslation);

	if (bakeTranslationX || bakeTranslationY || bakeTranslationZ)
	{
		// �ړ��l�𔲂�
		if (bakeTranslationX) translation.x = 0;
		if (bakeTranslationY) translation.y = 0;
		if (bakeTranslationZ) translation.z = 0;
	
		// ����̎p���̃��[���h�ʒu���Z�o
		DirectX::XMVECTOR LocalPosition = DirectX::XMLoadFloat3(&newPose.position);
		DirectX::XMVECTOR GlobalPosition = DirectX::XMVector3Transform(LocalPosition, ParentGlobalTransform);

		// XZ�������폜
		if (bakeTranslationY && bakeTranslationZ)
		{
			GlobalPosition = DirectX::XMVectorSetX(GlobalPosition, 0);
		}
		else if (bakeTranslationX && bakeTranslationZ)
		{
			GlobalPosition = DirectX::XMVectorSetY(GlobalPosition, 0);
		}
		else if (bakeTranslationX && bakeTranslationY)
		{
			GlobalPosition = DirectX::XMVectorSetZ(GlobalPosition, 0);
		}
		else if (bakeTranslationX)
		{
			GlobalPosition = DirectX::XMVectorSetY(GlobalPosition, 0);
			GlobalPosition = DirectX::XMVectorSetZ(GlobalPosition, 0);
		}
		else if (bakeTranslationY)
		{
			GlobalPosition = DirectX::XMVectorSetX(GlobalPosition, 0);
			GlobalPosition = DirectX::XMVectorSetZ(GlobalPosition, 0);
		}
		else if (bakeTranslationZ)
		{
			GlobalPosition = DirectX::XMVectorSetX(GlobalPosition, 0);
			GlobalPosition = DirectX::XMVectorSetY(GlobalPosition, 0);
		}

		// ���[�J����ԕϊ�
		DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
		LocalPosition = DirectX::XMVector3Transform(GlobalPosition, InverseParentGlobalTransform);
		// ���[�g���[�V�����m�[�h�̈ʒu��ݒ�
		DirectX::XMStoreFloat3(&rootMotionNodePosition, LocalPosition);
	}
	else
	{
		// ���[�g���[�V�����m�[�h�̈ʒu������̈ʒu�ɂ���
		rootMotionNodePosition = beginPose.position;
	}
}

// �����{�[���v�Z����
void CharacterControlScene::ComputePhysicsBones(
	std::vector<PhysicsBone>& bones,
	const std::vector<CollisionBone>& collisionBones,
	const DirectX::XMFLOAT3& force,
	float maxVelocity)
{
	DirectX::XMVECTOR Force = DirectX::XMLoadFloat3(&force);

	// ���[�g�{�[���̃��[���h�s��X�V
	PhysicsBone& root = bones.at(0);
	root.worldTransform = root.node->worldTransform;

	for (size_t i = 1; i < bones.size(); ++i)
	{
		PhysicsBone& bone = bones[i - 1];
		PhysicsBone& child = bones[i];

		DirectX::XMMATRIX ChildWorldTransform = DirectX::XMLoadFloat4x4(&child.worldTransform);
		DirectX::XMVECTOR ChildOldWorldPosition = DirectX::XMLoadFloat3(&child.oldWorldPosition);
		DirectX::XMVECTOR ChildWorldPosition = ChildWorldTransform.r[3];
		DirectX::XMStoreFloat3(&child.oldWorldPosition, ChildWorldPosition);

		// �O��ʒu���獡��ʒu�̍����Əd�͂ɂ�鑬�͂��Z�o����
		DirectX::XMVECTOR Velocity = DirectX::XMVectorSubtract(ChildWorldPosition, ChildOldWorldPosition);
		DirectX::XMVECTOR LengthSq = DirectX::XMVector3LengthSq(Velocity);
		if (DirectX::XMVectorGetX(LengthSq) > maxVelocity * maxVelocity)
		{
			Velocity = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Velocity), maxVelocity);
		}
		Velocity = DirectX::XMVectorScale(Velocity, 0.9f);
		Velocity = DirectX::XMVectorAdd(Velocity, Force);

		// �q�{�[���̈ʒu�𓮂���
		ChildWorldPosition = DirectX::XMVectorAdd(ChildWorldPosition, Velocity);

		// �R���W����
		ComputeCollisionBones(collisionBones, ChildWorldPosition, child.collisionRadius);

		// �f�t�H���g���[���h�s��
		DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&bone.node->parent->worldTransform);
		DirectX::XMMATRIX DefaultLocalTransform = DirectX::XMLoadFloat4x4(&bone.defaultLocalTransform);
		DirectX::XMMATRIX DefaultWorldTransform = DirectX::XMMatrixMultiply(DefaultLocalTransform, ParentWorldTransform);
		DirectX::XMFLOAT4X4 defaultWorldTransform;
		DirectX::XMStoreFloat4x4(&defaultWorldTransform, DefaultWorldTransform);
#if 0
		// �p�x����
		LimitBoneAngle(
			ConvertBoneWorldTransformAxis(defaultWorldTransform),
			ConvertBoneWorldTransformAxis(bone.worldTransform),
			bone.limitMinYaw, bone.limitMaxYaw,
			bone.limitMinPitch, bone.limitMaxPitch,
			ChildWorldPosition);
#endif
		// �ŏI�I�ȃ{�[���̕���
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
		DirectX::XMVECTOR WorldPosition = WorldTransform.r[3];
		DirectX::XMVECTOR WorldTargetDirection = DirectX::XMVectorSubtract(ChildWorldPosition, WorldPosition);
		WorldTargetDirection = DirectX::XMVector3Normalize(WorldTargetDirection);
		// ������Ԃ̐e�̃{�[������
		DirectX::XMVECTOR ChildLocalPosition = DirectX::XMLoadFloat3(&child.localPosition);
		DirectX::XMVECTOR WorldDefaultDirection = DirectX::XMVector3TransformNormal(ChildLocalPosition, DefaultWorldTransform);
		WorldDefaultDirection = DirectX::XMVector3Normalize(WorldDefaultDirection);
		// ��]�p�x�Z�o
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(WorldDefaultDirection, WorldTargetDirection);
		float angle = acosf(DirectX::XMVectorGetX(Dot));
		if (fabsf(angle) > 0.001f)
		{
			// ��]���Z�o
			DirectX::XMVECTOR WorldAxis = DirectX::XMVector3Cross(WorldDefaultDirection, WorldTargetDirection);
			if (DirectX::XMVector3NotEqual(WorldAxis, DirectX::XMVectorZero()))
			{
				WorldAxis = DirectX::XMVector3Normalize(WorldAxis);

				// ��]�������[�J����ԕϊ�
				DirectX::XMMATRIX DefaultInverseWorldTransform = DirectX::XMMatrixInverse(nullptr, DefaultWorldTransform);
				DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(WorldAxis, DefaultInverseWorldTransform);
				LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

				// ��]����
				DirectX::XMMATRIX LocalRotationAxis = DirectX::XMMatrixRotationAxis(LocalAxis, angle);
				WorldTransform = DirectX::XMMatrixMultiply(LocalRotationAxis, DefaultWorldTransform);
				DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

				// ���[�J����ԕϊ�
				DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(WorldTransform, DefaultInverseWorldTransform);
				DirectX::XMVECTOR LocalRotation = DirectX::XMQuaternionRotationMatrix(LocalTransform);

				DirectX::XMVECTOR DefaultLocalRotation = DirectX::XMLoadFloat4(&bone.defaultLocalRotation);
				LocalRotation = DirectX::XMQuaternionSlerp(LocalRotation, DefaultLocalRotation, 0.1f);

				DirectX::XMStoreFloat4(&bone.localRotation, LocalRotation);
			}

			// �q���[���h�s��X�V
			DirectX::XMMATRIX ChildLocalPositionTransform = DirectX::XMMatrixTranslation(child.localPosition.x, child.localPosition.y, child.localPosition.z);
			DirectX::XMMATRIX ChildLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&child.localRotation));
			DirectX::XMMATRIX ChildLocalTransform = DirectX::XMMatrixMultiply(ChildLocalRotationTransform, ChildLocalPositionTransform);
			ChildWorldTransform = DirectX::XMMatrixMultiply(ChildLocalTransform, WorldTransform);
			DirectX::XMStoreFloat4x4(&child.worldTransform, ChildWorldTransform);
		}
		else
		{
			// ���[���h�s��X�V
			DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone.localRotation);
			DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixRotationQuaternion(LocalRotation);
			LocalTransform.r[3] = DirectX::XMVectorSet(bone.localPosition.x, bone.localPosition.y, bone.localPosition.z, 1);
			WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
			DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

			// �q���[���h�s��X�V
			DirectX::XMMATRIX ChildLocalPositionTransform = DirectX::XMMatrixTranslation(child.localPosition.x, child.localPosition.y, child.localPosition.z);
			DirectX::XMMATRIX ChildLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&child.localRotation));
			DirectX::XMMATRIX ChildLocalTransform = DirectX::XMMatrixMultiply(ChildLocalRotationTransform, ChildLocalPositionTransform);
			ChildWorldTransform = DirectX::XMMatrixMultiply(ChildLocalTransform, WorldTransform);
			DirectX::XMStoreFloat4x4(&child.worldTransform, ChildWorldTransform);
		}
		// �m�[�h�ɔ��f
		bone.node->worldTransform = bone.worldTransform;
		bone.node->rotation = bone.localRotation;
		child.node->worldTransform = child.worldTransform;
	}

}

// TwoBoneIK�v�Z����
void CharacterControlScene::ComputeTwoBoneIK(Model::Node* rootBone, Model::Node* midBone, Model::Node* tipBone, const DirectX::XMFLOAT3& targetPosition, const DirectX::XMFLOAT3& polePosition)
{
	// �^�[�Q�b�g���W���擾
	DirectX::XMVECTOR TargetWorldPosition = DirectX::XMLoadFloat3(&targetPosition);

	// �e�{�[�����W���擾
	DirectX::XMMATRIX RootWorldTransform = DirectX::XMLoadFloat4x4(&rootBone->worldTransform);
	DirectX::XMMATRIX MidWorldTransform = DirectX::XMLoadFloat4x4(&midBone->worldTransform);
	DirectX::XMMATRIX TipWorldTransform = DirectX::XMLoadFloat4x4(&tipBone->worldTransform);

	DirectX::XMVECTOR RootWorldPosition = RootWorldTransform.r[3];
	DirectX::XMVECTOR MidWorldPosition = MidWorldTransform.r[3];
	DirectX::XMVECTOR TipWorldPosition = TipWorldTransform.r[3];

	// ���{�����ԁA���ԁ���[�A���{����[�x�N�g�����Z�o
	DirectX::XMVECTOR RootMidVec = DirectX::XMVectorSubtract(MidWorldPosition, RootWorldPosition);
	DirectX::XMVECTOR RootTargetVec = DirectX::XMVectorSubtract(TargetWorldPosition, RootWorldPosition);
	DirectX::XMVECTOR MidTipVec = DirectX::XMVectorSubtract(TipWorldPosition, MidWorldPosition);

	// �e�x�N�g���̒������Z�o
	float rootMidLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootMidVec));
	float midTipLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(MidTipVec));
	float rootTargetLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootTargetVec));

	// �{�[����]�֐�(�Q�̃x�N�g�������]�p�Ɖ�]�����Z�o���A�{�[������]������j
	auto rotateBone = [&](Model::Node* bone, const DirectX::XMVECTOR& Direction1, const DirectX::XMVECTOR& Direction2)
	{
		// ��]���Z�o
		DirectX::XMVECTOR WorldAxis = DirectX::XMVector3Cross(Direction1, Direction2);
		if (DirectX::XMVector3Equal(WorldAxis, DirectX::XMVectorZero())) return;
		WorldAxis = DirectX::XMVector3Normalize(WorldAxis);

		// ��]�������[�J����ԕϊ�
		DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&bone->parent->worldTransform);
		DirectX::XMMATRIX InverseParentWorldTransform = DirectX::XMMatrixInverse(nullptr, ParentWorldTransform);
		DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(WorldAxis, InverseParentWorldTransform);
		LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

		// ��]�p�x�Z�o
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Direction1, Direction2);
		float dot = DirectX::XMVectorGetX(Dot);
		float angle = acosf(std::clamp(dot, -1.0f, 1.0f));	// ���ς̌��ʂ��덷��-1.0�`1.0�̊ԂɎ��܂�Ȃ��ꍇ������

		// ��]�N�H�[�^�j�I���Z�o
		DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
		DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone->rotation);
		LocalRotation = DirectX::XMQuaternionMultiply(LocalRotation, LocalRotationAxis);
		DirectX::XMStoreFloat4(&bone->rotation, LocalRotation);
	};

	// �e�x�N�g����P�ʃx�N�g����
	DirectX::XMVECTOR RootTargetDirection = DirectX::XMVector3Normalize(RootTargetVec);
	DirectX::XMVECTOR RootMidDirection = DirectX::XMVector3Normalize(RootMidVec);

	// ���[�g�{�[�����^�[�Q�b�g�̕��֌����悤�ɉ�]������
	rotateBone(rootBone, RootMidDirection, RootTargetDirection);

	// ���[�g�{�[������^�[�Q�b�g�܂ł̋������Q�̃{�[���̒����̍��v���Z���ꍇ��
	// ��[�{�[�����^�[�Q�b�g�ʒu�Ɠ����ɂȂ�悤�ɉ�]����������
	if (rootTargetLength < rootMidLength + midTipLength)
	{
		// �w�����̌����ŎO�p�`�̖ʐς����߂�
		float s = (rootMidLength + midTipLength + rootTargetLength) / 2;
		float square = sqrtf(s * (s - rootMidLength) * (s - midTipLength) * (s - rootTargetLength));

		// ���[�g�{�[�����ӂƂ������̎O�p�`�̍��������߂�
		float rootMidHeight = (2 * square) / rootMidLength;			

		// ���p�O�p�`�̎Εӂƍ�������p�x�����߂�
		float angle = asinf(rootMidHeight / rootTargetLength);

		if (angle > FLT_EPSILON)
		{
			DirectX::XMVECTOR PoleWorldPosition = DirectX::XMLoadFloat3(&polePosition);

			// ���[�g�{�[������|�[���^�[�Q�b�g�ւ̃x�N�g���Z�o
			DirectX::XMVECTOR RootPoleVec = DirectX::XMVectorSubtract(PoleWorldPosition, RootWorldPosition);
			DirectX::XMVECTOR RootPoleDirection = DirectX::XMVector3Normalize(RootPoleVec);
			// ���[�g�{�[������]�������]�������߂�
			DirectX::XMMATRIX RootParentWorldTransform = DirectX::XMLoadFloat4x4(&rootBone->parent->worldTransform);
			DirectX::XMMATRIX InverseRootParentWorldTransform = DirectX::XMMatrixInverse(nullptr, RootParentWorldTransform);
			DirectX::XMVECTOR PoleWorldAxis = DirectX::XMVector3Cross(RootMidDirection, RootPoleDirection);
			DirectX::XMVECTOR PoleLocalAxis = DirectX::XMVector3TransformNormal(PoleWorldAxis, InverseRootParentWorldTransform);
			PoleLocalAxis = DirectX::XMVector3Normalize(PoleLocalAxis);
			// ���[�g�{�[������]������
			DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(PoleLocalAxis, angle);
			DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&rootBone->rotation);
			LocalRotation = DirectX::XMQuaternionMultiply(LocalRotation, LocalRotationAxis);
			DirectX::XMStoreFloat4(&rootBone->rotation, LocalRotation);
		}

	}
	// ���[���h�s��v�Z
	ComputeWorldTransform(rootBone);

	// ���ԃ{�[���Ɛ�[�{�[���̃��[���h���W���擾����
	MidWorldTransform = DirectX::XMLoadFloat4x4(&midBone->worldTransform);
	MidWorldPosition = MidWorldTransform.r[3];
	TipWorldTransform = DirectX::XMLoadFloat4x4(&tipBone->worldTransform);
	TipWorldPosition = TipWorldTransform.r[3];

	// ���ԃ{�[�����^�[�Q�b�g�̕��֌����悤�ɉ�]������
	MidTipVec = DirectX::XMVectorSubtract(TipWorldPosition, MidWorldPosition);
	DirectX::XMVECTOR MidTipDirection = DirectX::XMVector3Normalize(MidTipVec);
	DirectX::XMVECTOR MidTargetVec = DirectX::XMVectorSubtract(TargetWorldPosition, MidWorldPosition);
	DirectX::XMVECTOR MidTargetDirection = DirectX::XMVector3Normalize(MidTargetVec);

	rotateBone(midBone, MidTipDirection, MidTargetDirection);
	ComputeWorldTransform(midBone);
}

// �R���W������
void CharacterControlScene::ComputeCollisionBones(const std::vector<CollisionBone>& collisionBones, DirectX::XMVECTOR& Position, float radius)
{
	for (const CollisionBone& bone : collisionBones)
	{
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
		DirectX::XMVECTOR Offset = DirectX::XMLoadFloat3(&bone.offset);
		DirectX::XMVECTOR WorldPosition = DirectX::XMVector3Transform(Offset, WorldTransform);

		DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Position, WorldPosition);
		DirectX::XMVECTOR LengthSq = DirectX::XMVector3LengthSq(Vec);
		float lengthSq = DirectX::XMVectorGetX(LengthSq);
		float range = bone.radius + radius;
		if (lengthSq < range * range)
		{
			Vec = DirectX::XMVector3Normalize(Vec);
			Position = DirectX::XMVectorAdd(WorldPosition, DirectX::XMVectorScale(Vec, range));
		}
	}
}

// �w��m�[�h�ȉ��̃��[���h�s����v�Z
void CharacterControlScene::ComputeWorldTransform(Model::Node* node)
{
	DirectX::XMMATRIX LocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node->rotation));
	DirectX::XMMATRIX LocalPositionTransform = DirectX::XMMatrixTranslation(node->position.x, node->position.y, node->position.z);
	DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(LocalRotationTransform, LocalPositionTransform);
	DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&node->parent->worldTransform);
	DirectX::XMMATRIX WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
	DirectX::XMStoreFloat4x4(&node->worldTransform, WorldTransform);

	for (Model::Node* child : node->children)
	{
		ComputeWorldTransform(child);
	}
}

// ���ƎO�p�`�Ƃ̌����𔻒肷��
bool CharacterControlScene::SphereIntersectTriangle(
	const DirectX::XMFLOAT3& sphereCenter,
	const float sphereRadius,
	const DirectX::XMFLOAT3& triangleVertexA,
	const DirectX::XMFLOAT3& triangleVertexB,
	const DirectX::XMFLOAT3& triangleVertexC,
	DirectX::XMFLOAT3& hitPosition,
	DirectX::XMFLOAT3& hitNormal)
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
	if (DirectX::XMVector3Equal(TriangleNormal, DirectX::XMVectorZero()))
	{
		return false;
	}

	// ���ʂƋ��̋��������߂�
	DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&sphereCenter);
	DirectX::XMVECTOR Dot1 = DirectX::XMVector3Dot(TriangleNormal, SphereCenter);
	DirectX::XMVECTOR Dot2 = DirectX::XMVector3Dot(TriangleNormal, TriangleVertex[0]);
	DirectX::XMVECTOR Distance = DirectX::XMVectorSubtract(Dot1, Dot2);
	float distance = DirectX::XMVectorGetX(Distance);

	// ���a�ȏ㗣��Ă���Γ�����Ȃ�
	if (distance > sphereRadius)
	{
		return false;
	}

	// ���̒��S���ʂɐڂ��Ă��邩�A���̕����ɂ���ꍇ�͓�����Ȃ�
	if (distance < 0.0f)
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
	return false;
}

// ���ƃ��f���Ƃ̌����𔻒肷��
bool CharacterControlScene::SphereIntersectModel(
	const DirectX::XMFLOAT3& sphereCenter,
	const float sphereRadius,
	const Model* model,
	std::vector<HitResult>& hits)
{
	hits.clear();

	DirectX::BoundingSphere sphere;
	sphere.Center = sphereCenter;
	sphere.Radius = sphereRadius;

	for (const Model::Mesh& mesh : model->GetMeshes())
	{
		// �o�E���f�B���O�{�b�N�X�Ɣ��肷��
	//	if (!sphere.Intersects(mesh.worldBounds)) continue;

		// ���b�V���̃X�P�[�����O�ɂ���ď����𕪊򂷂�
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		float lengthSqAxisX = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[0]));
		float lengthSqAxisY = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[1]));
		float lengthSqAxisZ = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[2]));
		if (lengthSqAxisX == lengthSqAxisY && lengthSqAxisY == lengthSqAxisZ)
		{
			// ���̃��[�J����ԕϊ�
			float length = sqrtf(lengthSqAxisX);
			DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
			DirectX::XMVECTOR WorldSphereCenter = DirectX::XMLoadFloat3(&sphere.Center);
			DirectX::XMVECTOR LocalSphereCenter = DirectX::XMVector3Transform(WorldSphereCenter, InverseWorldTransform);
			
			DirectX::BoundingSphere localSphere;
			DirectX::XMStoreFloat3(&localSphere.Center, LocalSphereCenter);
			localSphere.Radius = sphereRadius / length;

			// ���[�J����Ԃł̋��ƎO�p�`�̏Փˏ���
			for (size_t i = 0; i < mesh.indices.size(); i += 3)
			{
				uint32_t indexA = mesh.indices.at(i + 0);
				uint32_t indexB = mesh.indices.at(i + 1);
				uint32_t indexC = mesh.indices.at(i + 2);

				const Model::Vertex& vertexA = mesh.vertices.at(indexA);
				const Model::Vertex& vertexB = mesh.vertices.at(indexB);
				const Model::Vertex& vertexC = mesh.vertices.at(indexC);

				// �o�E���f�B���O�{�b�N�X�Ɣ��肷��
				//DirectX::BoundingBox box;
				//DirectX::BoundingBox::CreateFromPoints(box, 3, &vertexA.position, sizeof(ModelResource::Vertex));
				//if (!localSphere.Intersects(box)) continue;

				// ���ƎO�p�`�̏Փˏ���
				DirectX::XMFLOAT3 localHitPosition, localHitNormal;
				if (SphereIntersectTriangle(
					localSphere.Center, localSphere.Radius,
					vertexA.position, vertexB.position, vertexC.position,
					localHitPosition, localHitNormal))
				{
					// ���[���h��ԕϊ�
					DirectX::XMVECTOR LocalHitPosition = DirectX::XMLoadFloat3(&localHitPosition);
					DirectX::XMVECTOR LocalHitNormal = DirectX::XMLoadFloat3(&localHitNormal);
					DirectX::XMVECTOR WorldHitPosition = DirectX::XMVector3Transform(LocalHitPosition, WorldTransform);
					DirectX::XMVECTOR WorldHitNormal = DirectX::XMVector3TransformNormal(LocalHitNormal, WorldTransform);

					HitResult& hit = hits.emplace_back();
					DirectX::XMStoreFloat3(&hit.position, WorldHitPosition);
					DirectX::XMStoreFloat3(&hit.normal, DirectX::XMVector3Normalize(WorldHitNormal));
					sphere.Center = hit.position;
				}

			}
		}
		else
		{
			// ���[���h��Ԃł̋��ƎO�p�`�̏Փˏ���
			for (size_t i = 0; i < mesh.indices.size(); i += 3)
			{
				uint32_t indexA = mesh.indices.at(i + 0);
				uint32_t indexB = mesh.indices.at(i + 1);
				uint32_t indexC = mesh.indices.at(i + 2);

				const Model::Vertex& vertexA = mesh.vertices.at(indexA);
				const Model::Vertex& vertexB = mesh.vertices.at(indexB);
				const Model::Vertex& vertexC = mesh.vertices.at(indexC);

				// ���_���W�̃��[���h��ԕϊ�
				DirectX::XMVECTOR LocalPositionA = DirectX::XMLoadFloat3(&vertexA.position);
				DirectX::XMVECTOR LocalPositionB = DirectX::XMLoadFloat3(&vertexB.position);
				DirectX::XMVECTOR LocalPositionC = DirectX::XMLoadFloat3(&vertexC.position);

				DirectX::XMVECTOR WorldPositionA = DirectX::XMVector3Transform(LocalPositionA, WorldTransform);
				DirectX::XMVECTOR WorldPositionB = DirectX::XMVector3Transform(LocalPositionB, WorldTransform);
				DirectX::XMVECTOR WorldPositionC = DirectX::XMVector3Transform(LocalPositionC, WorldTransform);

				DirectX::XMFLOAT3 worldPositionA, worldPositionB, worldPositionC;
				DirectX::XMStoreFloat3(&worldPositionA, WorldPositionA);
				DirectX::XMStoreFloat3(&worldPositionB, WorldPositionB);
				DirectX::XMStoreFloat3(&worldPositionC, WorldPositionC);

				// ���ƎO�p�`�̏Փˏ���
				DirectX::XMFLOAT3 worldHitPosition, worldHitNormal;
				if (SphereIntersectTriangle(
					sphere.Center, sphere.Radius,
					worldPositionA, worldPositionB, worldPositionC,
					worldHitPosition, worldHitNormal))
				{
					HitResult& hit = hits.emplace_back();
					hit.position = worldHitPosition;
					hit.normal = worldHitNormal;
					sphere.Center = hit.position;
				}
			}
		}
	}

	return hits.size() > 0;
}

// ���C�ƃ��f���Ƃ̌����𔻒肷��
bool CharacterControlScene::RayIntersectModel(
	const DirectX::XMFLOAT3& rayStart,
	const DirectX::XMFLOAT3& rayEnd,
	const Model* model,
	HitResult& hitResult)
{
	DirectX::XMVECTOR WorldRayStart = DirectX::XMLoadFloat3(&rayStart);
	DirectX::XMVECTOR WorldRayEnd = DirectX::XMLoadFloat3(&rayEnd);
	DirectX::XMVECTOR WorldRayVec = DirectX::XMVectorSubtract(WorldRayEnd, WorldRayStart);
	DirectX::XMVECTOR WorldRayDirection = DirectX::XMVector3Normalize(WorldRayVec);
	float worldRayLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(WorldRayVec));
	float nearestWorldHitLength = worldRayLength;

	bool hit = false;
	for (const Model::Mesh& mesh : model->GetMeshes())
	{
		// �o�E���f�B���O�{�b�N�X�Ɣ��肷��
		//float length = worldRayLength;
		//if (!mesh.worldBounds.Intersects(WorldRayStart, WorldRayDirection, length)) continue;

		// ���C�̃��[�J����ԕϊ�
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
		DirectX::XMVECTOR LocalRayStart = DirectX::XMVector3Transform(WorldRayStart, InverseWorldTransform);
		DirectX::XMVECTOR LocalRayVec = DirectX::XMVector3TransformNormal(WorldRayVec, InverseWorldTransform);
		DirectX::XMVECTOR LocalRayDirection = DirectX::XMVector3Normalize(LocalRayVec);
		float localRayLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(LocalRayVec));

		// ���[�J����Ԃł̋��ƎO�p�`�̏Փˏ���
		for (size_t i = 0; i < mesh.indices.size(); i += 3)
		{
			uint32_t indexA = mesh.indices.at(i + 0);
			uint32_t indexB = mesh.indices.at(i + 1);
			uint32_t indexC = mesh.indices.at(i + 2);

			const Model::Vertex& vertexA = mesh.vertices.at(indexA);
			const Model::Vertex& vertexB = mesh.vertices.at(indexB);
			const Model::Vertex& vertexC = mesh.vertices.at(indexC);

			DirectX::XMVECTOR LocalPositionA = DirectX::XMLoadFloat3(&vertexA.position);
			DirectX::XMVECTOR LocalPositionB = DirectX::XMLoadFloat3(&vertexB.position);
			DirectX::XMVECTOR LocalPositionC = DirectX::XMLoadFloat3(&vertexC.position);

			// ���C�ƎO�p�`�̌�������
			float localRayDist = localRayLength;
			if (DirectX::TriangleTests::Intersects(LocalRayStart, LocalRayDirection, LocalPositionA, LocalPositionB, LocalPositionC, localRayDist))
			{
				// �����ʒu�����߂�
				DirectX::XMVECTOR LocalHitVec = DirectX::XMVectorScale(LocalRayDirection, localRayDist);
				DirectX::XMVECTOR LocalHitPosition = DirectX::XMVectorAdd(LocalRayStart, LocalHitVec);

				// ���[���h��ԕϊ�
				DirectX::XMVECTOR WorldHitPosition = DirectX::XMVector3Transform(LocalHitPosition, WorldTransform);
				DirectX::XMVECTOR WorldHitVec = DirectX::XMVectorSubtract(WorldHitPosition, WorldRayStart);

				// �ŋߔ���
				float worldHitLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(WorldHitVec));
				if (worldHitLength < nearestWorldHitLength)
				{
					nearestWorldHitLength = worldHitLength;

					// �ʖ@�������߂�
					DirectX::XMVECTOR EdgeAB = DirectX::XMVectorSubtract(LocalPositionB, LocalPositionA);
					DirectX::XMVECTOR EdgeBC = DirectX::XMVectorSubtract(LocalPositionC, LocalPositionB);
					DirectX::XMVECTOR LocalHitNormal = DirectX::XMVector3Cross(EdgeAB, EdgeBC);

					// ���[���h��ԕϊ�
					DirectX::XMVECTOR WorldHitNormal = DirectX::XMVector3TransformNormal(LocalHitNormal, WorldTransform);

					// �f�[�^�i�[
					DirectX::XMStoreFloat3(&hitResult.position, WorldHitPosition);
					DirectX::XMStoreFloat3(&hitResult.normal, DirectX::XMVector3Normalize(WorldHitNormal));

					hit = true;
				}
			}
		}
	}
	return hit;
}
