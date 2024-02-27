#include <algorithm>
#include <functional>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXCollision.h>
#include "Graphics.h"
#include "TransformUtils.h"
#include "Scene/CharacterControlScene.h"

// コンストラクタ
CharacterControlScene::CharacterControlScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();

	// カメラセットアップ
	SetupCamera();

	// ステージセットアップ
	SetupStage(device);

	// ボールセットアップ
	SetupBalls(device);

	// ユニティちゃんセットアップ
	SetupUnityChan(device);

}

// 更新処理
void CharacterControlScene::Update(float elapsedTime)
{
	elapsedTime *= timeScale;

	// 入力更新処理
	UpdateInput();

	// カメラ更新処理
	if (useFreeCamera)
	{
		cameraController.Update();
		cameraController.SyncControllerToCamera(camera);
	}
	else
	{
		UpdateThirdPersonCamera(elapsedTime);
	}

	// ボール更新処理
	UpdateBalls(elapsedTime);

	// ユニティちゃん更新処理
	UpdateUnityChan(elapsedTime);
}

// 描画処理
void CharacterControlScene::Render(float elapsedTime)
{
	elapsedTime *= timeScale;

	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();
	ModelRenderer* modelRenderer = Graphics::Instance().GetModelRenderer();

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// グリッド描画
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// 描画コンテキスト設定
	RenderContext rc;
	rc.camera = &camera;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.lightManager = &lightManager;

	// モデル描画
	for (Ball& ball : balls)
	{
		modelRenderer->Draw(ShaderId::Lambert, ball.model);
	}
	modelRenderer->Draw(ShaderId::Lambert, stage.model);
	modelRenderer->Draw(ShaderId::Basic, unitychan.model);
	modelRenderer->Draw(ShaderId::Basic, unitychan.staff);
	modelRenderer->Render(rc);

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// エフェクト描画
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

	// レンダーステート設定
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// 物理ボーン描画
	{
		auto drawPhysicsBones = [&](const std::vector<PhysicsBone>& bones)
		{
			for (size_t i = 0; i < bones.size() - 1; ++i)
			{
				const PhysicsBone& bone = bones[i];
				const PhysicsBone& child = bones[i + 1];

				DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&bone.worldTransform);

				// 軸描画
				float length = child.localPosition.x;
				World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[0]), length);
				World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[1]), length);
				World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[2]), length);
				DirectX::XMFLOAT4X4 world;
				DirectX::XMStoreFloat4x4(&world, World);
				primitiveRenderer->DrawAxis(world, { 1, 1, 0, 1 });

				// ボーン描画
				World = DirectX::XMLoadFloat4x4(&bone.worldTransform);
				DirectX::XMMATRIX Offset = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(90));
				World = DirectX::XMMatrixMultiply(Offset, World);
				DirectX::XMStoreFloat4x4(&world, World);
				shapeRenderer->DrawBone(world, length, { 1, 1, 0, 1 });

				DirectX::XMMATRIX ChildWorld = DirectX::XMLoadFloat4x4(&child.worldTransform);
				// コリジョン描画
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

	// コリジョン描画
	{
		auto drawCollisionBones = [&](const std::vector<CollisionBone>& bones)
		{
			for (const CollisionBone& bone : bones)
			{
				DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);

				// 軸描画
				World.r[0] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[0]), bone.radius);
				World.r[1] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[1]), bone.radius);
				World.r[2] = DirectX::XMVectorScale(DirectX::XMVector3Normalize(World.r[2]), bone.radius);
				DirectX::XMFLOAT4X4 world;
				DirectX::XMStoreFloat4x4(&world, World);
				primitiveRenderer->DrawAxis(world, { 1, 1, 0, 1 });
				// 球描画
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

