/* main.cpp
Copyright (c) 2014 by Michael Zahniser

Main function for Endless Sky, a space exploration and combat RPG.

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Audio.h"
#include "Command.h"
#include "Conversation.h"
#include "ConversationPanel.h"
#include "DataFile.h"
#include "DataNode.h"
#include "Dialog.h"
#include "Editor.h"
#include "Files.h"
#include "text/Font.h"
#include "FrameTimer.h"
#include "GameAssets.h"
#include "GameData.h"
#include "GameWindow.h"
#include "GameLoadingPanel.h"
#include "Hardpoint.h"
#include "Logger.h"
#include "MenuAnimationPanel.h"
#include "MenuPanel.h"
#include "Outfit.h"
#include "Panel.h"
#include "PlayerInfo.h"
#include "Preferences.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "TaskQueue.h"
#include "Test.h"
#include "TestContext.h"
#include "UI.h"
#include "Version.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "nfd.h"

#include <chrono>
#include <iostream>
#include <map>
#include <thread>

#include <cassert>
#include <future>
#include <exception>
#include <string>

#ifdef _WIN32
#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

using namespace std;

void PrintHelp();
void PrintVersion();
void GameLoop();
#ifdef _WIN32
void InitConsole();
#endif



// Entry point for the EndlessSky executable
int main(int argc, char *argv[])
{
	// Handle command-line arguments
#ifdef _WIN32
	if(argc > 1)
		InitConsole();
#endif
	// Ensure that we log errors to the errors.txt file.
	Logger::SetLogErrorCallback([](const string &errorMessage) { Files::LogErrorToFile(errorMessage); });

	for(const char *const *it = argv + 1; *it; ++it)
	{
		string arg = *it;
		if(arg == "-h" || arg == "--help")
		{
			PrintHelp();
			return 0;
		}
		else if(arg == "-v" || arg == "--version")
		{
			PrintVersion();
			return 0;
		}
	}
	Files::Init(argv);

	try {
		TaskQueue _;

		// OpenAL needs to be initialized before we begin loading any sounds/music.
		Audio::Init();

		// Begin loading the game data.
		future<void> dataLoading = GameData::BeginLoad(0);

		// On Windows, make sure that the sleep timer has at least 1 ms resolution
		// to avoid irregular frame rates.
#ifdef _WIN32
		timeBeginPeriod(1);
#endif

		Preferences::Load();

		if(!GameWindow::Init([](SDL_Window *window, const SDL_GLContext &context)
			{
				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
				//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

				ImGui_ImplSDL2_InitForOpenGL(window, context);

				SDL_SetWindowTitle(window, "Editor");
			}))
			return 1;

		ImGui_ImplOpenGL3_Init(nullptr);
		NFD_Init();


		GameData::LoadShaders(!GameWindow::HasSwizzle());

		// Show something other than a blank window.
		GameWindow::Step();

		// This is the main loop where all the action begins.
		GameLoop();
	}
	catch(const exception &error)
	{
		Audio::Quit();
		GameWindow::ExitWithError(error.what(), true);
		return 1;
	}

	// Remember the window state and preferences if quitting normally.
	Preferences::Set("maximized", GameWindow::IsMaximized());
	Preferences::Set("fullscreen", GameWindow::IsFullscreen());
	Screen::SetRaw(GameWindow::Width(), GameWindow::Height());
	Preferences::Save();

	NFD_Quit();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	Audio::Quit();
	GameWindow::Quit();

	return 0;
}



void GameLoop()
{
	// Panels shown to the user. Unlike the proper game, there is only one group
	// of panels.
	UI panels;

	Editor editor(panels);

	// Whether the game data is done loading. This is used to trigger any
	// tests to run.
	bool dataFinishedLoading = false;
	panels.Push(new GameLoadingPanel([&panels, &editor] (GameLoadingPanel *This)
		{
			panels.Pop(This);
			editor.Initialize();
		}, dataFinishedLoading));

	int frameRate = 60;
	FrameTimer timer(frameRate);
	bool isPaused = false;

	// Limit how quickly full-screen mode can be toggled.
	int toggleTimeout = 0;

	const auto &io = ImGui::GetIO();
	// IsDone becomes true when the game is quit.
	while(!panels.IsDone())
	{
		if(toggleTimeout)
			--toggleTimeout;

		// Handle any events that occurred in this frame.
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			if(!io.WantCaptureKeyboard && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE)
				isPaused = !isPaused;
			else if(event.type == SDL_QUIT)
				panels.Quit();
			else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				// The window has been resized. Adjust the raw screen size
				// and the OpenGL viewport to match.
				GameWindow::AdjustViewport();
			}
			else if(((!io.WantCaptureKeyboard && event.type == SDL_KEYDOWN) ||
						(!io.WantCaptureMouse && (event.type == SDL_MOUSEMOTION
						|| event.type == SDL_MOUSEBUTTONDOWN
						|| event.type == SDL_MOUSEBUTTONUP
						|| event.type == SDL_MOUSEWHEEL))) && panels.Handle(event))
			{
				// The UI handled the event.
			}
			else if(!io.WantCaptureKeyboard && event.type == SDL_KEYDOWN && !toggleTimeout
					&& (Command(event.key.keysym.sym).Has(Command::FULLSCREEN)
					|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))))
			{
				toggleTimeout = 30;
				GameWindow::ToggleFullscreen();
			}

			ImGui_ImplSDL2_ProcessEvent(&event);
		}
		SDL_Keymod mod = SDL_GetModState();
		Font::ShowUnderlines((mod & KMOD_ALT) && !io.WantCaptureKeyboard);

		// Tell all the panels to step forward, then draw them.
		panels.StepAll();

		// Process any tasks to execute.
		TaskQueue::ProcessTasks();

		Audio::Step();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		if(dataFinishedLoading)
			editor.RenderMain();
		ImGui::Render();

		// Events in this frame may have cleared out the menu, in which case
		// we should draw the game panels instead:
		panels.DrawAll();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);

		GameWindow::Step();

		// Lock the framerate to 60fps.
		timer.Wait();
	}
}



void PrintHelp()
{
	cerr << endl;
	cerr << "Command line options:" << endl;
	cerr << "    -h, --help: print this help message." << endl;
	cerr << "    -v, --version: print version information." << endl;
	cerr << endl;
	cerr << "Report bugs to: <https://github.com/quyykk/editor/issues>" << endl;
	cerr << endl;
}



void PrintVersion()
{
	cerr << endl;
	cerr << "Editor, compatible with Endless Sky " << ES_VERSION << endl;
	cerr << "License GPLv3+: GNU GPL version 3 or later: <https://gnu.org/licenses/gpl.html>" << endl;
	cerr << "This is free software: you are free to change and redistribute it." << endl;
	cerr << "There is NO WARRANTY, to the extent permitted by law." << endl;
	cerr << endl;
	cerr << GameWindow::SDLVersions() << endl;
	cerr << endl;
}



#ifdef _WIN32
void InitConsole()
{
	const int UNINITIALIZED = -2;
	bool redirectStdout = _fileno(stdout) == UNINITIALIZED;
	bool redirectStderr = _fileno(stderr) == UNINITIALIZED;
	bool redirectStdin = _fileno(stdin) == UNINITIALIZED;

	// Bail if stdin, stdout, and stderr are already initialized (e.g. writing to a file)
	if(!redirectStdout && !redirectStderr && !redirectStdin)
		return;

	// Bail if we fail to attach to the console
	if(!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole())
		return;

	// Perform console redirection.
	if(redirectStdout)
	{
		FILE *fstdout = nullptr;
		freopen_s(&fstdout, "CONOUT$", "w", stdout);
		if(fstdout)
			setvbuf(stdout, nullptr, _IOFBF, 4096);
	}
	if(redirectStderr)
	{
		FILE *fstderr = nullptr;
		freopen_s(&fstderr, "CONOUT$", "w", stderr);
		if(fstderr)
			setvbuf(stderr, nullptr, _IOLBF, 1024);
	}
	if(redirectStdin)
	{
		FILE *fstdin = nullptr;
		freopen_s(&fstdin, "CONIN$", "r", stdin);
		if(fstdin)
			setvbuf(stdin, nullptr, _IONBF, 0);
	}
}
#endif
