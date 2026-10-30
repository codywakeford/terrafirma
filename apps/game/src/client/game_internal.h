// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "game.h"

#include <AnimatedMeshSceneNode.h>
#include <optional>
#include <vector>
#include "camera.h"
#include "client.h"
#include "client/clientevent.h"
#include "client/game_formspec.h"
#include "client/renderingengine.h"
#include "clientdynamicinfo.h"
#include "clouds.h"
#include "far/view.h"
#include "client/hold.h"
#include "client/hand.h"
#include "sculpt/sculpt.h"
#include "gui/touchcontrols.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "log_internal.h"
#include "sky.h"
#include "util/pointedthing.h"
#include "config.h"
#if USE_SHELL
#include "shell/session.h"
#include "online/host.h"
#endif

/* DO NOT INCLUDE THIS FROM OUTSIDE GAME.CPP */

class Game;
class GameUI;
class SoundMaker;
class Server;
class NodeMetadata;
class ProfilerGraph;
class EventManager;
class GUIChatConsole;
class QuicktuneShortcutter;
struct GameErrorData;

const static float object_hit_delay = 0.2;

const static u16 bbox_debug_flag = scene::EDS_BBOX_ALL;

/* The reason the following structs are not anonymous structs within the
 * class is that they are not used by the majority of member functions and
 * many functions that do require objects of thse types do not modify them
 * (so they can be passed as a const qualified parameter)
 */

struct GameRunData {
	u16 dig_index;
	u16 new_playeritem;
	PointedThing pointed_old;
	bool digging;
	bool punching;
	bool btn_down_for_dig;
	bool dig_instantly;
	bool digging_blocked;
	bool reset_jump_timer;
	float nodig_delay_timer;
	float dig_time;
	float dig_time_complete;
	float repeat_place_timer;
	float object_hit_delay_timer;
	float time_from_last_punch;
	ClientActiveObject *selected_object;

	float jump_timer_up;          // from key up until key down
	float jump_timer_down;        // since last key down
	float jump_timer_down_before; // from key down until key down again

	float damage_flash;
	float update_draw_list_timer;
	float touch_blocks_timer;

	f32 fog_range;

	v3f update_draw_list_last_cam_dir;

	float time_of_day_smooth;
};

struct ClientEventHandler
{
	void (Game::*handler)(ClientEvent *, CameraOrientation *);
};

// Animations are paused by setting their FPS to 0.
// We need to remember the original FPS for each track to allow resumption.
struct PausedNode {
	irr_ptr<scene::AnimatedMeshSceneNode> node;
	struct Track {
		u16 id;
		f32 fps;
	};
	std::vector<Track> tracks;
};

using PausedNodesList = std::vector<PausedNode>;

/* This is not intended to be a public class. If a public class becomes
 * desirable then it may be better to create another 'wrapper' class that
 * hides most of the stuff in this class (nothing in this class is required
 * by any other file) but exposes the public methods/data only.
 */
class Game {
public:
	Game();
	~Game();

	bool startup(volatile std::sig_atomic_t *kill,
			InputHandler *input,
			RenderingEngine *rendering_engine,
			GameStartData &game_params,
			GameErrorData &errordata,
			ChatBackend *chat_backend);

	void run();
	void shutdown();

	Client *getClient() { return client; }

	// Pre-calculated value
	int crack_animation_length;

protected:

	// Basic initialisation
	bool init(GameStartData &start_data);
	bool initSound();
	bool createServer(GameStartData &start_data);
	void copyServerClientCache();

	// Client creation
	bool createClient(const GameStartData &start_data);
	bool initGui();

	// Client connection
	bool connectToServer(const GameStartData &start_data,
			bool *connect_ok, bool *aborted);
	bool getServerContent(bool *aborted);

	// Main loop

	void updateInteractTimers(f32 dtime);
	bool checkConnection();
	void processQueues();
	void updateProfilers(const RunStats &stats, const FpsControl &draw_times, f32 dtime);
	void updateDebugState();
	void updateStats(RunStats *stats, const FpsControl &draw_times, f32 dtime);
	void updateProfilerGraphs(ProfilerGraph *graph);