// GUI描画処理
void CharacterControlScene::DrawGUI()
{
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 700), ImGuiCond_Once);

	if (ImGui::Begin(u8"キャラクター制御"))
	{
		if (ImGui::CollapsingHeader(u8"グローバル", ImGuiTreeNodeFlags_DefaultOpen))
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
		if (ImGui::CollapsingHeader(u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen))
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

		if (ImGui::CollapsingHeader(u8"ユニティちゃん", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(&unitychan);

			ImGui::Separator();
			ImGui::Text(u8"基本");
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
			ImGui::Text(u8"移動関連");
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
			ImGui::Text(u8"モニタリング");
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
			if (ImGui::Checkbox(u8"物理ボーン", &visiblePhysicsBone))
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
						// 表の描画
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
			if (ImGui::Checkbox(u8"コリジョンボーン", &visibleCollisionBone))
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
						// 表の描画
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
			ImGui::Text(u8"ノードツリー");
			ImGui::Separator();
			{
				// ノードツリーを再帰的に描画する関数
				std::function<void(Model::Node*)> drawNodeTree = [&](Model::Node* node)
				{
					// 矢印をクリック、またはノードをダブルクリックで階層を開く
					ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
						| ImGuiTreeNodeFlags_OpenOnDoubleClick;

					// 子がいない場合は矢印をつけない
					size_t childCount = node->children.size();
					if (childCount == 0)
					{
						nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					}

					// 選択フラグ
					if (selectionNode == node)
					{
						nodeFlags |= ImGuiTreeNodeFlags_Selected;
					}

					// ツリーノードを表示
					bool opened = ImGui::TreeNodeEx(node, nodeFlags, node->name.c_str());

					// フォーカスされたノードを選択する
					if (ImGui::IsItemFocused())
					{
						selectionNode = node;
					}

					if (selectionNode == node)
					{
						// 位置
						ImGui::DragFloat3("Position", &selectionNode->position.x, 0.1f);

						// 回転
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

						// スケール
						ImGui::DragFloat3("Scale", &selectionNode->scale.x, 0.01f);
					}

					// 開かれている場合、子階層も同じ処理を行う
					if (opened && childCount > 0)
					{
						for (Model::Node* child : node->children)
						{
							drawNodeTree(child);
						}
						ImGui::TreePop();
					}
				};
				// 再帰的にノードを描画
				if (unitychan.model != nullptr)
				{
					drawNodeTree(unitychan.model->GetRootNode());
				}
			}
			ImGui::PopID();
		}
		if (ImGui::CollapsingHeader(u8"ボール", ImGuiTreeNodeFlags_DefaultOpen))
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

// 入力更新処理
void CharacterControlScene::UpdateInput()
{
	// 左スティック入力
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
	// 右スティック
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

	// キー入力
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

// カメラセットアップ
void CharacterControlScene::SetupCamera()
{
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	thirdPersonCamera.eye = { 20, 2, 20 };
	thirdPersonCamera.focus = { 15, 1, 15 };
	thirdPersonCamera.angle.x = DirectX::XMConvertToRadians(20);

	// カメラ設定
	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		screenWidth / screenHeight,			// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	camera.SetLookAt(
		thirdPersonCamera.eye,		// 視点
		thirdPersonCamera.focus,	// 注視点
		{ 0, 1, 0 }					// 上ベクトル
	);
	cameraController.SyncCameraToController(camera);
}

// 三人称視点カメラ更新処理
void CharacterControlScene::UpdateThirdPersonCamera(float elapsedTime)
{
	thirdPersonCamera.focus = unitychan.position;
	thirdPersonCamera.focus.y += 1.1f;

	// 注視点
	DirectX::XMFLOAT3 cameraFocus = camera.GetFocus();
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&thirdPersonCamera.focus);
	DirectX::XMVECTOR CameraFocus = DirectX::XMLoadFloat3(&cameraFocus);
	float moveSpeed = thirdPersonCamera.moveSpeed * elapsedTime;
	CameraFocus = DirectX::XMVectorLerp(CameraFocus, Focus, std::clamp(moveSpeed, 0.0f, 1.0f));
	DirectX::XMStoreFloat3(&cameraFocus, CameraFocus);

	// Y軸角度算出
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&thirdPersonCamera.eye);
	DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Focus, Eye);
	Vec = DirectX::XMVectorSetY(Vec, 0);
	Vec = DirectX::XMVector3Normalize(Vec);
	DirectX::XMFLOAT3 vec;
	DirectX::XMStoreFloat3(&vec, Vec);
	thirdPersonCamera.angle.y = atan2f(vec.x, vec.z);

	// 回転
	float rotateSpeed = thirdPersonCamera.rotateSpeed * elapsedTime;
	thirdPersonCamera.angle.x += thirdPersonCamera.inputAxisY * rotateSpeed;
	thirdPersonCamera.angle.y += thirdPersonCamera.inputAxisX * rotateSpeed;

	// 角度制限
	thirdPersonCamera.angle.x = std::clamp(thirdPersonCamera.angle.x, thirdPersonCamera.minPitch, thirdPersonCamera.maxPitch);

	// 視点
	DirectX::XMMATRIX Transform = DirectX::XMMatrixRotationRollPitchYaw(thirdPersonCamera.angle.x, thirdPersonCamera.angle.y, thirdPersonCamera.angle.z);
	DirectX::XMVECTOR Front = Transform.r[2];
	Eye = DirectX::XMVectorSubtract(Focus, DirectX::XMVectorScale(Front, thirdPersonCamera.focusRange));
	DirectX::XMStoreFloat3(&thirdPersonCamera.eye, Eye);

	DirectX::XMFLOAT3 cameraEye = camera.GetEye();
	DirectX::XMVECTOR CameraEye = DirectX::XMLoadFloat3(&cameraEye);
	CameraEye = DirectX::XMVectorLerp(CameraEye, Eye, 0.1f);
	DirectX::XMStoreFloat3(&cameraEye, CameraEye);

	// コリジョン
	HitResult hit;
	if (RayIntersectModel(cameraFocus, cameraEye, stage.model.get(), hit))
	{
		cameraEye = hit.position;
	}

	camera.SetLookAt(cameraEye, cameraFocus, { 0, 1, 0 });
}

// ステージセットアップ
void CharacterControlScene::SetupStage(ID3D11Device* device)
{
	// モデル読み込み
	stage.model = std::make_shared<Model>(device, "Data/Model/Greybox/Greybox.glb", 1.0f);
}

// ボールセットアップ
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

// ボール行列更新処理
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

// ユニティちゃんセットアップ
void CharacterControlScene::SetupUnityChan(ID3D11Device* device)
{
	// モデル読み込み
	unitychan.model = std::make_shared<Model>(device, "Data/Model/unitychan/unitychan.glb");
	unitychan.model->GetNodePoses(unitychan.nodePoses);
	unitychan.model->GetNodePoses(unitychan.cacheNodePoses);
	unitychan.rootMotionNodeIndex = unitychan.model->GetNodeIndex("Character1_Hips");
	unitychan.hipsNodeIndex = unitychan.model->GetNodeIndex("Character1_Hips");
	unitychan.position = { 15, 0.5f, 15 };
	PlayUnityChanAnimation("Idle", 0, true, 0);

	unitychan.staff = std::make_shared<Model>(device, "Data/Model/Weapon/Staff.glb");

	// ノード検索
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

	// ルックアットボーン
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

	// 物理ボーンセットアップ
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

	// コリジョンボーンセットアップ
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

	// IKボーンセットアップ
	unitychan.leftFootIKBone.thighNode = findNode("Character1_LeftUpLeg");
	unitychan.leftFootIKBone.legNode = findNode("Character1_LeftLeg");
	unitychan.leftFootIKBone.footNode = findNode("Character1_LeftFoot");
	unitychan.leftFootIKBone.toeNode = findNode("Character1_LeftToeBase");
	unitychan.rightFootIKBone.thighNode = findNode("Character1_RightUpLeg");
	unitychan.rightFootIKBone.legNode = findNode("Character1_RightLeg");
	unitychan.rightFootIKBone.footNode = findNode("Character1_RightFoot");
	unitychan.rightFootIKBone.toeNode = findNode("Character1_RightToeBase");
}

// ユニティちゃん更新処理
void CharacterControlScene::UpdateUnityChan(float elapsedTime)
{
	// アニメーション更新処理
	UpdateUnityChanAnimation(elapsedTime);

	// ステートマシン更新処理
	UpdateUnityChanStateMachine(elapsedTime);

	// ベロシティ更新処理
	UpdateUnityChanVelocity(elapsedTime);

	// 位置更新処理
	UpdateUnityChanPosition(elapsedTime);

	// 行列更新処理
	UpdateUnityChanTransform();

	// エフェクト更新処理
	UpdateUnityChanEffect();

	// ルックアット更新処理
	UpdateUnityChanLookAt(elapsedTime);

	// IKボーン更新処理
	UpdateUnityChanIKBones();

	// 物理ボーン更新処理
	UpdateUnityChanPhysicsBones(elapsedTime);
}

