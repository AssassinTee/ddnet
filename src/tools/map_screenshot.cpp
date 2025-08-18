#include <base/logger.h>
#include <base/system.h>
#include <engine/console.h>
#include <engine/config.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <game/map/map_renderer.h>
#include <game/map/render_map.h>

static const char *TOOL_NAME = "map_screenshot";

/*
#include "SDL.h"
#ifdef main
#undef main
#endif
*/

class CToolMapScreenshot : public IEnvelopeEval, public IMapImages
{
	IGraphics *m_pGraphics;
	ITextRender *m_pTextRender;
	IStorage *m_pStorage;
	IEngineMap *m_pMap;

public:
	CToolMapScreenshot(IEngineMap *pMap, IGraphics *pGraphics, ITextRender *pTextRender, IStorage *pStorage);
	void LoadMap(const char *pMapName, const char *pOutMapName);
	void RenderMapFast();
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels) override;

	void LoadMapImages(class CLayers *pLayers);

	IGraphics::CTextureHandle Get(int Index) const override;
	int Num() const override;
	// DDRace
	IGraphics::CTextureHandle GetEntities(EMapImageEntityLayerType EntityLayerType) override { return IGraphics::CTextureHandle(); }
	IGraphics::CTextureHandle GetSpeedupArrow() override { return IGraphics::CTextureHandle(); }

	IGraphics::CTextureHandle GetOverlayBottom() override { return IGraphics::CTextureHandle(); }
	IGraphics::CTextureHandle GetOverlayTop() override { return IGraphics::CTextureHandle(); }
	IGraphics::CTextureHandle GetOverlayCenter() override { return IGraphics::CTextureHandle(); }

private:
	void GenerateOutName();
	void CheckSuffix(const char *pOutMapName, const char *pSuffix);

	const char *m_pMapName;
	std::optional<const char *> m_pMapOutName;
	CRenderMap m_RenderMap;
	std::shared_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePointAccess;

	int m_MapImageCount;
	IGraphics::CTextureHandle m_aTextures[MAX_MAPIMAGES];
	CLayers m_Layers;
};

CToolMapScreenshot::CToolMapScreenshot(IEngineMap *pMap, IGraphics *pGraphics, ITextRender *pTextRender, IStorage *pStorage)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	m_pStorage = pStorage;
	m_RenderMap.Init(pGraphics, pTextRender);
	m_pMap = pMap;
}

void CToolMapScreenshot::GenerateOutName()
{
	m_pMapOutName = "TODO_GENERATE_Properly.png";
}

