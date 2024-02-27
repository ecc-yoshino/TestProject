#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"
#include "Light.h"
#include "Model.h"

// �L�����N�^�[����V�[��
class CharacterControlScene : public Scene
{
public:
	CharacterControlScene();
	~CharacterControlScene() override = default;

	// �X�V����
	void Update(float elapsedTime) override;

	// �`�揈��
	void Render(float elapsedTime) override;

	// GUI�`�揈��
	void DrawGUI() override;

private:
	struct HitResult
	{
		DirectX::XMFLOAT3	position;
		DirectX::XMFLOAT3	normal;
	};

	struct PhysicsBone
	{
		Model::Node* node = nullptr;
		float collisionRadius = 0.1f;
		DirectX::XMFLOAT3	localPosition;
		DirectX::XMFLOAT4	localRotation;
		DirectX::XMFLOAT4	defaultLocalRotation;
		DirectX::XMFLOAT4X4	defaultLocalTransform;
		DirectX::XMFLOAT4X4	worldTransform;
		DirectX::XMFLOAT3	oldWorldPosition;
	};

	struct CollisionBone
	{
		Model::Node*		node = nullptr;
		DirectX::XMFLOAT3	offset;
		float				radius = 0.1f;
	};

	struct LookAtIKBone
	{
		Model::Node* node = nullptr;
		DirectX::XMFLOAT3	forward;
		DirectX::XMFLOAT3	target;
	};

	struct FootIKBone
	{
		Model::Node*		thighNode = nullptr;
		Model::Node*		legNode = nullptr;
		Model::Node*		footNode = nullptr;
		Model::Node*		toeNode = nullptr;

		DirectX::XMFLOAT3	rayStart;
		DirectX::XMFLOAT3	rayEnd;
		DirectX::XMFLOAT3	anklePosition;
		DirectX::XMFLOAT3	ankleTarget;
		HitResult			hitResult;
		bool				hit;
	};

	struct ThirdPersonCamera
	{
		float				inputAxisX = 0;
		float				inputAxisY = 0;
		float				inputAxisLength = 0;
		DirectX::XMFLOAT3	angle = { 0, 0, 0 };
		DirectX::XMFLOAT3	eye = { 0, 0, 0 };
		DirectX::XMFLOAT3	focus = { 0, 0, 0 };
		float				moveSpeed = 4.0f;
		float				rotateSpeed = 1.57f;
		float				focusRange = 5.0f;
		float				minPitch = -DirectX::XMConvertToRadians(80.0f);
		float				maxPitch = DirectX::XMConvertToRadians(80.0f);
	};

	struct UnityChan
	{
		static const uint32_t	KeyJump = (1 << 0);
		static const uint32_t	KeyAttack = (1 << 1);

		// �X�e�[�g�֘A
		enum class State
		{
			Idle,
			Move,
			Jump,
			Air,
			Landing,
			Combo1,
			Combo2,
			Combo3,
			Combo4,

			EnumCount
		};
		State								state = State::Idle;
		State								nextState = State::Idle;

		// ���f���֘A
		std::shared_ptr<Model>				model;
		std::shared_ptr<Model>				staff;

		// �g�����X�t�H�[���֘A
		DirectX::XMFLOAT3					position;
		DirectX::XMFLOAT3					angle;
		DirectX::XMFLOAT4X4					transform;
		float								radius = 0.4f;

		// ���͊֘A
		float								inputAxisX = 0;
		float								inputAxisY = 0;
		float								inputAxisLength = 0;
		uint32_t							inputKeyOld = 0;
		uint32_t							inputKeyNew = 0;
		uint32_t							inputKeyDown = 0;
		uint32_t							inputKeyUp = 0;

		// �A�j���[�V�����֘A
		float								oldAnimationSeconds = 0;
		float								animationSeconds = 0;
		float								animationBlendSeconds = 0;
		float								animationBlendSecondsLength = 0;
		int									animationIndex = -1;
		bool								animationLoop = false;
		bool								animationChange = false;
		bool								animationPlaying = false;
		bool								computeRootMotion = false;
		int									rootMotionNodeIndex = -1;
		DirectX::XMFLOAT3					rootMotionTranslation = { 0, 0, 0 };

		std::vector<Model::NodePose>		nodePoses;
		std::vector<Model::NodePose>		cacheNodePoses;