// ユニティちゃんアニメーション更新処理
void CharacterControlScene::UpdateUnityChanAnimation(float elapsedTime)
{
	// 指定時間のアニメーション
	if (unitychan.animationIndex >= 0)
	{
		// アニメーション切り替え時に現在の姿勢をキャッシュ
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

		// アニメーション計算
		unitychan.model->ComputeAnimation(unitychan.animationIndex, unitychan.animationSeconds, unitychan.nodePoses);

		// ルートモーション計算
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

		// アニメーション時間更新
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

		// ブレンド率の計算
		if (unitychan.animationBlendSecondsLength > 0.0f)
		{
			float blendRate = unitychan.animationBlendSeconds / unitychan.animationBlendSecondsLength;

			// ブレンド計算
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
			// ブレンド時間更新
			unitychan.animationBlendSeconds += elapsedTime;
			if (unitychan.animationBlendSeconds >= unitychan.animationBlendSecondsLength)
			{
				unitychan.animationBlendSecondsLength = 0.0f;
			}
		}
	}

	// ノード
	unitychan.model->SetNodePoses(unitychan.nodePoses);
}

// ユニティちゃんステートマシン更新処理
void CharacterControlScene::UpdateUnityChanStateMachine(float elapsedTime)
{
	// ステート切り替え処理
	if (unitychan.state != unitychan.nextState)
	{
		UnityChanStateExit(elapsedTime);

		unitychan.state = unitychan.nextState;

		UnityChanStateEnter(elapsedTime);
	}
	// ステート更新処理
	UnityChanStateUpdate(elapsedTime);
}

// ユニティちゃんベロシティ更新処理
void CharacterControlScene::UpdateUnityChanVelocity(float elapsedTime)
{
	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	// 重力処理
	if (unitychan.onGround)
	{
		unitychan.velocity.y -= 0.001f;
	}
	else
	{
		unitychan.velocity.y -= gravity * elapsedFrame;
	}

	// 水平速力の減速
	{
		float length = sqrtf(unitychan.velocity.x * unitychan.velocity.x + unitychan.velocity.z * unitychan.velocity.z);
		if (length > 0.0f)
		{
			// 摩擦力
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

// ユニティちゃん位置更新処理
void CharacterControlScene::UpdateUnityChanPosition(float elapsedTime)
{
	float elapsedFrame = ConvertToGameFrame(elapsedTime);

	// 移動処理
	{
		// レイキャストで壁抜けが発生しないようにする
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

		// 移動値リセット
		unitychan.deltaMove = { 0, 0, 0 };
	}

	// ステージとの衝突処理
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
				// すべての衝突結果を平均化した値を採用する
				DirectX::XMFLOAT3 hitPosition = { 0, 0, 0 };
				DirectX::XMFLOAT3 hitGroundNormal = { 0, 0, 0 };
				int hitGroundCount = 0;
				for (HitResult& hit : unitychan.hits)
				{
					// 傾斜が４５度以下のポリゴンと衝突した場合は地面とする
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
				// 衝突した座標を平均化
				float f = 1.0f / static_cast<float>(unitychan.hits.size());
				unitychan.position.x = (hitPosition.x * f);
				unitychan.position.y = (hitPosition.y * f) - unitychan.radius;
				unitychan.position.z = (hitPosition.z * f);

				// 衝突した地面法線を平均化
				if (hitGroundCount > 0)
				{
					f = 1.0f / static_cast<float>(hitGroundCount);
					unitychan.groundNormal.x = hitGroundNormal.x * f;
					unitychan.groundNormal.y = hitGroundNormal.y * f;
					unitychan.groundNormal.z = hitGroundNormal.z * f;
				}
			}
			// 前フレームに接地状態で今フレームで浮いている場合は判定を下げてチェックする
			// ※スロープや階段で浮かないようにするため
			if (!onGround || unitychan.onGround || unitychan.velocity.y > 0)
			{
				break;
			}
			position.y -= groundAdjust / (n - 1);
		}

		// 接地していない場合は地面法線を真上方向にする
		if (!unitychan.onGround)
		{
			unitychan.groundNormal = { 0, 1, 0 };
		}
	}
}

// ユニティちゃん行列更新処理
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

// ユニティちゃんエフェクト更新処理
void CharacterControlScene::UpdateUnityChanEffect()
{
	UnityChanStaffTrail();
}

// ユニティちゃんルックアット更新処理
void CharacterControlScene::UpdateUnityChanLookAt(float elapsedTime)
{
	// ターゲット座標を算出（デフォルトは前方100メートル先）
	DirectX::XMMATRIX RootWorldTransform = DirectX::XMLoadFloat4x4(&unitychan.transform);
	DirectX::XMVECTOR RootWorldForward = RootWorldTransform.r[2];
	DirectX::XMMATRIX HeadWorldTransform = DirectX::XMLoadFloat4x4(&unitychan.lookAtBone.node->worldTransform);
	DirectX::XMVECTOR HeadWorldPosition = HeadWorldTransform.r[3];
	DirectX::XMVECTOR TargetWorldPosition = DirectX::XMVectorAdd(HeadWorldPosition, DirectX::XMVectorScale(RootWorldForward, 100));
	// 一番近いボールを検索
	float minLength = 5.0f;
	for (const Ball& ball : balls)
	{
		DirectX::XMVECTOR BallWorldPosition = DirectX::XMLoadFloat3(&ball.position);
		DirectX::XMVECTOR WorldVec = DirectX::XMVectorSubtract(BallWorldPosition, HeadWorldPosition);
		DirectX::XMVECTOR Length = DirectX::XMVector3Length(WorldVec);
		float length = DirectX::XMVectorGetX(Length);
		if (length < minLength)
		{
			// 前後判定
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
	// 補完処理
	float elapsedFrame = ConvertToGameFrame(elapsedTime);
	DirectX::XMVECTOR WorldLookPosition = DirectX::XMLoadFloat3(&unitychan.lookAtBone.target);
	WorldLookPosition = DirectX::XMVectorLerp(WorldLookPosition, TargetWorldPosition, elapsedFrame * 0.2f);
	DirectX::XMStoreFloat3(&unitychan.lookAtBone.target, WorldLookPosition);
	// ローカル空間変換
	DirectX::XMMATRIX InverseHeadWorldTransform = DirectX::XMMatrixInverse(nullptr, HeadWorldTransform);
	DirectX::XMVECTOR LocalVec = DirectX::XMVector3Transform(WorldLookPosition, InverseHeadWorldTransform);
	DirectX::XMVECTOR LocalDirection = DirectX::XMVector3Normalize(LocalVec);
	DirectX::XMVECTOR LocalForward = DirectX::XMLoadFloat3(&unitychan.lookAtBone.forward);

	// ローカル空間で回転処理
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

// ユニティちゃんIKボーン更新処理
void CharacterControlScene::UpdateUnityChanIKBones()
{
	// 足IK処理
	bool processedFootIK = false;
	if (unitychan.applyFootIK)
	{
		float maxOffset = 0.25f;	// 腰骨を下げる最大幅
		// 地面に向けてレイキャスト
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

			// 踵から地面に向けてレイキャスト
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

			// 踵の最終的な位置を計算する
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

			// 腰骨を下げ幅を計算する
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

			// 腰骨の位置を調整する
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

			// IK処理
			auto computeFootIK = [](FootIKBone& bone)
			{
				if (!bone.hit) return;

				// ポールターゲット位置を算出
				DirectX::XMMATRIX LegWorldTransform = DirectX::XMLoadFloat4x4(&bone.legNode->worldTransform);
				DirectX::XMVECTOR PoleWorldPosition = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0.1f, 0, 0), LegWorldTransform);
				DirectX::XMFLOAT3 poleWorldPosition;
				DirectX::XMStoreFloat3(&poleWorldPosition, PoleWorldPosition);

				// IK処理
				ComputeTwoBoneIK(bone.thighNode, bone.legNode, bone.footNode, bone.ankleTarget, poleWorldPosition);

				// 足の回転を地面に合わせる
				DirectX::XMMATRIX Leg = DirectX::XMLoadFloat4x4(&bone.legNode->worldTransform);
				DirectX::XMMATRIX Foot = DirectX::XMLoadFloat4x4(&bone.footNode->worldTransform);
				DirectX::XMMATRIX Toe = DirectX::XMLoadFloat4x4(&bone.toeNode->worldTransform);

				// 地面の法線ベクトルを参考に足のワールド行列を直接編集する
				Foot.r[1] = DirectX::XMLoadFloat3(&bone.hitResult.normal);
				Foot.r[0] = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Foot.r[3], Toe.r[3]));
				Foot.r[2] = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Foot.r[0], Foot.r[1]));
				Foot.r[0] = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Foot.r[1], Foot.r[2]));

				// オフセット回転
				DirectX::XMMATRIX Offset = DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(60.0f));
				Foot = DirectX::XMMatrixMultiply(Offset, Foot);

				// ローカル空間変換
				DirectX::XMMATRIX InverseLegWorldTransform = DirectX::XMMatrixInverse(nullptr, Leg);
				DirectX::XMMATRIX FootLocalTransform = DirectX::XMMatrixMultiply(Foot, InverseLegWorldTransform);
				FootLocalTransform.r[0] = DirectX::XMVector3Normalize(FootLocalTransform.r[0]);
				FootLocalTransform.r[1] = DirectX::XMVector3Normalize(FootLocalTransform.r[1]);
				FootLocalTransform.r[2] = DirectX::XMVector3Normalize(FootLocalTransform.r[2]);
				DirectX::XMVECTOR FootLocalRotation = DirectX::XMQuaternionRotationMatrix(FootLocalTransform);
				DirectX::XMStoreFloat4(&bone.footNode->rotation, FootLocalRotation);

				// ワールド行列計算
				ComputeWorldTransform(bone.footNode);

			};
			computeFootIK(unitychan.leftFootIKBone);
			computeFootIK(unitychan.rightFootIKBone);
			processedFootIK = true;
		}
	}
	// IK処理していないときは腰骨調整値をもとに戻す
	if (!processedFootIK)
	{
		DirectX::XMVECTOR HipsOffset = DirectX::XMLoadFloat3(&unitychan.hipsOffset);
		HipsOffset = DirectX::XMVectorLerp(HipsOffset, DirectX::XMVectorZero(), 0.1f);
		DirectX::XMStoreFloat3(&unitychan.hipsOffset, HipsOffset);
	}
}