// from void CMapImages::OnMapLoadImpl(class CLayers *pLayers, IMap *pMap)
void CToolMapScreenshot::LoadMapImages(class CLayers *pLayers)
{
	int Start;
	m_pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_MapImageCount);
	m_MapImageCount = std::clamp<int>(m_MapImageCount, 0, MAX_MAPIMAGES);

	unsigned char aTextureUsedByTileOrQuadLayerFlag[MAX_MAPIMAGES] = {0}; // 0: nothing, 1(as flag): tile layer, 2(as flag): quad layer
	for(int GroupIndex = 0; GroupIndex < pLayers->NumGroups(); GroupIndex++)
	{
		const CMapItemGroup *pGroup = pLayers->GetGroup(GroupIndex);
		if(!pGroup)
		{
			continue;
		}

		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			const CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer + LayerIndex);
			if(!pLayer)
			{
				continue;
			}

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				const CMapItemLayerTilemap *pLayerTilemap = reinterpret_cast<const CMapItemLayerTilemap *>(pLayer);
				if(pLayerTilemap->m_Image >= 0 && pLayerTilemap->m_Image < m_MapImageCount)
				{
					aTextureUsedByTileOrQuadLayerFlag[pLayerTilemap->m_Image] |= 1;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				const CMapItemLayerQuads *pLayerQuads = reinterpret_cast<const CMapItemLayerQuads *>(pLayer);
				if(pLayerQuads->m_Image >= 0 && pLayerQuads->m_Image < m_MapImageCount)
				{
					aTextureUsedByTileOrQuadLayerFlag[pLayerQuads->m_Image] |= 2;
				}
			}
		}
	}

	const int TextureLoadFlag = m_pGraphics->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;

	// load new textures
	bool ShowWarning = false;
	for(int i = 0; i < m_MapImageCount; i++)
	{
		if(aTextureUsedByTileOrQuadLayerFlag[i] == 0)
		{
			// skip loading unused images
			continue;
		}

		const int LoadFlag = (((aTextureUsedByTileOrQuadLayerFlag[i] & 1) != 0) ? TextureLoadFlag : 0) | (((aTextureUsedByTileOrQuadLayerFlag[i] & 2) != 0) ? 0 : (m_pGraphics->HasTextureArraysSupport() ? IGraphics::TEXLOAD_NO_2D_TEXTURE : 0));
		const CMapItemImage_v2 *pImg = static_cast<const CMapItemImage_v2 *>(m_pMap->GetItem(Start + i));

		const char *pName = m_pMap->GetDataString(pImg->m_ImageName);
		if(pName == nullptr || pName[0] == '\0')
		{
			if(pImg->m_External)
			{
				log_error("mapimages", "Failed to load map image %d: failed to load name.", i);
				ShowWarning = true;
				continue;
			}
			pName = "(error)";
		}

		if(pImg->m_Version > 1 && pImg->m_MustBe1 != 1)
		{
			log_error("mapimages", "Failed to load map image %d '%s': invalid map image type.", i, pName);
			ShowWarning = true;
			continue;
		}

		if(pImg->m_External)
		{
			char aPath[IO_MAX_PATH_LENGTH];
			bool Translated = false;
			/*
			if(Client()->IsSixup())
			{
				Translated =
					!str_comp(pName, "grass_doodads") ||
					!str_comp(pName, "grass_main") ||
					!str_comp(pName, "winter_main") ||
					!str_comp(pName, "generic_unhookable");
			}*/
			str_format(aPath, sizeof(aPath), "mapres/%s%s.png", pName, Translated ? "_0.7" : "");
			m_aTextures[i] = m_pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL, LoadFlag);
		}
		else
		{
			CImageInfo ImageInfo;
			ImageInfo.m_Width = pImg->m_Width;
			ImageInfo.m_Height = pImg->m_Height;
			ImageInfo.m_Format = CImageInfo::FORMAT_RGBA;
			ImageInfo.m_pData = static_cast<uint8_t *>(m_pMap->GetData(pImg->m_ImageData));
			if(ImageInfo.m_pData && (size_t)m_pMap->GetDataSize(pImg->m_ImageData) >= ImageInfo.DataSize())
			{
				char aTexName[IO_MAX_PATH_LENGTH];
				str_format(aTexName, sizeof(aTexName), "embedded: %s", pName);
				m_aTextures[i] = m_pGraphics->LoadTextureRaw(ImageInfo, LoadFlag, aTexName);
				m_pMap->UnloadData(pImg->m_ImageData);
			}
			else
			{
				m_pMap->UnloadData(pImg->m_ImageData);
				log_error("mapimages", "Failed to load map image %d: failed to load data.", i);
				ShowWarning = true;
				continue;
			}
		}
		m_pMap->UnloadData(pImg->m_ImageName);
		ShowWarning = ShowWarning || m_aTextures[i].IsNullTexture();
	}
	if(ShowWarning)
	{
		log_warn(TOOL_NAME, "Some map images could not be loaded. Check the local console for details.");
	}
}

void CToolMapScreenshot::LoadMap(const char *pMapName, const char *pOutMapName)
{
	// check names
	CheckSuffix(pMapName, ".map");
	CheckSuffix(pOutMapName, ".png");
	m_pMapName = pMapName;
	if(pOutMapName)
		m_pMapOutName = pOutMapName;
	else
		GenerateOutName();

	// load map
	if(!m_pMap->Load(pMapName))
	{
		dbg_msg(TOOL_NAME, "could not load map %s", pMapName);
		std::exit(EXIT_FAILURE);
	}

	// envelope access
	m_pEnvelopePointAccess = std::make_shared<CMapBasedEnvelopePointAccess>(m_pMap);

	// map images
	m_Layers.Init(m_pMap, false);
	LoadMapImages(&m_Layers);
}

