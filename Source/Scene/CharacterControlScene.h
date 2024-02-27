#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"
#include "Light.h"
#include "Model.h"

// キャラクター操作シーン
class CharacterControlScene : public Scene
{
public:
	CharacterControlScene();
	~CharacterControlScene() override = default;

	// 更新処理
	void Update(float elapsedTime) override;

	// 描画処理
	void Render(float elapsedTime) override;

	// GUI描画処理
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

		// ステート関連
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

		// モデル関連
		std::shared_ptr<Model>				model;
		std::shared_ptr<Model>				staff;

		// トランスフォーム関連
		DirectX::XMFLOAT3					position;
		DirectX::XMFLOAT3					angle;
		DirectX::XMFLOAT4X4					transform;
		float								radius = 0.4f;

		// 入力関連
		float								inputAxisX = 0;
		float								inputAxisY = 0;
		float								inputAxisLength = 0;
		uint32_t							inputKeyOld = 0;
		uint32_t							inputKeyNew = 0;
		uint32_t							inputKeyDown = 0;
		uint32_t							inputKeyUp = 0;

		// アニメーション関連
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

		// 移動関連
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
		
		// 攻撃関連
		bool								combo = false;

		// ボーン関連
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

		// トレール関連
		DirectX::XMFLOAT3					trailPositions[2][128];

		// デバッグ関連
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

	// 入力更新処理
	void UpdateInput();

	// カメラセットアップ
	void SetupCamera();

	// 三人称視点カメラ更新処理
	void UpdateThirdPersonCamera(float elapsedTime);

	// ステージセットアップ
	void SetupStage(ID3D11Device* device);

	// ボールセットアップ
	void SetupBalls(ID3D11Device* device);

	// ボール更新処理
	void UpdateBalls(float elapsedTime);

	// ユニティちゃんセットアップ
	void SetupUnityChan(ID3D11Device* device);

	// ユニティちゃん更新処理
	void UpdateUnityChan(float elapsedTime);

	// ユニティちゃんアニメーション更新処理
	void UpdateUnityChanAnimation(float elapsedTime);

	// ユニティちゃんステートマシン更新処理
	void UpdateUnityChanStateMachine(float elapsedTime);

	// ユニティちゃんベロシティ更新処理
	void UpdateUnityChanVelocity(float elapsedTime);

	// ユニティちゃん位置更新処理
	void UpdateUnityChanPosition(float elapsedTime);

	// ユニティちゃん行列更新処理
	void UpdateUnityChanTransform();

	// ユニティちゃんエフェクト更新処理
	void UpdateUnityChanEffect();

	// ユニティちゃんルックアット更新処理
	void UpdateUnityChanLookAt(float elapsedTime);

	// ユニティちゃんIKボーン更新処理
	void UpdateUnityChanIKBones();

	// ユニティちゃん物理ボーン更新処理
	void UpdateUnityChanPhysicsBones(float elapsedTime);

	// ユニティちゃんアニメーション再生
	void PlayUnityChanAnimation(const char* name, float blendSeconds, bool loop, bool rootMotion);

	// ユニティちゃんステート切り替え
	void ChangeUnityChanState(UnityChan::State state);

	// ユニティちゃんステート開始処理
	void UnityChanStateEnter(float elapsedTime);

	// ユニティちゃんステート開始処理
	void UnityChanStateUpdate(float elapsedTime);

	// ユニティちゃんステート終了処理
	void UnityChanStateExit(float elapsedTime);

	// ユニティちゃん入力移動処理
	void UnityChanInputMovement(float elapsedTime);

	// ユニティちゃん入力移動処理
	void UnityChanInputJump();

	// ユニティちゃん空中処理
	void UnityChanAirControl(float elapsedTime);

	// ユニティちゃんコンボ入力処理
	void UnityChanInputCombo();

	// ユニティちゃんスタッフトレール更新処理
	void UnityChanStaffTrail();

	// ルートモーション処理
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

	// 物理ボーン計算処理
	static void ComputePhysicsBones(
		std::vector<PhysicsBone>& bones,
		const std::vector<CollisionBone>& collisionBones,
		const DirectX::XMFLOAT3& force,
		float maxVelocity);

	// TwoBoneIK計算処理
	static void ComputeTwoBoneIK(
		Model::Node* rootBone,
		Model::Node* midBone,
		Model::Node* tipBone,
		const DirectX::XMFLOAT3& targetPosition,
		const DirectX::XMFLOAT3& polePosition);

	// コリジョンボーン計算処理
	static void ComputeCollisionBones(
		const std::vector<CollisionBone>& collisionBones,
		DirectX::XMVECTOR& Position,
		float radius);

	// 指定ノード以下のワールド行列を計算
	static void ComputeWorldTransform(Model::Node* node);

	// 球と三角形との交差を判定する
	static bool SphereIntersectTriangle(
		const DirectX::XMFLOAT3& sphereCenter,
		const float sphereRadius,
		const DirectX::XMFLOAT3& triangleVertexA,
		const DirectX::XMFLOAT3& triangleVertexB,
		const DirectX::XMFLOAT3& triangleVertexC,
		DirectX::XMFLOAT3& hitPosition,
		DirectX::XMFLOAT3& hitNormal);

	// 球とモデルとの交差を判定する
	static bool SphereIntersectModel(
		const DirectX::XMFLOAT3& sphereCenter,
		const float sphereRadius,
		const Model* model,
		std::vector<HitResult>& hits);

	// レイとモデルとの交差を判定する
	static bool RayIntersectModel(
		const DirectX::XMFLOAT3& rayStart,
		const DirectX::XMFLOAT3& rayEnd,
		const Model* model,
		HitResult& hit);

	// 秒をゲームフレームに変換
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