// ユニティちゃん物理ボーン更新処理
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

// ユニティちゃんアニメーション再生
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

// ユニティちゃんステート切り替え
void CharacterControlScene::ChangeUnityChanState(UnityChan::State state)
{
	unitychan.nextState = state;
}

// ユニティちゃんステート開始処理
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

// ユニティちゃんステート開始処理
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

// ユニティちゃんステート終了処理
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

// ユニティちゃん入力移動処理
void CharacterControlScene::UnityChanInputMovement(float elapsedTime)
{
	// カメラ情報
	const DirectX::XMFLOAT3& cameraFront = camera.GetFront();
	const DirectX::XMFLOAT3& cameraRight = camera.GetRight();
	float cameraFrontLengthXZ = sqrtf(cameraFront.x * cameraFront.x + cameraFront.z * cameraFront.z);
	float cameraRightLengthXZ = sqrtf(cameraRight.x * cameraRight.x + cameraRight.z * cameraRight.z);
	float cameraFrontX = cameraFront.x / cameraFrontLengthXZ;
	float cameraFrontZ = cameraFront.z / cameraFrontLengthXZ;
	float cameraRightX = cameraRight.x / cameraRightLengthXZ;
	float cameraRightZ = cameraRight.z / cameraRightLengthXZ;

	// 旋回
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
		// 空中にいるときは加速力を減らす
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

	// 移動
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
		// 加速処理
		float length = sqrtf(unitychan.velocity.x * unitychan.velocity.x + unitychan.velocity.z * unitychan.velocity.z);
		if (length <= unitychan.moveSpeed)
		{
			float elapsedFrame = elapsedTime * 60.0f;
			float acceleration = unitychan.acceleration * elapsedFrame;
			float moveSpeed = unitychan.moveSpeed;
			// 空中にいるときは加速力を減らす
			if (!unitychan.onGround)
			{
				acceleration *= unitychan.airControl;
				moveSpeed *= 0.8f;
			}
			// 速度制限
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

			// 傾斜対応
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

// ユニティちゃん入力移動処理
void CharacterControlScene::UnityChanInputJump()
{
}

// ユニティちゃん空中処理
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
			// 地面に向けてレイキャスト
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

// ユニティちゃんコンボ入力処理
void CharacterControlScene::UnityChanInputCombo()
{
	// 各ステート毎の次のコンボへシフトするフレームを取得
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
		// シフトフレームまでにキー入力をすればコンボ
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
		// 次のステートへ
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

// ユニティちゃんスタッフトレール処理
void CharacterControlScene::UnityChanStaffTrail()
{
	// 保存していた座標をずらす
	for (int i = 0; i < 2; ++i)
	{
		for (int j = _countof(unitychan.trailPositions[i]) - 1; j > 0; --j)
		{
			unitychan.trailPositions[i][j] = unitychan.trailPositions[i][j - 1];
		}
	}

	// 剣の根本と先端から座標を取得
	DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&unitychan.staff->GetNodes().at(0).worldTransform);
	DirectX::XMVECTOR Root = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, 0.6f, 0), WorldTransform);
	DirectX::XMVECTOR Tip = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, 0.9f, 0), WorldTransform);
	DirectX::XMStoreFloat3(&unitychan.trailPositions[0][0], Root);
	DirectX::XMStoreFloat3(&unitychan.trailPositions[1][0], Tip);
}

