#include <memory>
#include <sstream>
#include <imgui.h>

#include "Framework.h"
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include "Scene/ModelViewerScene.h"
#include "Scene/WeightedCollisionScene.h"
#include "Scene/RaceRankingScene.h"
#include "Scene/SphereCastMoveScene.h"
#include "Scene/SwordTrailScene.h"
#include "Scene/RootMotionScene.h"
#include "Scene/ConfirmCommandScene.h"
#include "Scene/SpaceDivisionRaycastScene.h"
#include "Scene/SortingAndFilteringScene.h"
#include "Scene/LookAtScene.h"
#include "Scene/TwoBoneIKScene.h"
#include "Scene/SphereVsTriangleCollisionScene.h"
#include "Scene/PhysicsRopeScene.h"
#include "Scene/PhysicsBoneScene.h"
#include "Scene/CCDIKScene.h"
#include "Scene/CharacterControlScene.h"

// 垂直同期間隔設定
static const int syncInterval = 1;

// コンストラクタ
Framework::Framework(HWND hWnd)
	: hWnd(hWnd)
{
	hDC = GetDC(hWnd);

	// グラフィックス初期化
	Graphics::Instance().Initialize(hWnd);

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

	// シーン初期化
	//scene = std::make_unique<ModelViewerScene>();
	scene = std::make_unique<WeightedCollisionScene>();
	//scene = std::make_unique<RaceRankingScene>();
	//scene = std::make_unique<ConfirmCommandScene>();
	//scene = std::make_unique<SortingAndFilteringScene>();
	//scene = std::make_unique<RootMotionScene>();
	//scene = std::make_unique<SwordTrailScene>();
	//scene = std::make_unique<SphereVsTriangleCollisionScene>();
	//scene = std::make_unique<PhysicsRopeScene>();
	//scene = std::make_unique<PhysicsBoneScene>();
	//scene = std::make_unique<TwoBoneIKScene>();
	//scene = std::make_unique<CCDIKScene>();
	//scene = std::make_unique<LookAtScene>();
	//scene = std::make_unique<SpaceDivisionRaycastScene>();
	//scene = std::make_unique<SphereCastMoveScene>();
	//scene = std::make_unique<CharacterControlScene>();
}

// デストラクタ
Framework::~Framework()
{
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	ReleaseDC(hWnd, hDC);
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	// IMGUIフレーム開始処理	
	ImGuiRenderer::NewFrame();

	// シーン更新処理
	scene->Update(elapsedTime);
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

	// 画面クリア
	Graphics::Instance().Clear(0.5f, 0.5f, 0.5f, 1);

	// レンダーターゲット設定
	Graphics::Instance().SetRenderTargets();

	// シーン描画処理
	scene->Render(elapsedTime);

	// シーンGUI描画処理
	scene->DrawGUI();

	// シーン切り替えGUI
	SceneSelectGUI();
#if 0
	// IMGUIデモウインドウ描画（IMGUI機能テスト用）
	ImGui::ShowDemoWindow();
#endif
	// IMGUI描画
	ImGuiRenderer::Render(dc);

	// 画面表示
	Graphics::Instance().Present(syncInterval);
}

template<class T>
void Framework::ChangeSceneButtonGUI(const char* name)
{
	if (ImGui::Button(name))
	{
		scene = std::make_unique<T>();
	}
}

// シーン切り替えGUI
void Framework::SceneSelectGUI()
{
	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	float width = 210;
	float height = 490;
	ImGui::SetNextWindowPos(ImVec2(pos.x + displaySize.x - width - 10, pos.y + 10), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Once);

	if (ImGui::Begin("Scene"))
	{
		ChangeSceneButtonGUI<ModelViewerScene>(u8"00.モデルビューア");
		ChangeSceneButtonGUI<WeightedCollisionScene>(u8"01.重みのある衝突処理");
		ChangeSceneButtonGUI<RaceRankingScene>(u8"02.レース順位判定処理");
		ChangeSceneButtonGUI<SphereCastMoveScene>(u8"03.スフィアキャスト移動処理");
		ChangeSceneButtonGUI<SwordTrailScene>(u8"04.剣の軌跡処理");
		ChangeSceneButtonGUI<RootMotionScene>(u8"05.ルートモーション処理");
		ChangeSceneButtonGUI<ConfirmCommandScene>(u8"06.コマンド判定処理");
		ChangeSceneButtonGUI<SpaceDivisionRaycastScene>(u8"07.空間分割レイキャスト");
		ChangeSceneButtonGUI<SortingAndFilteringScene>(u8"08.ソート＆フィルタリング処理");
		ChangeSceneButtonGUI<LookAtScene>(u8"09.ルックアット処理");
		ChangeSceneButtonGUI<TwoBoneIKScene>(u8"10.2本のボーンIK制御");
		ChangeSceneButtonGUI<SphereVsTriangleCollisionScene>(u8"11.球と三角形の衝突処理");
		ChangeSceneButtonGUI<PhysicsRopeScene>(u8"12.揺れもの処理(ロープ)");
		ChangeSceneButtonGUI<PhysicsBoneScene>(u8"13.揺れもの処理(ボーン)");
		ChangeSceneButtonGUI<CCDIKScene>(u8"14.3本以上のボーンIK制御");
		ChangeSceneButtonGUI<CharacterControlScene>(u8"99.キャラクター制御");
	}
	ImGui::End();
}

// フレームレート計算
void Framework::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.
	static int frames = 0;
	static float time_tlapsed = 0.0f;

	frames++;

	// Compute averages over one second period.
	if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
	{
		float fps = static_cast<float>(frames); // fps = frameCnt / 1
		float mspf = 1000.0f / fps;
		std::ostringstream outs;
		outs.precision(6);
		outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
		SetWindowTextA(hWnd, outs.str().c_str());

		// Reset for next average.
		frames = 0;
		time_tlapsed += 1.0f;
	}
}

// アプリケーションループ
int Framework::Run()
{
	MSG msg = {};

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			timer.Tick();
			CalculateFrameStats();

			float elapsedTime = syncInterval == 0
				? timer.TimeInterval()
				: syncInterval / static_cast<float>(GetDeviceCaps(hDC, VREFRESH))
				;
			Update(elapsedTime);
			Render(elapsedTime);
		}
	}
	return static_cast<int>(msg.wParam);
}

// メッセージハンドラ
LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CREATE:
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	case WM_ENTERSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
		timer.Stop();
		break;
	case WM_EXITSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		timer.Start();
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