		// �ړ��֘A
		DirectX::XMFLOAT3					velocity = { 0, 0, 0 };
		DirectX::XMFLOAT3					deltaMove = { 0, 0, 0 };
		DirectX::XMFLOAT3					groundNormal = { 0, 1, 0 };
		DirectX::XMFLOAT3					desiredDirection{ 0, 0, 0 };
		float								friction = 0.5f;
		float								acceleration = 1.0f;
		float								jumpSpeed = 5.0f;
		float								moveSpeed = 6.0f;
		float								turnSpeed = DirectX::XMConvertToRadians(720);
		float								maxPhysicsBoneVelocity = 0.05f;
		float								slopeLimit = 45.0f;
		float								airControl = 0.5f;
		float								groundAdjust = 0.1f;
		bool								onGround = false;
		std::vector<HitResult>				hits;
		
		// �U���֘A
		bool								combo = false;

		// �{�[���֘A
		LookAtIKBone						lookAtBone;
		std::vector<PhysicsBone>			leftHairTailBones;
		std::vector<PhysicsBone>			rightHairTailBones;
		std::vector<PhysicsBone>			leftSkirtFrontBones;
		std::vector<PhysicsBone>			leftSkirtBackBones;
		std::vector<PhysicsBone>			rightSkirtFrontBones;
		std::vector<PhysicsBone>			rightSkirtBackBones;
		std::vector<CollisionBone>			upperBodyCollisionBones;
		std::vector<CollisionBone>			leftLegCollisionBones;
		std::vector<CollisionBone>			rightLegCollisionBones;

		FootIKBone							leftFootIKBone;
		FootIKBone							rightFootIKBone;
		bool								applyFootIK = false;
		int									hipsNodeIndex;
		DirectX::XMFLOAT3					hipsOffset;

		// �g���[���֘A
		DirectX::XMFLOAT3					trailPositions[2][128];

		// �f�o�b�O�֘A
		bool								visibleLeftHairTailBones = false;
		bool								visibleRightHairTailBones = false;
		bool								visibleLeftSkirtFrontBones = false;
		bool								visibleLeftSkirtBackBones = false;
		bool								visibleRightSkirtFrontBones = false;
		bool								visibleRightSkirtBackBones = false;
		bool								visibleUpperBodyCollisionBones = false;
		bool								visibleLeftLegCollisionBones = false;
		bool								visibleRightLegCollisionBones = false;
		bool								visibleCharacterCollision = false;
	};

	struct Stage
	{
		std::shared_ptr<Model>				model;
	};

	struct Ball
	{
		std::shared_ptr<Model>				model;
		DirectX::XMFLOAT3					position;
		float								scale = 0.3f;
	};

	// ���͍X�V����
	void UpdateInput();

	// �J�����Z�b�g�A�b�v
	void SetupCamera();

	// �O�l�̎��_�J�����X�V����
	void UpdateThirdPersonCamera(float elapsedTime);

	// �X�e�[�W�Z�b�g�A�b�v
	void SetupStage(ID3D11Device* device);

	// �{�[���Z�b�g�A�b�v
	void SetupBalls(ID3D11Device* device);

	// �{�[���X�V����
	void UpdateBalls(float elapsedTime);

	// ���j�e�B�����Z�b�g�A�b�v
	void SetupUnityChan(ID3D11Device* device);

	// ���j�e�B�����X�V����
	void UpdateUnityChan(float elapsedTime);

	// ���j�e�B�����A�j���[�V�����X�V����
	void UpdateUnityChanAnimation(float elapsedTime);

	// ���j�e�B�����X�e�[�g�}�V���X�V����
	void UpdateUnityChanStateMachine(float elapsedTime);

	// ���j�e�B�����x���V�e�B�X�V����
	void UpdateUnityChanVelocity(float elapsedTime);

	// ���j�e�B�����ʒu�X�V����
	void UpdateUnityChanPosition(float elapsedTime);

	// ���j�e�B�����s��X�V����
	void UpdateUnityChanTransform();

	// ���j�e�B�����G�t�F�N�g�X�V����
	void UpdateUnityChanEffect();

	// ���j�e�B����񃋃b�N�A�b�g�X�V����
	void UpdateUnityChanLookAt(float elapsedTime);