// ルートモーション処理
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
	// 初回、前回、今回のルートモーションノードの姿勢を取得
	Model::NodePose beginPose, oldPose, newPose;
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, 0, beginPose);
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, oldAnimationSeconds, oldPose);
	model->ComputeAnimation(animationIndex, rootMotionNodeIndex, newAnimationSeconds, newPose);

	// ローカル移動値を算出
	DirectX::XMFLOAT3 localTranslation;
	if (oldAnimationSeconds > newAnimationSeconds)
	{
		// ループ時処理
		const Model::Animation& animation = model->GetAnimations().at(animationIndex);
		Model::NodePose endPose;
		model->ComputeAnimation(animationIndex, rootMotionNodeIndex, animation.secondsLength, endPose);

		localTranslation.x = (endPose.position.x - oldPose.position.x) + (newPose.position.x - beginPose.position.x);
		localTranslation.y = (endPose.position.y - oldPose.position.y) + (newPose.position.y - beginPose.position.y);
		localTranslation.z = (endPose.position.z - oldPose.position.z) + (newPose.position.z - beginPose.position.z);
	}
	else
	{
		// 通常処理
		localTranslation.x = newPose.position.x - oldPose.position.x;
		localTranslation.y = newPose.position.y - oldPose.position.y;
		localTranslation.z = newPose.position.z - oldPose.position.z;
	}

	// グローバル移動値を算出
	const Model::Node& rootMotionNode = model->GetNodes().at(rootMotionNodeIndex);
	DirectX::XMVECTOR LocalTranslation = DirectX::XMLoadFloat3(&localTranslation);
	DirectX::XMMATRIX ParentGlobalTransform = DirectX::XMLoadFloat4x4(&rootMotionNode.parent->globalTransform);
	DirectX::XMVECTOR GlobalTranslation = DirectX::XMVector3TransformNormal(LocalTranslation, ParentGlobalTransform);
	DirectX::XMStoreFloat3(&translation, GlobalTranslation);

	if (bakeTranslationX || bakeTranslationY || bakeTranslationZ)
	{
		// 移動値を抜く
		if (bakeTranslationX) translation.x = 0;
		if (bakeTranslationY) translation.y = 0;
		if (bakeTranslationZ) translation.z = 0;
	
		// 今回の姿勢のワールド位置を算出
		DirectX::XMVECTOR LocalPosition = DirectX::XMLoadFloat3(&newPose.position);
		DirectX::XMVECTOR GlobalPosition = DirectX::XMVector3Transform(LocalPosition, ParentGlobalTransform);

		// XZ成分を削除
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

		// ローカル空間変換
		DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
		LocalPosition = DirectX::XMVector3Transform(GlobalPosition, InverseParentGlobalTransform);
		// ルートモーションノードの位置を設定
		DirectX::XMStoreFloat3(&rootMotionNodePosition, LocalPosition);
	}
	else
	{
		// ルートモーションノードの位置を初回の位置にする
		rootMotionNodePosition = beginPose.position;
	}
}

// 物理ボーン計算処理
void CharacterControlScene::ComputePhysicsBones(
	std::vector<PhysicsBone>& bones,
	const std::vector<CollisionBone>& collisionBones,
	const DirectX::XMFLOAT3& force,
	float maxVelocity)
{
	DirectX::XMVECTOR Force = DirectX::XMLoadFloat3(&force);

	// ルートボーンのワールド行列更新
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

		// 前回位置から今回位置の差分と重力による速力を算出する
		DirectX::XMVECTOR Velocity = DirectX::XMVectorSubtract(ChildWorldPosition, ChildOldWorldPosition);
		DirectX::XMVECTOR LengthSq = DirectX::XMVector3LengthSq(Velocity);
		if (DirectX::XMVectorGetX(LengthSq) > maxVelocity * maxVelocity)
		{
			Velocity = DirectX::XMVectorScale(DirectX::XMVector3Normalize(Velocity), maxVelocity);
		}
		Velocity = DirectX::XMVectorScale(Velocity, 0.9f);
		Velocity = DirectX::XMVectorAdd(Velocity, Force);

		// 子ボーンの位置を動かす
		ChildWorldPosition = DirectX::XMVectorAdd(ChildWorldPosition, Velocity);

		// コリジョン
		ComputeCollisionBones(collisionBones, ChildWorldPosition, child.collisionRadius);

		// デフォルトワールド行列
		DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&bone.node->parent->worldTransform);
		DirectX::XMMATRIX DefaultLocalTransform = DirectX::XMLoadFloat4x4(&bone.defaultLocalTransform);
		DirectX::XMMATRIX DefaultWorldTransform = DirectX::XMMatrixMultiply(DefaultLocalTransform, ParentWorldTransform);
		DirectX::XMFLOAT4X4 defaultWorldTransform;
		DirectX::XMStoreFloat4x4(&defaultWorldTransform, DefaultWorldTransform);