void CToolMapScreenshot::RenderMapFast()
{
	CMapRenderer MapRenderer;
	MapRenderer.OnInit(m_pGraphics, m_pTextRender, &m_RenderMap);
	if(!m_pGraphics->IsTileBufferingEnabled() || !m_pGraphics->IsQuadBufferingEnabled())
		dbg_msg(TOOL_NAME, "Some buffering options are not available");
	FRenderUploadCallback FRenderCallback = [&](const char *pTitle, const char *pMessage, int IncreaseCounter) { dbg_msg(TOOL_NAME, "MapRenderer: %s: %s, %d", pTitle, pMessage, IncreaseCounter); };
	std::optional<FRenderUploadCallback> NoCallback = FRenderCallback;
	MapRenderer.Load(RENDERTYPE_ALL, &m_Layers, this, this, NoCallback);

	dbg_msg("dbg", "finished loading stuff");

	IEngineGraphics *pEngineGraphics = (IEngineGraphics*)m_pGraphics;

	m_pGraphics->Clear(0, 0, 0);
	m_pGraphics->Swap();
	//pEngineGraphics->Set
	m_pGraphics->Resize(400, 300, 60);
	m_pGraphics->UpdateViewport(0, 0, 400, 300, false);

	CRenderLayerParams Params;
	Params.m_Center = vec2(0, 0);
	Params.m_EntityOverlayVal = 0;
	Params.m_RenderType = RENDERTYPE_ALL;
	Params.m_RenderInvalidTiles = false;
	Params.m_RenderText = false;
	Params.m_RenderTileBorder = true;
	Params.m_TileAndQuadBuffering = true;
	Params.m_Zoom = 1.0f;

	int seconds = 3;
	
	for(int i = 0; i < 10000 * seconds; ++i)
	{
		m_pGraphics->MapScreen(0.0f, 0.0f, 400.0f, 300.0f);
		MapRenderer.Render(Params);
		if(i == 10000)
			m_pGraphics->TakeCustomScreenshot("test.png");
		m_pGraphics->Swap();
		m_pGraphics->Clear(0, 0, 0);
	}
}

void CToolMapScreenshot::CheckSuffix(const char *pOutMapName, const char *pSuffix)
{
	if(!pOutMapName)
		return;

	if(str_endswith(pOutMapName, pSuffix) == nullptr)
	{
		dbg_msg(TOOL_NAME, "map does not contain .map suffix");
		std::exit(EXIT_FAILURE);
	}
}

void CToolMapScreenshot::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels)
{
	// no animations, just render with it's offset
	m_RenderMap.RenderEvalEnvelope(m_pEnvelopePointAccess.get(), std::chrono::milliseconds(TimeOffsetMillis), Result, Channels);
}

IGraphics::CTextureHandle CToolMapScreenshot::Get(int Index) const
{
	if(m_aTextures[Index].IsValid())
		dbg_msg("dbg", "valid Index %d", Index);
	return m_aTextures[Index];
}

int CToolMapScreenshot::Num() const
{
	return m_MapImageCount;
}