	// Input related
	void processUserInput(f32 dtime);
	void processKeyInput();
	void processItemSelection(u16 *new_playeritem);
	bool shouldShowTouchControls();

	void dropSelectedItem(bool single_item = false);
	void openConsole(float scale, const wchar_t *line=NULL);
	void toggleFreeMove();
	void toggleFreeMoveAlt();
	void togglePitchMove();
	void toggleFast();
	void toggleNoClip();
	void toggleCinematic();
	void toggleBlockBounds();
	void toggleAutoforward();

	void toggleMinimap(bool shift_pressed);
	// Terra Firma: the Planetarium (the session's: shell/planetarium.h) over
	// the whole window, by its key; and whether it's shown, by the key or the page
	void openPlanetarium();
	void closePlanetarium();
	bool planetariumShown();
	void toggleFog();
	void toggleDebug();
	void toggleUpdateCamera();

	void increaseViewRange();
	void decreaseViewRange();
	void toggleFullViewRange();
	void checkZoomEnabled();

	void updateCameraDirection(CameraOrientation *cam, float dtime);
	void updateCameraOrientation(CameraOrientation *cam, float dtime);
	bool getTogglableKeyState(GameKeyType key, bool toggling_enabled, bool prev_key_state);
	void updatePlayerControl(const CameraOrientation &cam);
	void updatePauseState();
	void step(f32 dtime);
	void processClientEvents(CameraOrientation *cam);
	void updateCameraMode(); // call after changing it
	void updateCameraOffset();
	void updateCamera(f32 dtime);
	void updateSound(f32 dtime);
	void processPlayerInteraction(f32 dtime, bool show_hud);
	/*!
	 * Returns the object or node the player is pointing at.
	 * Also updates the selected thing in the Hud.
	 *
	 * @param[in]  shootline         the shootline, starting from
	 * the camera position. This also gives the maximal distance
	 * of the search.
	 * @param[in]  liquids_pointable if false, liquids are ignored
	 * @param[in]  pointabilities    item specific pointable overriding
	 * @param[in]  look_for_object   if false, objects are ignored
	 * @param[in]  camera_offset     offset of the camera
	 * @param[out] selected_object   the selected object or
	 * NULL if not found
	 */
	PointedThing updatePointedThing(
			const core::line3d<f32> &shootline, bool liquids_pointable,
			const std::optional<Pointabilities> &pointabilities,
			bool look_for_object, const v3s16 &camera_offset);
	void handlePointingAtNothing(const ItemStack &playerItem);
	void handlePointingAtNode(const PointedThing &pointed,
			const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime);
	void handlePointingAtObject(const PointedThing &pointed, const ItemStack &playeritem,
			const ItemStack &hand_item, const v3f &player_position, bool show_debug);
	void handleDigging(const PointedThing &pointed, const v3s16 &nodepos,
			const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime);
	void updateFrame(ProfilerGraph *graph, RunStats *stats, f32 dtime,
			const CameraOrientation &cam);
	void updateClouds(float dtime);
	void updateShadows();
	void drawScene(ProfilerGraph *graph, RunStats *stats);

	// Misc
	void showOverlayMessage(const char *msg, float dtime, int percent,
			float *indef_pos = nullptr);

	inline bool fogEnabled()
	{
		// Client setting only takes effect if fog distance unlimited or debug priv
		if (sky->getFogDistance() < 0 || client->checkPrivilege("debug"))
			return m_cache_enable_fog;
		return true;
	}

	static void settingChangedCallback(const std::string &setting_name, void *data);
	void readSettings();