#if 0
		// 角度制限
		LimitBoneAngle(
			ConvertBoneWorldTransformAxis(defaultWorldTransform),
			ConvertBoneWorldTransformAxis(bone.worldTransform),
			bone.limitMinYaw, bone.limitMaxYaw,
			bone.limitMinPitch, bone.limitMaxPitch,
			ChildWorldPosition);
#endif
		// 最終的なボーンの方向
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.worldTransform);
		DirectX::XMVECTOR WorldPosition = WorldTransform.r[3];
		DirectX::XMVECTOR WorldTargetDirection = DirectX::XMVectorSubtract(ChildWorldPosition, WorldPosition);
		WorldTargetDirection = DirectX::XMVector3Normalize(WorldTargetDirection);
		// 初期状態の親のボーン方向
		DirectX::XMVECTOR ChildLocalPosition = DirectX::XMLoadFloat3(&child.localPosition);
		DirectX::XMVECTOR WorldDefaultDirection = DirectX::XMVector3TransformNormal(ChildLocalPosition, DefaultWorldTransform);
		WorldDefaultDirection = DirectX::XMVector3Normalize(WorldDefaultDirection);
		// 回転角度算出
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(WorldDefaultDirection, WorldTargetDirection);
		float angle = acosf(DirectX::XMVectorGetX(Dot));
		if (fabsf(angle) > 0.001f)
		{
			// 回転軸算出
			DirectX::XMVECTOR WorldAxis = DirectX::XMVector3Cross(WorldDefaultDirection, WorldTargetDirection);
			if (DirectX::XMVector3NotEqual(WorldAxis, DirectX::XMVectorZero()))
			{
				WorldAxis = DirectX::XMVector3Normalize(WorldAxis);

				// 回転軸をローカル空間変換
				DirectX::XMMATRIX DefaultInverseWorldTransform = DirectX::XMMatrixInverse(nullptr, DefaultWorldTransform);
				DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(WorldAxis, DefaultInverseWorldTransform);
				LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

				// 回転処理
				DirectX::XMMATRIX LocalRotationAxis = DirectX::XMMatrixRotationAxis(LocalAxis, angle);
				WorldTransform = DirectX::XMMatrixMultiply(LocalRotationAxis, DefaultWorldTransform);
				DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

				// ローカル空間変換
				DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixMultiply(WorldTransform, DefaultInverseWorldTransform);
				DirectX::XMVECTOR LocalRotation = DirectX::XMQuaternionRotationMatrix(LocalTransform);

				DirectX::XMVECTOR DefaultLocalRotation = DirectX::XMLoadFloat4(&bone.defaultLocalRotation);
				LocalRotation = DirectX::XMQuaternionSlerp(LocalRotation, DefaultLocalRotation, 0.1f);

				DirectX::XMStoreFloat4(&bone.localRotation, LocalRotation);
			}

			// 子ワールド行列更新
			DirectX::XMMATRIX ChildLocalPositionTransform = DirectX::XMMatrixTranslation(child.localPosition.x, child.localPosition.y, child.localPosition.z);
			DirectX::XMMATRIX ChildLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&child.localRotation));
			DirectX::XMMATRIX ChildLocalTransform = DirectX::XMMatrixMultiply(ChildLocalRotationTransform, ChildLocalPositionTransform);
			ChildWorldTransform = DirectX::XMMatrixMultiply(ChildLocalTransform, WorldTransform);
			DirectX::XMStoreFloat4x4(&child.worldTransform, ChildWorldTransform);
		}
		else
		{
			// ワールド行列更新
			DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone.localRotation);
			DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixRotationQuaternion(LocalRotation);
			LocalTransform.r[3] = DirectX::XMVectorSet(bone.localPosition.x, bone.localPosition.y, bone.localPosition.z, 1);
			WorldTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentWorldTransform);
			DirectX::XMStoreFloat4x4(&bone.worldTransform, WorldTransform);

			// 子ワールド行列更新
			DirectX::XMMATRIX ChildLocalPositionTransform = DirectX::XMMatrixTranslation(child.localPosition.x, child.localPosition.y, child.localPosition.z);
			DirectX::XMMATRIX ChildLocalRotationTransform = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&child.localRotation));
			DirectX::XMMATRIX ChildLocalTransform = DirectX::XMMatrixMultiply(ChildLocalRotationTransform, ChildLocalPositionTransform);
			ChildWorldTransform = DirectX::XMMatrixMultiply(ChildLocalTransform, WorldTransform);
			DirectX::XMStoreFloat4x4(&child.worldTransform, ChildWorldTransform);
		}
		// ノードに反映
		bone.node->worldTransform = bone.worldTransform;
		bone.node->rotation = bone.localRotation;
		child.node->worldTransform = child.worldTransform;
	}

}