int main(int argc, const char *argv[])
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	bool ModeFast = argc >= 2 && argc <= 3;
	bool ModePreview = argc >= 4 && argc <= 5;
	bool ModeExpert = argc >= 7 && argc <= 8;
	bool HasOutput = argc == 3 || argc == 5 || argc == 8;

	/*if(!ModeFast && !ModePreview && !ModeExpert)
	{
		dbg_msg(TOOL_NAME, "Invalid arguments");
		dbg_msg(TOOL_NAME, "Usage 1: %s <dm1.map> [<output.png>]\n Render all of the map into a screenshot pixel perfect", argv[0]);
		dbg_msg(TOOL_NAME, "Usage 2: %s <dm1.map> <pixel-width> <pixel-height> [<output.png>]", argv[0]);
		dbg_msg(TOOL_NAME, "Render All of the <map> into a <width>x<height> image");
		dbg_msg(TOOL_NAME, "Usage 3: %s <dm1.map> <pixel-width> <pixel-height> <pos-x>, <pos-y> <zoom> [<output.png>]", argv[0]);
		dbg_msg(TOOL_NAME, "Render an image of a <map> with <width>x<height> pixels at map position <pos-x> <pos-y> with zoom level <zoom>");
		dbg_msg(TOOL_NAME, "Note: a tile contains 32x32 pixels, <pos-x> and <pos-y> expects tile cooridnates");

		return -1;
	}*/

	//const char *MapName = argv[1];
	const char *MapName = "data/maps/dm1.map";
	//const char *MapName = "~/AppData/Roaming/Teeworlds/maps/KingsLeap.map";
	const char *MapOutName = nullptr;

	std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();

	// create the components
	IKernel *pKernel = IKernel::Create();

	// Engine
	IEngine *pEngine = CreateEngine(TOOL_NAME, pFutureConsoleLogger);
	pKernel->RegisterInterface(pEngine, false);

	// Storage
	IStorage* pStorage = CreateStorage(IStorage::EInitializationType::CLIENT, 1, argv);
	pKernel->RegisterInterface(pStorage);
	//pKernel->RegisterInterface(static_cast<IStorage *>(pStorage), false);

	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT).release();
	pKernel->RegisterInterface(pConsole);

	IConfigManager *pConfigManager = CreateConfigManager();
	pKernel->RegisterInterface(pConfigManager);

	IEngineTextRender *pEngineTextRender = CreateEngineTextRender();
	pKernel->RegisterInterface(pEngineTextRender); // IEngineTextRender
	pKernel->RegisterInterface(static_cast<ITextRender *>(pEngineTextRender), false);

	IEngineMap *pEngineMap = CreateEngineMap();
	pKernel->RegisterInterface(pEngineMap); // IEngineMap
	pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

	pEngine->Init();
	pConsole->Init();
	pConfigManager->Init();
	//pEngineTextRender->Init();

	/*
	// Do not automatically translate touch events to mouse events and vice versa.
	SDL_SetHint("SDL_TOUCH_MOUSE_EVENTS", "0");
	SDL_SetHint("SDL_MOUSE_TOUCH_EVENTS", "0");

	// Support longer IME composition strings (enables SDL_TEXTEDITING_EXT).
#if SDL_VERSION_ATLEAST(2, 0, 22)
	SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");
#endif

#if defined(CONF_PLATFORM_MACOS)
	// Hints will not be set if there is an existing override hint or environment variable that takes precedence.
	// So this respects cli environment overrides.
	SDL_SetHint("SDL_MAC_OPENGL_ASYNC_DISPATCH", "1");
#endif

#if defined(CONF_FAMILY_WINDOWS)
	SDL_SetHint("SDL_IME_SHOW_UI", g_Config.m_InpImeNativeUi ? "1" : "0");
#else
	SDL_SetHint("SDL_IME_SHOW_UI", "1");
#endif

#if defined(CONF_PLATFORM_ANDROID)
	// Trap the Android back button so it can be handled in our code reliably
	// instead of letting the system handle it.
	SDL_SetHint("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
	// Force landscape screen orientation.
	SDL_SetHint("SDL_IOS_ORIENTATIONS", "LandscapeLeft LandscapeRight");
#endif

	if(SDL_Init(0) < 0)
	{
		char aError[256];
		str_format(aError, sizeof(aError), "Unable to initialize SDL base: %s", SDL_GetError());
		log_error(TOOL_NAME, "%s", aError);
		return -1;
	}
	*/

	// doesn't work
	str_copy(pConfigManager->Values()->m_GfxBackend, "vulkan");

	IEngineGraphics *pGraphics = CreateEngineGraphicsThreaded();
	pKernel->RegisterInterface(pGraphics); // IEngineTextRender
	pKernel->RegisterInterface(static_cast<IGraphics *>(pGraphics), false);

	pGraphics->Init();
	dbg_assert(pGraphics->IsBackendInitialized(), "Graphics backend not initialized");

	dbg_msg("dbg", "Finished initializing backends");

	CToolMapScreenshot MapTool(pEngineMap, pGraphics, pEngineTextRender, pStorage);
	if(ModeFast || true)
	{
		if(HasOutput)
			MapOutName = argv[2];
		MapTool.LoadMap(MapName, MapOutName);
		MapTool.RenderMapFast();
	}
	dbg_msg("dbg", "Success!");
	return 0;
}