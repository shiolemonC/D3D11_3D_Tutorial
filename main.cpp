#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sstream>
#include <DirectXMath.h>
//#include "debug_ostream.h"
#include "game_window.h"
#include "direct3d.h"
#include "shader.h"
#include "shader3d.h"
#include "sampler.h"
#include "sprite.h"
#include "texture.h"
#include "sprite_anim.h"
#include "fade.h"
#include "debug_text.h"
#include "system_timer.h"
#include "mouse.h"
#include "key_logger.h"
#include "game.h"
#include "Audio.h"
#include "collision.h"
#include "cube.h"
#include "scene.h"
#include <Xinput.h>
#include "grid.h"
#include "meshfield.h"
#include "light.h"
#include "ModelStatic.h"
#include "ModelSkinned.h"
#include "AnimatorRegistry.h"
#pragma comment(lib, "xinput.lib")

using namespace DirectX;

/*--------------------------------------------------
	メイン、テンプレート
----------------------------------------------------*/


int APIENTRY WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow
)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// DPIスケーリング
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HWND hWnd = GameWindow_Create(hInstance); // game_window.cppの中で定義された関数を使うのこと、hInstanceは引数として渡す

	// Initialization
	SystemTimer_Initialize();
	KeyLogger_Initialize();
	Mouse_Initialize(hWnd); // 引数はウィンドウ
	InitAudio();

	Direct3D_Initialize(hWnd); // Direct3Dの初期化、必ず一番先頭
	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Shader3d_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sampler_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	SpriteAnim_Initialize();
	Fade_Initialize();
	Mouse_SetVisible(true);

	Game_Initialize();
	Scene_Initialize();
	Grid_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Cube_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Light_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	MeshField_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	ModelStatic_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	// 配置默认模型路径（mat 可留空；overrideDiffuse 可选）
	ModelStatic_SetDefaultPaths(
		L"D:/AssetCooker/resources/Cooked/ninja_T.mesh",
		L"D:/AssetCooker/resources/Cooked/ninja_T.mat",
		// 若 .mat 没有写纹理或想强制指定，请给一张 png；不想覆盖就传 nullptr
		//nullptr
		L"D:/AssetCooker/resources/test/ninja_T.fbm/Ch24_1001_Diffuse.png"
	);

	// 加载
	//ModelStatic_LoadDefault();

	//ModelSkinned_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	//ModelSkinnedDesc d;
	//d.meshPath = L"D:/AssetCooker/resources/Cooked/walk_test.mesh";
	//d.skelPath = L"D:/AssetCooker/resources/Cooked/walk_test.skel";
	//d.animPath = L"D:/AssetCooker/resources/Cooked/walk_test.anim";   // 没有可以留空
	//d.matPath =  L"D:/AssetCooker/resources/Cooked/walk_test.mat";    // 不写则默认同名 .mat
	//d.baseColorTexOverride = L"D:/AssetCooker/resources/test/ninja_T.fbm/Ch24_1001_Diffuse.png";

	//ModelSkinned_Load(d);

	//ModelSkinned_SetLoop(true);
	//ModelSkinned_SetPlaybackRate(1.0f);

	AnimatorRegistry_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	// 一键登录你项目的所有动作（放在 animator_register.cpp 里）
	AnimRegister();

	// 开机默认播 Idle（可选覆盖 loop/rate）
	bool changed = false;
	AnimatorRegistry_Play(L"Walk", &changed);



#if defined(DEBUG) || defined(_DEBUG)
	// Debug Mode
	hal::DebugText dt(Direct3D_GetDevice(), Direct3D_GetContext(),
		L"consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),
		Direct3D_GetBackBufferHeight(),
		0.0f, 0.0f,
		0, 0,
		0.0f, 16.0f);

	Collision_DebugInitialize(Direct3D_GetDevice(), Direct3D_GetContext());

#endif

	ShowWindow(hWnd, nCmdShow); // ウィンドウ表示
	UpdateWindow(hWnd); // ウィンドウの中を更新する

	// fps・実行フレーム速度計測用
	double exec_last_time = SystemTimer_GetTime(); // 前回処理した時間を記録
	double fps_last_time = exec_last_time;         // fps計測開始の基準時間
	double current_time = 0.0;                     // 現在時刻（毎フレーム用）
	ULONG frame_count = 0;                         // フレーム数カウント用
	double fps = 0.0;                              // fps値を保存

	MSG msg;
	
	do
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			// ウィンドウメッセージが来ていたら
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else // ゲーム処理
		{
			// fps計測処理

			current_time = SystemTimer_GetTime(); // 現在のシステム時刻を取得
			double elapsed_time = current_time - fps_last_time; // fps計測のための経過時間を計算

			if (elapsed_time >= 1.0) // 1秒ごとに計測
			{
				fps = frame_count / elapsed_time;
				fps_last_time = current_time; // 次回のためにfpsを測定した時刻保存
				frame_count = 0; // カウントをリセット
			}

			// 1/60秒ごとに実行（実行の制御）
			elapsed_time = current_time - exec_last_time; // 実行の経過時間を計算
			if (elapsed_time >= (1.0 / 60.0))
			//if (true)
			{
				exec_last_time = current_time; // 処理した時刻を保存

				// ゲームの更新
				KeyLogger_Update(); // キーの状態を更新

				Game_Update(elapsed_time);
				Scene_Update(elapsed_time);
				SpriteAnim_Update(elapsed_time);
				Fade_Update(elapsed_time);
				
				// ゲームの描画
				Direct3D_Clear(); // Clear the screen

				Sprite_Begin();

				Game_Draw();
				Scene_Draw();
				Fade_Draw();

				// フレーム計測数表示
#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効

				std::stringstream ss;
				ss << "fps: " << fps << std::endl;
				dt.SetText(ss.str().c_str(), { 0.0f, 0.0f, 1.0f, 1.0f });

				dt.Draw();
				dt.Clear();
#endif

				Direct3D_Present();
				Scene_Refresh();

				frame_count++; //フレーム計測用、気を付けて
			}

		}

	} while (msg.message != WM_QUIT);
	
#if defined(DEBUG) || defined(_DEBUG)
	Collision_DebugFinalize();
#endif

	//Game_Finalize();
	MeshField_Finalize();
	Grid_Finalize();
	Light_Finalize();
	Cube_Finalize();
	ModelStatic_UnloadDefault();
	ModelStatic_Finalize();
	Scene_Finalize();
	

	Fade_Finalize();
	SpriteAnim_Finalize();
	Texture_Finalize();
	Sprite_Finalize();
	Shader3d_Finalize();
	Shader_Finalize();
	Sampler_Finalize();
	Direct3D_Finalize();

	UninitAudio();
	Mouse_Finalize();
	
	return (int)msg.wParam;

	return 0;
}