// TwoBoneIK計算処理
void CharacterControlScene::ComputeTwoBoneIK(Model::Node* rootBone, Model::Node* midBone, Model::Node* tipBone, const DirectX::XMFLOAT3& targetPosition, const DirectX::XMFLOAT3& polePosition)
{
	// ターゲット座標を取得
	DirectX::XMVECTOR TargetWorldPosition = DirectX::XMLoadFloat3(&targetPosition);

	// 各ボーン座標を取得
	DirectX::XMMATRIX RootWorldTransform = DirectX::XMLoadFloat4x4(&rootBone->worldTransform);
	DirectX::XMMATRIX MidWorldTransform = DirectX::XMLoadFloat4x4(&midBone->worldTransform);
	DirectX::XMMATRIX TipWorldTransform = DirectX::XMLoadFloat4x4(&tipBone->worldTransform);

	DirectX::XMVECTOR RootWorldPosition = RootWorldTransform.r[3];
	DirectX::XMVECTOR MidWorldPosition = MidWorldTransform.r[3];
	DirectX::XMVECTOR TipWorldPosition = TipWorldTransform.r[3];

	// 根本→中間、中間→先端、根本→先端ベクトルを算出
	DirectX::XMVECTOR RootMidVec = DirectX::XMVectorSubtract(MidWorldPosition, RootWorldPosition);
	DirectX::XMVECTOR RootTargetVec = DirectX::XMVectorSubtract(TargetWorldPosition, RootWorldPosition);
	DirectX::XMVECTOR MidTipVec = DirectX::XMVectorSubtract(TipWorldPosition, MidWorldPosition);

	// 各ベクトルの長さを算出
	float rootMidLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootMidVec));
	float midTipLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(MidTipVec));
	float rootTargetLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(RootTargetVec));

	// ボーン回転関数(２つのベクトルから回転角と回転軸を算出し、ボーンを回転させる）
	auto rotateBone = [&](Model::Node* bone, const DirectX::XMVECTOR& Direction1, const DirectX::XMVECTOR& Direction2)
	{
		// 回転軸算出
		DirectX::XMVECTOR WorldAxis = DirectX::XMVector3Cross(Direction1, Direction2);
		if (DirectX::XMVector3Equal(WorldAxis, DirectX::XMVectorZero())) return;
		WorldAxis = DirectX::XMVector3Normalize(WorldAxis);

		// 回転軸をローカル空間変換
		DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&bone->parent->worldTransform);
		DirectX::XMMATRIX InverseParentWorldTransform = DirectX::XMMatrixInverse(nullptr, ParentWorldTransform);
		DirectX::XMVECTOR LocalAxis = DirectX::XMVector3TransformNormal(WorldAxis, InverseParentWorldTransform);
		LocalAxis = DirectX::XMVector3Normalize(LocalAxis);

		// 回転角度算出
		DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Direction1, Direction2);
		float dot = DirectX::XMVectorGetX(Dot);
		float angle = acosf(std::clamp(dot, -1.0f, 1.0f));	// 内積の結果が誤差で-1.0～1.0の間に収まらない場合がある

		// 回転クォータニオン算出
		DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(LocalAxis, angle);
		DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&bone->rotation);
		LocalRotation = DirectX::XMQuaternionMultiply(LocalRotation, LocalRotationAxis);
		DirectX::XMStoreFloat4(&bone->rotation, LocalRotation);
	};

	// 各ベクトルを単位ベクトル化
	DirectX::XMVECTOR RootTargetDirection = DirectX::XMVector3Normalize(RootTargetVec);
	DirectX::XMVECTOR RootMidDirection = DirectX::XMVector3Normalize(RootMidVec);

	// ルートボーンをターゲットの方へ向くように回転させる
	rotateBone(rootBone, RootMidDirection, RootTargetDirection);

	// ルートボーンからターゲットまでの距離が２つのボーンの長さの合計より短い場合は
	// 先端ボーンがターゲット位置と同じになるように回転調整をする
	if (rootTargetLength < rootMidLength + midTipLength)
	{
		// ヘロンの公式で三角形の面積を求める
		float s = (rootMidLength + midTipLength + rootTargetLength) / 2;
		float square = sqrtf(s * (s - rootMidLength) * (s - midTipLength) * (s - rootTargetLength));

		// ルートボーンを底辺とした時の三角形の高さを求める
		float rootMidHeight = (2 * square) / rootMidLength;			

		// 直角三角形の斜辺と高さから角度を求める
		float angle = asinf(rootMidHeight / rootTargetLength);

		if (angle > FLT_EPSILON)
		{
			DirectX::XMVECTOR PoleWorldPosition = DirectX::XMLoadFloat3(&polePosition);

			// ルートボーンからポールターゲットへのベクトル算出
			DirectX::XMVECTOR RootPoleVec = DirectX::XMVectorSubtract(PoleWorldPosition, RootWorldPosition);
			DirectX::XMVECTOR RootPoleDirection = DirectX::XMVector3Normalize(RootPoleVec);
			// ルートボーンを回転させる回転軸を求める
			DirectX::XMMATRIX RootParentWorldTransform = DirectX::XMLoadFloat4x4(&rootBone->parent->worldTransform);
			DirectX::XMMATRIX InverseRootParentWorldTransform = DirectX::XMMatrixInverse(nullptr, RootParentWorldTransform);
			DirectX::XMVECTOR PoleWorldAxis = DirectX::XMVector3Cross(RootMidDirection, RootPoleDirection);
			DirectX::XMVECTOR PoleLocalAxis = DirectX::XMVector3TransformNormal(PoleWorldAxis, InverseRootParentWorldTransform);
			PoleLocalAxis = DirectX::XMVector3Normalize(PoleLocalAxis);
			// ルートボーンを回転させる
			DirectX::XMVECTOR LocalRotationAxis = DirectX::XMQuaternionRotationAxis(PoleLocalAxis, angle);
			DirectX::XMVECTOR LocalRotation = DirectX::XMLoadFloat4(&rootBone->rotation);
			LocalRotation = DirectX::XMQuaternionMultiply(LocalRotation, LocalRotationAxis);
			DirectX::XMStoreFloat4(&rootBone->rotation, LocalRotation);
		}

	}
	// ワールド行列計算
	ComputeWorldTransform(rootBone);

	// 中間ボーンと先端ボーンのワールド座標を取得する
	MidWorldTransform = DirectX::XMLoadFloat4x4(&midBone->worldTransform);
	MidWorldPosition = MidWorldTransform.r[3];
	TipWorldTransform = DirectX::XMLoadFloat4x4(&tipBone->worldTransform);
	TipWorldPosition = TipWorldTransform.r[3];

	// 中間ボーンをターゲットの方へ向くように回転させる
	MidTipVec = DirectX::XMVectorSubtract(TipWorldPosition, MidWorldPosition);
	DirectX::XMVECTOR MidTipDirection = DirectX::XMVector3Normalize(MidTipVec);
	DirectX::XMVECTOR MidTargetVec = DirectX::XMVectorSubtract(TargetWorldPosition, MidWorldPosition);
	DirectX::XMVECTOR MidTargetDirection = DirectX::XMVector3Normalize(MidTargetVec);

	rotateBone(midBone, MidTipDirection, MidTargetDirection);
	ComputeWorldTransform(midBone);
}

// コリジョン骨
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

// 指定ノード以下のワールド行列を計算
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