	// ���j�e�B�����IK�{�[���X�V����
	void UpdateUnityChanIKBones();

	// ���j�e�B����񕨗��{�[���X�V����
	void UpdateUnityChanPhysicsBones(float elapsedTime);

	// ���j�e�B�����A�j���[�V�����Đ�
	void PlayUnityChanAnimation(const char* name, float blendSeconds, bool loop, bool rootMotion);

	// ���j�e�B�����X�e�[�g�؂�ւ�
	void ChangeUnityChanState(UnityChan::State state);

	// ���j�e�B�����X�e�[�g�J�n����
	void UnityChanStateEnter(float elapsedTime);

	// ���j�e�B�����X�e�[�g�J�n����
	void UnityChanStateUpdate(float elapsedTime);

	// ���j�e�B�����X�e�[�g�I������
	void UnityChanStateExit(float elapsedTime);

	// ���j�e�B�������͈ړ�����
	void UnityChanInputMovement(float elapsedTime);

	// ���j�e�B�������͈ړ�����
	void UnityChanInputJump();

	// ���j�e�B�����󒆏���
	void UnityChanAirControl(float elapsedTime);

	// ���j�e�B�����R���{���͏���
	void UnityChanInputCombo();

	// ���j�e�B�����X�^�b�t�g���[���X�V����
	void UnityChanStaffTrail();

	// ���[�g���[�V��������
	static void ComputeRootMotion(
		const Model* model,
		int rootMotionNodeIndex,
		int animationIndex,
		float oldAnimationSeconds,
		float newAnimationSeconds,
		DirectX::XMFLOAT3& translation,
		DirectX::XMFLOAT3& rootMotionNodePosition,
		bool bakeTranslationX,
		bool bakeTranslationY,
		bool bakeTranslationZ);

	// �����{�[���v�Z����
	static void ComputePhysicsBones(
		std::vector<PhysicsBone>& bones,
		const std::vector<CollisionBone>& collisionBones,
		const DirectX::XMFLOAT3& force,
		float maxVelocity);

	// TwoBoneIK�v�Z����
	static void ComputeTwoBoneIK(
		Model::Node* rootBone,
		Model::Node* midBone,
		Model::Node* tipBone,
		const DirectX::XMFLOAT3& targetPosition,
		const DirectX::XMFLOAT3& polePosition);

	// �R���W�����{�[���v�Z����
	static void ComputeCollisionBones(
		const std::vector<CollisionBone>& collisionBones,
		DirectX::XMVECTOR& Position,
		float radius);

	// �w��m�[�h�ȉ��̃��[���h�s����v�Z
	static void ComputeWorldTransform(Model::Node* node);

	// ���ƎO�p�`�Ƃ̌����𔻒肷��
	static bool SphereIntersectTriangle(
		const DirectX::XMFLOAT3& sphereCenter,
		const float sphereRadius,
		const DirectX::XMFLOAT3& triangleVertexA,
		const DirectX::XMFLOAT3& triangleVertexB,
		const DirectX::XMFLOAT3& triangleVertexC,
		DirectX::XMFLOAT3& hitPosition,
		DirectX::XMFLOAT3& hitNormal);

	// ���ƃ��f���Ƃ̌����𔻒肷��
	static bool SphereIntersectModel(
		const DirectX::XMFLOAT3& sphereCenter,
		const float sphereRadius,
		const Model* model,
		std::vector<HitResult>& hits);

	// ���C�ƃ��f���Ƃ̌����𔻒肷��
	static bool RayIntersectModel(
		const DirectX::XMFLOAT3& rayStart,
		const DirectX::XMFLOAT3& rayEnd,
		const Model* model,
		HitResult& hit);

	// �b���Q�[���t���[���ɕϊ�
	static float ConvertToGameFrame(float seconds) { return seconds * 60.0f; }

private:
	Camera									camera;
	FreeCameraController					cameraController;
	LightManager							lightManager;
	UnityChan								unitychan;
	Stage									stage;
	std::vector<Ball>						balls;
	ThirdPersonCamera						thirdPersonCamera;

	bool									useFreeCamera = false;
	float									gravity = 0.2f;
	float									timeScale = 1.0f;
	DirectX::XMFLOAT3						fieldForce = { 0, 0, 0 };
	Model::Node*							selectionNode = nullptr;
};