	inline float getAxisValue(GameKeyType k)
	{
		return input->getAxisValue(k);
	}
	inline bool isKeyDown(GameKeyType k)
	{
		return input->isKeyDown(k);
	}
	inline bool wasKeyDown(GameKeyType k)
	{
		return input->wasKeyDown(k);
	}
	inline bool wasKeyPressed(GameKeyType k)
	{
		return input->wasKeyPressed(k);
	}
	inline bool wasKeyReleased(GameKeyType k)
	{
		return input->wasKeyReleased(k);
	}

#ifdef __ANDROID__
	void handleAndroidChatInput();
#endif

private:
	struct Flags {
		bool disable_camera_update = false;
		/// 0 = no debug text active, see toggleDebug() for the rest
		int debug_state = 0;
	};

	void pauseAnimation();
	void resumeAnimation();

	// ClientEvent handlers
	void handleClientEvent_None(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_PlayerDamage(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_PlayerForceMove(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_DeathscreenLegacy(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowCSMFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowPauseMenuFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HandleParticleEvent(ClientEvent *event,
		CameraOrientation *cam);
	void handleClientEvent_HudAdd(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HudRemove(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HudChange(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetSky(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetSun(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetMoon(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetStars(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_OverrideDayNightRatio(ClientEvent *event,
		CameraOrientation *cam);
	void handleClientEvent_CloudParams(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_UpdateCamera(ClientEvent *event, CameraOrientation *cam);

	void updateChat(f32 dtime);

	bool nodePlacement(const ItemDefinition &selected_def, const ItemStack &selected_item,
		const v3s16 &nodepos, const v3s16 &neighborpos, const PointedThing &pointed,
		const NodeMetadata *meta);
	static const ClientEventHandler clientEventHandler[CLIENTEVENT_MAX];

	f32 getSensitivityScaleFactor() const;

	InputHandler *input = nullptr;

	Client *client = nullptr;
	Server *server = nullptr;

	ClientDynamicInfo client_display_info{};
	float dynamic_info_send_timer = 0;

	IWritableTextureSource *texture_src = nullptr;
	IWritableShaderSource *shader_src = nullptr;

	// When created, these will be filled with data received from the server
	IWritableItemDefManager *itemdef_manager = nullptr;
	NodeDefManager *nodedef_manager = nullptr;
	std::unique_ptr<ItemVisualsManager> m_item_visuals_manager;

	std::unique_ptr<ISoundManager> sound_manager;
	std::unique_ptr<SoundMaker> soundmaker;

	ChatBackend *chat_backend = nullptr;
	CaptureLogOutput m_chat_log_buf;

	EventManager *eventmgr = nullptr;
	QuicktuneShortcutter *quicktune = nullptr;

	std::unique_ptr<GameUI> m_game_ui;
#if USE_SHELL
	std::unique_ptr<shell::Session> m_shell_session;
	// Terra Firma: this world hosted for friends (src/online/host.h); null otherwise.
	// Holds the server's socket handle, so it goes before the server does.
	std::unique_ptr<online::Host> m_online_host;
	// Terra Firma: joining a friend's world: how to say a failed connection
	online::Launch::Kind m_online_kind = online::Launch::Kind::Offline;
	std::string m_online_host_name;
#endif
	irr_ptr<GUIChatConsole> gui_chat_console;
	MapDrawControl *draw_control = nullptr;
	Camera *camera = nullptr;
	irr_ptr<Clouds> clouds;
	irr_ptr<Sky> sky;
	irr_ptr<far::View> far_view; // Terra Firma: the horizon, on a terra world
	int far_body = 0;            // Terra Firma: the atlas body far_view draws
	// Terra Firma: the planet viewer, while it's open (it takes the input)
	bool m_planetarium_alone = false;   // opened by its key (no screen over it): its key and Esc close it
	bool m_planetarium_closing = false; // asked from the input callback: closed next frame
	// Terra Firma: the tools in hand (sprint 45): out (dig and place make
	// strokes, the wheel sizes them), which, and how big (sixteenths)
	Hand m_hand;
	// the focus view, while it's open (sprint 46)
	std::optional<Hold> m_hold;
	// Terra Firma: when the held object's pose was last sent, and what
	f32 m_pose_age = 0;
	std::optional<Hold::Pose> m_pose_sent;
	// Terra Firma: a new object of ours, to focus when it arrives
	std::optional<u16> m_awaited;
	// What the tools are pointed at (sprint 47): the world (piece 0, frame
	// the world's) or a piece in its own grid; the node, point (BS) and
	// face in that frame.
	struct Aim
	{
		u16 piece; // the machine's object id; 0: the world
		u64 part;  // which of its parts
		core::matrix4 frame; // the frame's coordinates (BS) to the world's
		v3s16 node;
		v3f point;
		v3s16 normal;
	};
	std::optional<Aim> aimAt(const PointedThing &pointed) const;
	// the stroke a click would make there, or a drag to `to`
	std::optional<sculpt::Stroke> strokeAt(const Aim &aim, sculpt::Act act,
			std::optional<shape::Sixteenth> to = std::nullopt) const;
	// a shape drawn (sprints 50, 55): pressed on a face, its footprint
	// dragged out on it (none: still a click); let go, it's pulled out of
	// the face (or into it) as the cursor goes, and a click puts it there.
	// A face pulled: its flat stretch, pulled as it's held.
	struct Draw
	{
		Aim start;
		std::optional<shape::Sixteenth> to;
		bool pulling = false;
		s32 deep = 1; // sixteenths, out of the face (in, < 0)
		std::optional<shape::Sixteenth> pull_from; // a face pulled: its stretch's first corner
	};
	// the stroke it's come to, pulled as far as it's been
	std::optional<sculpt::Stroke> drawn(const Draw &draw) const;
	// how far along the start's normal the cursor's ray comes nearest
	// (sixteenths, signed)
	s32 pullDepth(const Draw &draw) const;
	// the flat stretch of face a press on `aim` is on: its corners (none:
	// it's not a face of anything)
	std::optional<std::pair<shape::Sixteenth, shape::Sixteenth>> flatFace(const Aim &aim) const;
	std::optional<Draw> m_draw;
	// where this frame's ray meets the drawn shape's face, as a sixteenth;
	// none where it's the one it started on, or doesn't meet the face
	std::optional<shape::Sixteenth> dragTo(const Draw &draw) const;
	void handleTools(const PointedThing &pointed);
	// one of the page's rail's asks (Hand::Ask), or a key's
	void handleAsk(const Hand::Ask &ask, const PointedThing &pointed);
	void showGhost(const Aim &aim, const sculpt::Stroke &stroke, v3s16 camera_offset);
	core::line3d<f32> m_pointed_from; // this frame's shootline
	// a button held off a held object: dragged, it turns it (on the
	// handle's disc or ring, about theirs; elsewhere about the up, moved
	// across, or the right)
	struct Drag
	{
		bool held = false;
		v2s32 start;
		std::optional<Hold::Grip> grip;
		bool across = false; // off the handle: the first move was across
		bool turned = false; // it's turned something: not a click
		bool place = false;  // the other button's (a click of it: the part's menu)
	} m_drag;
	// a part's menu, opened by the other button clicked on it: where
	// (pixels), and the face it was opened on (sockets go there)
	std::optional<v2s32> m_menu;
	piece::Machine::Face m_menu_face{};
	// the cursor free (Tab): the view stays, clicks go where it points
	bool m_cursor = false;
	// the workbench's frame: its handle turned, its pose said
	void benchStep(f32 dtime);
	// a machine's sockets, drawn
	void showSockets(GenericCAO &obj, bool lit) const;
	// the held object's turning handle, drawn
	void showHandle(v3s16 camera_offset);
	// an object grabbed (the tools away) and carried as the button's held,
	// as far along the look as it was grabbed (sprint 54); pointed at
	// another's face, where it'd go on it: let go, it's joined there
	struct Carry
	{
		u16 object;
		f32 distance; // from the eye to where it was grabbed (BS)
		v3f offset;   // its origin from where it was grabbed (BS), in the view's right, up and forward
		v3f rotation; // as it was grabbed
		f32 yaw;      // the player's, as it was grabbed: it turns as they do
		f32 age;      // since its pose was last sent
		struct Snap
		{
			u16 object;              // what it'd go on
			piece::Machine::Face on; // their face (none of it, onto a socket)
			piece::Machine::Face mine;
			std::optional<u64> socket; // their socket it'd go on
		};
		std::optional<Snap> snap;
	};
	std::optional<Carry> m_carry;
	// the view's own ways (BS-free unit vectors)
	struct View
	{
		v3f right, up, forward;
	};
	static View view(v3f look);
	// the object pointed at in the world, lit
	u16 m_lit = 0;
	void handleCarry(const PointedThing &pointed, f32 dtime);
	void showTool();
	// an object's middle in its own grid (node coordinates); nothing if
	// it's not an object (or gone)
	std::optional<v3f> pieceMiddle(u16 id) const;
	// how far its design reaches from `middle` (nodes)
	f32 pieceRadius(u16 id, v3f middle) const;
	// where a part of a machine sits in it
	const piece::Pose &partPose(u16 id, u64 part) const;
	// holding a machine, the part picked to work on alone; none: all of it
	std::optional<u64> m_part;
	void pickPart(std::optional<u64> part);
	piece::Machine::Face faceOf(const Aim &aim) const;
	// what a socket's mirrored across: the workbench machine's middle, if mirrored
	std::optional<sculpt::Stroke::Mirror> machineMirror() const;
	// a click in a mode that isn't drawing, on what's aimed at
	void clickMode(const Aim &aim);
	// opens the focus view on what's pointed at, or closes it
	// the focus on an object: held while it's open, let go when it closes
	void focusOn(u16 id);
	void closeFocus();
	void toggleFocus();
	// the ghost's outline (world sixteenths), and the stroke it's of
	std::optional<sculpt::Stroke> m_ghost_of;
	std::vector<std::pair<v3s32, v3s32>> m_ghost;
	Hud *hud = nullptr;
	Minimap *mapper = nullptr;
	GameFormSpec m_game_formspec;

	// Map server hud ids to client hud ids
	std::unordered_map<u32, u32> m_hud_server_to_client;

	GameRunData runData;
	Flags m_flags;

	/* 'cache'
	   This class does take ownership/responsibily for cleaning up etc of any of
	   these items (e.g. device)
	*/
	IrrlichtDevice             *device;
	RenderingEngine            *m_rendering_engine;
	video::IVideoDriver        *driver;
	scene::ISceneManager       *smgr;
	volatile std::sig_atomic_t *kill;
	GameErrorData              *errordata;
	PausedNodesList             paused_animated_nodes;

	bool simple_singleplayer_mode;
	/* End 'cache' */

	IntervalLimiter profiler_interval;

	/*
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool m_cache_doubletap_jump;
	bool m_cache_toggle_sneak_key;
	bool m_cache_toggle_aux1_key;
	bool m_cache_enable_fog;
	bool m_cache_enable_noclip;
	bool m_cache_enable_free_move;
	f32  m_cache_mouse_sensitivity;
	f32  m_repeat_place_time;
	f32  m_repeat_dig_time;
	f32  m_cache_cam_smoothing;

	bool m_invert_mouse;
	bool m_enable_hotbar_mouse_wheel;
	bool m_invert_hotbar_mouse_wheel;

	bool m_first_loop_after_window_activation = false;
	bool m_camera_offset_changed = false;
	bool m_game_focused = false;

	bool m_does_lost_focus_pause_game = false;

	// if true, (almost) the whole game is paused
	// this happens in pause menu in singleplayer
	bool m_is_paused = false;

	bool m_touch_simulate_aux1 = false;
	bool isTouchShootlineUsed() const;
#ifdef __ANDROID__
	bool m_android_chat_open = false;
#endif

	float m_shutdown_progress = 0.0f;
};