// 球と三角形との交差を判定する
bool CharacterControlScene::SphereIntersectTriangle(
	const DirectX::XMFLOAT3& sphereCenter,
	const float sphereRadius,
	const DirectX::XMFLOAT3& triangleVertexA,
	const DirectX::XMFLOAT3& triangleVertexB,
	const DirectX::XMFLOAT3& triangleVertexC,
	DirectX::XMFLOAT3& hitPosition,
	DirectX::XMFLOAT3& hitNormal)
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
	if (DirectX::XMVector3Equal(TriangleNormal, DirectX::XMVectorZero()))
	{
		return false;
	}

	// 平面と球の距離を求める
	DirectX::XMVECTOR SphereCenter = DirectX::XMLoadFloat3(&sphereCenter);
	DirectX::XMVECTOR Dot1 = DirectX::XMVector3Dot(TriangleNormal, SphereCenter);
	DirectX::XMVECTOR Dot2 = DirectX::XMVector3Dot(TriangleNormal, TriangleVertex[0]);
	DirectX::XMVECTOR Distance = DirectX::XMVectorSubtract(Dot1, Dot2);
	float distance = DirectX::XMVectorGetX(Distance);

	// 半径以上離れていれば当たらない
	if (distance > sphereRadius)
	{
		return false;
	}

	// 球の中心が面に接しているか、負の方向にある場合は当たらない
	if (distance < 0.0f)
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
	return false;
}

// 球とモデルとの交差を判定する
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
		// バウンディングボックスと判定する
	//	if (!sphere.Intersects(mesh.worldBounds)) continue;

		// メッシュのスケーリングによって処理を分岐する
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		float lengthSqAxisX = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[0]));
		float lengthSqAxisY = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[1]));
		float lengthSqAxisZ = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(WorldTransform.r[2]));
		if (lengthSqAxisX == lengthSqAxisY && lengthSqAxisY == lengthSqAxisZ)
		{
			// 球のローカル空間変換
			float length = sqrtf(lengthSqAxisX);
			DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
			DirectX::XMVECTOR WorldSphereCenter = DirectX::XMLoadFloat3(&sphere.Center);
			DirectX::XMVECTOR LocalSphereCenter = DirectX::XMVector3Transform(WorldSphereCenter, InverseWorldTransform);
			
			DirectX::BoundingSphere localSphere;
			DirectX::XMStoreFloat3(&localSphere.Center, LocalSphereCenter);
			localSphere.Radius = sphereRadius / length;

			// ローカル空間での球と三角形の衝突処理
			for (size_t i = 0; i < mesh.indices.size(); i += 3)
			{
				uint32_t indexA = mesh.indices.at(i + 0);
				uint32_t indexB = mesh.indices.at(i + 1);
				uint32_t indexC = mesh.indices.at(i + 2);

				const Model::Vertex& vertexA = mesh.vertices.at(indexA);
				const Model::Vertex& vertexB = mesh.vertices.at(indexB);
				const Model::Vertex& vertexC = mesh.vertices.at(indexC);

				// バウンディングボックスと判定する
				//DirectX::BoundingBox box;
				//DirectX::BoundingBox::CreateFromPoints(box, 3, &vertexA.position, sizeof(ModelResource::Vertex));
				//if (!localSphere.Intersects(box)) continue;

				// 球と三角形の衝突処理
				DirectX::XMFLOAT3 localHitPosition, localHitNormal;
				if (SphereIntersectTriangle(
					localSphere.Center, localSphere.Radius,
					vertexA.position, vertexB.position, vertexC.position,
					localHitPosition, localHitNormal))
				{
					// ワールド空間変換
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
			// ワールド空間での球と三角形の衝突処理
			for (size_t i = 0; i < mesh.indices.size(); i += 3)
			{
				uint32_t indexA = mesh.indices.at(i + 0);
				uint32_t indexB = mesh.indices.at(i + 1);
				uint32_t indexC = mesh.indices.at(i + 2);

				const Model::Vertex& vertexA = mesh.vertices.at(indexA);
				const Model::Vertex& vertexB = mesh.vertices.at(indexB);
				const Model::Vertex& vertexC = mesh.vertices.at(indexC);

				// 頂点座標のワールド空間変換
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

				// 球と三角形の衝突処理
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

// レイとモデルとの交差を判定する
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
		// バウンディングボックスと判定する
		//float length = worldRayLength;
		//if (!mesh.worldBounds.Intersects(WorldRayStart, WorldRayDirection, length)) continue;

		// レイのローカル空間変換
		DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		DirectX::XMMATRIX InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, WorldTransform);
		DirectX::XMVECTOR LocalRayStart = DirectX::XMVector3Transform(WorldRayStart, InverseWorldTransform);
		DirectX::XMVECTOR LocalRayVec = DirectX::XMVector3TransformNormal(WorldRayVec, InverseWorldTransform);
		DirectX::XMVECTOR LocalRayDirection = DirectX::XMVector3Normalize(LocalRayVec);
		float localRayLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(LocalRayVec));

		// ローカル空間での球と三角形の衝突処理
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

			// レイと三角形の交差判定
			float localRayDist = localRayLength;
			if (DirectX::TriangleTests::Intersects(LocalRayStart, LocalRayDirection, LocalPositionA, LocalPositionB, LocalPositionC, localRayDist))
			{
				// 交差位置を求める
				DirectX::XMVECTOR LocalHitVec = DirectX::XMVectorScale(LocalRayDirection, localRayDist);
				DirectX::XMVECTOR LocalHitPosition = DirectX::XMVectorAdd(LocalRayStart, LocalHitVec);

				// ワールド空間変換
				DirectX::XMVECTOR WorldHitPosition = DirectX::XMVector3Transform(LocalHitPosition, WorldTransform);
				DirectX::XMVECTOR WorldHitVec = DirectX::XMVectorSubtract(WorldHitPosition, WorldRayStart);

				// 最近判定
				float worldHitLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(WorldHitVec));
				if (worldHitLength < nearestWorldHitLength)
				{
					nearestWorldHitLength = worldHitLength;

					// 面法線を求める
					DirectX::XMVECTOR EdgeAB = DirectX::XMVectorSubtract(LocalPositionB, LocalPositionA);
					DirectX::XMVECTOR EdgeBC = DirectX::XMVectorSubtract(LocalPositionC, LocalPositionB);
					DirectX::XMVECTOR LocalHitNormal = DirectX::XMVector3Cross(EdgeAB, EdgeBC);

					// ワールド空間変換
					DirectX::XMVECTOR WorldHitNormal = DirectX::XMVector3TransformNormal(LocalHitNormal, WorldTransform);

					// データ格納
					DirectX::XMStoreFloat3(&hitResult.position, WorldHitPosition);
					DirectX::XMStoreFloat3(&hitResult.normal, DirectX::XMVector3Normalize(WorldHitNormal));

					hit = true;
				}
			}
		}
	}
	return hit;
}
