/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/array.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/demo.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/render.h>
#include <game/client/components/mapimages.h>

#include "camera.h"
#include "mapimages.h"
#include "menus.h"
#include "maplayers.h"

CMapLayers::CMapLayers(int t)
{
	m_Type = t;
	m_CurrentLocalTick = 0;
	m_LastLocalTick = 0;
	m_EnvelopeUpdate = false;
	m_pMenuMap = 0;
	m_pMenuLayers = 0;
	m_OnlineStartTime = 0;
}

void CMapLayers::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE)
		m_OnlineStartTime = Client()->LocalTime(); // reset time for non-scynchronized envelopes
}

void CMapLayers::LoadBackgroundMap()
{
	if(!Config()->m_ClShowMenuMap)
		return;

	int HourOfTheDay = time_houroftheday();
	char aBuf[128];
	// check for the appropriate day/night map
	str_format(aBuf, sizeof(aBuf), "ui/themes/%s_%s.map", Config()->m_ClMenuMap, (HourOfTheDay >= 6 && HourOfTheDay < 18) ? "day" : "night");
	if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
	{
		// fall back on generic map
		str_format(aBuf, sizeof(aBuf), "ui/themes/%s.map", Config()->m_ClMenuMap);
		if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
		{
			// fall back on day/night alternative map
			str_format(aBuf, sizeof(aBuf), "ui/themes/%s_%s.map", Config()->m_ClMenuMap, (HourOfTheDay >= 6 && HourOfTheDay < 18) ? "night" : "day");
			if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
			{
				str_format(aBuf, sizeof(aBuf), "map '%s' not found", Config()->m_ClMenuMap);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
				return;
			}
		}
	}

	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", Config()->m_ClMenuMap);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);

	m_pMenuLayers->Init(Kernel(), m_pMenuMap);
	m_pClient->m_pMapimages->OnMenuMapLoad(m_pMenuMap);
	LoadEnvPoints(m_pMenuLayers, m_lEnvPointsMenu);
}

int CMapLayers::GetInitAmount() const
{
	if(m_Type == TYPE_BACKGROUND)
		return 15;
	return 0;
}

void CMapLayers::OnInit()
{
	if(m_Type == TYPE_BACKGROUND)
	{
		m_pMenuLayers = new CLayers;
		m_pMenuMap = CreateEngineMap();
		m_pClient->m_pMenus->RenderLoading(1);
		LoadBackgroundMap();
		m_pClient->m_pMenus->RenderLoading(14);
	}

	m_pEggTiles = 0;
}

static void PlaceEggDoodads(int LayerWidth, int LayerHeight, CTile* aOutTiles, CTile* aGameLayerTiles, int ItemWidth, int ItemHeight, const int* aImageTileID, int ImageTileIDCount, int Freq)
{
	for(int y = 0; y < LayerHeight-ItemHeight; y++)
	{
		for(int x = 0; x < LayerWidth-ItemWidth; x++)
		{
			bool Overlap = false;
			bool ObstructedByWall = false;
			bool HasGround = true;

			for(int iy = 0; iy < ItemHeight; iy++)
			{
				for(int ix = 0; ix < ItemWidth; ix++)
				{
					int Tid = (y+iy) * LayerWidth + (x+ix);
					int DownTid = (y+iy+1) * LayerWidth + (x+ix);

					if(aOutTiles[Tid].m_Index != 0)
					{
						Overlap = true;
						break;
					}

					if(aGameLayerTiles[Tid].m_Index == 1)
					{
						ObstructedByWall = true;
						break;
					}

					if(iy == ItemHeight-1 && aGameLayerTiles[DownTid].m_Index != 1)
					{
						HasGround = false;
						break;
					}
				}
			}

			if(!Overlap && !ObstructedByWall && HasGround && random_int()%Freq == 0)
			{
				const int BaskerStartID = aImageTileID[random_int()%ImageTileIDCount];

				for(int iy = 0; iy < ItemHeight; iy++)
				{
					for(int ix = 0; ix < ItemWidth; ix++)
					{
						int Tid = (y+iy) * LayerWidth + (x+ix);
						aOutTiles[Tid].m_Index = BaskerStartID + iy * 16 + ix;
					}
				}
			}
		}
	}
}

void CMapLayers::OnMapLoad()
{
	if(Layers())
		LoadEnvPoints(Layers(), m_lEnvPoints);

	// easter time, place eggs
	if(m_pClient->IsEaster())
	{
		CMapItemLayerTilemap* pGameLayer = Layers()->GameLayer();
		if(m_pEggTiles)
			mem_free(m_pEggTiles);

		m_EggLayerWidth = pGameLayer->m_Width;
		m_EggLayerHeight = pGameLayer->m_Height;
		m_pEggTiles = (CTile*)mem_alloc(sizeof(CTile) * m_EggLayerWidth * m_EggLayerHeight,1);
		mem_zero(m_pEggTiles, sizeof(CTile) * m_EggLayerWidth * m_EggLayerHeight);
		CTile* aGameLayerTiles = (CTile*)Layers()->Map()->GetData(pGameLayer->m_Data);

		// first pass: baskets
		static const int s_aBasketIDs[] = {
			38,
			86
		};

		static const int s_BasketCount = sizeof(s_aBasketIDs)/sizeof(s_aBasketIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 3, 2, s_aBasketIDs, s_BasketCount, 250);

		// second pass: double eggs
		static const int s_aDoubleEggIDs[] = {
			9,
			25,
			41,
			57,
			73,
			89
		};

		static const int s_DoubleEggCount = sizeof(s_aDoubleEggIDs)/sizeof(s_aDoubleEggIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 2, 1, s_aDoubleEggIDs, s_DoubleEggCount, 100);

		// third pass: eggs
		static const int s_aEggIDs[] = {
			1, 2, 3, 4, 5,
			17, 18, 19, 20,
			33, 34, 35, 36,
			49, 50,     52,
			65, 66,
				82,
				98
		};

		static const int s_EggCount = sizeof(s_aEggIDs)/sizeof(s_aEggIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 1, 1, s_aEggIDs, s_EggCount, 30);
	}
}

void CMapLayers::OnShutdown()
{
	if(m_pEggTiles)
	{
		mem_free(m_pEggTiles);
		m_pEggTiles = 0;
	}
}

void CMapLayers::LoadEnvPoints(const CLayers *pLayers, array<CEnvPoint>& lEnvPoints)
{
	lEnvPoints.clear();

	// get envelope points
	CEnvPoint *pPoints = 0x0;
	{
		int Start, Num;
		pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);

		if(!Num)
			return;

		pPoints = (CEnvPoint *)pLayers->Map()->GetItem(Start, 0, 0);
	}

	// get envelopes
	int Start, Num;
	pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
	if(!Num)
		return;


	for(int env = 0; env < Num; env++)
	{
		CMapItemEnvelope *pItem = (CMapItemEnvelope *)pLayers->Map()->GetItem(Start+env, 0, 0);

		if(pItem->m_Version >= 3)
		{
			for(int i = 0; i < pItem->m_NumPoints; i++)
				lEnvPoints.add(pPoints[i + pItem->m_StartPoint]);
		}
		else
		{
			// backwards compatibility
			for(int i = 0; i < pItem->m_NumPoints; i++)
			{
				// convert CEnvPoint_v1 -> CEnvPoint
				CEnvPoint_v1 *pEnvPoint_v1 = &((CEnvPoint_v1 *)pPoints)[i + pItem->m_StartPoint];
				CEnvPoint p;

				p.m_Time = pEnvPoint_v1->m_Time;
				p.m_Curvetype = pEnvPoint_v1->m_Curvetype;

				for(int c = 0; c < pItem->m_Channels; c++)
				{
					p.m_aValues[c] = pEnvPoint_v1->m_aValues[c];
					p.m_aInTangentdx[c] = 0;
					p.m_aInTangentdy[c] = 0;
					p.m_aOutTangentdx[c] = 0;
					p.m_aOutTangentdy[c] = 0;
				}

				lEnvPoints.add(p);
			}
		}
	}
}

void CMapLayers::EnvelopeUpdate()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		m_CurrentLocalTick = pInfo->m_CurrentTick;
		m_LastLocalTick = pInfo->m_CurrentTick;
		m_EnvelopeUpdate = true;
	}
}

void CMapLayers::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;
	pChannels[0] = 0;
	pChannels[1] = 0;
	pChannels[2] = 0;
	pChannels[3] = 0;

	CEnvPoint *pPoints = 0;
	CLayers *pLayers = 0;
	{
		if(pThis->Client()->State() == IClient::STATE_ONLINE || pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			pLayers = pThis->Layers();
			pPoints = pThis->m_lEnvPoints.base_ptr();
		}
		else
		{
			pLayers = pThis->m_pMenuLayers;
			pPoints = pThis->m_lEnvPointsMenu.base_ptr();
		}
	}

	int Start, Num;
	pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pLayers->Map()->GetItem(Start+Env, 0, 0);

	float Time = 0.0f;
	if(pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = pThis->DemoPlayer()->BaseInfo();

		if(!pInfo->m_Paused || pThis->m_EnvelopeUpdate)
		{
			if(pThis->m_CurrentLocalTick != pInfo->m_CurrentTick)
			{
				pThis->m_LastLocalTick = pThis->m_CurrentLocalTick;
				pThis->m_CurrentLocalTick = pInfo->m_CurrentTick;
			}

			Time = mix(pThis->m_LastLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->m_CurrentLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->Client()->IntraGameTick());
		}

		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
	else if(pThis->Client()->State() != IClient::STATE_OFFLINE)
	{
		if(pThis->m_pClient->m_Snap.m_pGameData && !(pThis->m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		{
			if(pItem->m_Version < 2 || pItem->m_Synchronized)
			{
				Time = mix((pThis->Client()->PrevGameTick()-pThis->m_pClient->m_Snap.m_pGameData->m_GameStartTick) / (float)pThis->Client()->GameTickSpeed(),
							(pThis->Client()->GameTick()-pThis->m_pClient->m_Snap.m_pGameData->m_GameStartTick) / (float)pThis->Client()->GameTickSpeed(),
							pThis->Client()->IntraGameTick());
			}
			else
				Time = pThis->Client()->LocalTime()-pThis->m_OnlineStartTime;
		}

		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
	else
	{
		Time = pThis->Client()->LocalTime();
		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
}

void CMapLayers::OnRender()
{
	if((Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK && !m_pMenuMap))
		return;

	CLayers *pLayers = 0;
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pLayers = Layers();
	else if(m_pMenuMap->IsLoaded())
		pLayers = m_pMenuLayers;

	if(!pLayers)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = *m_pClient->m_pCamera->GetCenter();

	bool PassedGameLayer = false;

	for(int g = 0; g < pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = pLayers->GetGroup(g);

		if(!Config()->m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			RenderTools()->MapScreenToGroup(Center.x, Center.y, pLayers->GameGroup(), m_pClient->m_pCamera->GetZoom());
			Graphics()->GetScreen(&Points[0], &Points[1], &Points[2], &Points[3]);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			if(x1 < 0.0f || x0 > 1.0f || y1 < 0.0f || y0 > 1.0f)
				continue;

			Graphics()->ClipEnable((int)(x0*Graphics()->ScreenWidth()), (int)(y0*Graphics()->ScreenHeight()),
				(int)((x1-x0)*Graphics()->ScreenWidth()), (int)((y1-y0)*Graphics()->ScreenHeight()));
		}

		if(!Config()->m_ClZoomBackgroundLayers && !pGroup->m_ParallaxX && !pGroup->m_ParallaxY)
			RenderTools()->MapScreenToGroup(Center.x, Center.y, pGroup, 1.0f);
		else
			RenderTools()->MapScreenToGroup(Center.x, Center.y, pGroup, m_pClient->m_pCamera->GetZoom());

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool Render = false;
			bool IsGameLayer = false;
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsEntityLayer = false;

			if(pLayer == (CMapItemLayer*)pLayers->GameLayer())
			{
				IsEntityLayer = IsGameLayer = true;
				PassedGameLayer = true;
			}

			if(pLayer == (CMapItemLayer*)pLayers->FrontLayer())
				IsEntityLayer = IsFrontLayer = true;

			if(pLayer == (CMapItemLayer*)pLayers->SwitchLayer())
				IsEntityLayer = IsSwitchLayer = true;

			if(pLayer == (CMapItemLayer*)pLayers->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if(pLayer == (CMapItemLayer*)pLayers->SpeedupLayer())
				IsEntityLayer = IsSpeedupLayer = true;

			if(pLayer == (CMapItemLayer*)pLayers->TuneLayer())
				IsEntityLayer = IsTuneLayer = true;

			if(m_Type == -1)
				Render = true;
			else if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
					return;
				Render = true;

				if(m_Type == TYPE_BACKGROUND_FORCE)
				{
					if(pLayer->m_Type == LAYERTYPE_TILES && !Config()->m_ClBackgroundShowTilesLayers)
						continue;
				}
			}
			else // TYPE_FOREGROUND
			{
				if(PassedGameLayer && !IsGameLayer)
					Render = true;
			}

			if(Render && pLayer->m_Type == LAYERTYPE_TILES && Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyIsPressed(KEY_LSHIFT) && Input()->KeyPress(KEY_KP_0))
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				CTile *pTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Data);
				CServerInfo CurrentServerInfo;
				Client()->GetServerInfo(&CurrentServerInfo);
				char aFilename[IO_MAX_PATH_LENGTH];
				str_format(aFilename, sizeof(aFilename), "dumps/tilelayer_dump_%s-%d-%d-%dx%d.txt", CurrentServerInfo.m_aMap, g, l, pTMap->m_Width, pTMap->m_Height);
				IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
				if(File)
				{
					for(int y = 0; y < pTMap->m_Height; y++)
					{
						for(int x = 0; x < pTMap->m_Width; x++)
							io_write(File, &(pTiles[y*pTMap->m_Width + x].m_Index), sizeof(pTiles[y*pTMap->m_Width + x].m_Index));
						io_write_newline(File);
					}
					io_close(File);
				}
			}

			// skip rendering if detail layers if not wanted, or is entity layer and we are a background map
			if((pLayer->m_Flags&LAYERFLAG_DETAIL && !Config()->m_GfxHighDetail && !IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE && IsEntityLayer))
				continue;

			bool Online = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;

			if((Render && (!Online || Config()->m_ClOverlayEntities < 100) && !IsGameLayer && !IsFrontLayer && !IsSwitchLayer && !IsTeleLayer && !IsSpeedupLayer && !IsTuneLayer) || (Config()->m_ClOverlayEntities && IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE))
			{
				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
					if(pTMap->m_Image == -1)
					{
						if(!IsGameLayer)
							Graphics()->TextureClear();
						else
							Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());
					}
					else
						Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pTMap->m_Image));

					CTile *pTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Data);
					unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Data);

					if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile)) || pTMap->m_Version >= 4)
					{
						vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
						if (Online)
						{
							if(IsGameLayer && Config()->m_ClOverlayEntities)
								Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
							else if(!IsGameLayer && Config()->m_ClOverlayEntities && !(m_Type == TYPE_BACKGROUND_FORCE))
								Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*(100-Config()->m_ClOverlayEntities)/100.0f);
						}

						Graphics()->BlendNone();
						RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
														EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);

						Graphics()->BlendNormal();

						// draw kill tiles outside the entity clipping rectangle
						if(IsGameLayer && Online)
						{
							// slow blinking to hint that it's not a part of the map
							double Seconds = time_get()/(double)time_freq();
							vec4 ColorHint = vec4(1.0f, 1.0f, 1.0f, 0.3f + 0.7f*(1.0f+sin(2.0f*pi*Seconds/3.f))/2.0f);

							RenderTools()->RenderTileRectangle(-201, -201, pTMap->m_Width+402, pTMap->m_Height+402,
																0, TILE_DEATH, // display air inside, death outside
																32.0f, Color*ColorHint, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
																EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
						}

						RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
														EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
					}
				}
				else if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
					if(pQLayer->m_Image == -1)
						Graphics()->TextureClear();
					else
						Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pQLayer->m_Image));

					CQuad *pQuads = (CQuad *)pLayers->Map()->GetDataSwapped(pQLayer->m_Data);
					if(m_Type == TYPE_BACKGROUND_FORCE)
					{
						if(Config()->m_ClShowQuads)
						{
							Graphics()->BlendNormal();
							RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this, 1.f);
						}
					}
					else
					{
						Graphics()->BlendNormal();
						if (m_pClient->Client()->State() == IClient::STATE_ONLINE || m_pClient->Client()->State() == IClient::STATE_DEMOPLAYBACK)
							RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this);
						else // menu background map
							RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this, 1.f);
					}
				}
			}
			else if(Render && Config()->m_ClOverlayEntities && IsFrontLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());

				CTile *pFrontTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Front);
				unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Front);

				if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile)) || pTMap->m_Version >= 4)
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
					Graphics()->BlendNone();
					RenderTools()->RenderTilemap(pFrontTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
							EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
					Graphics()->BlendNormal();
					RenderTools()->RenderTilemap(pFrontTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
							EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
				}
			}
			else if(Render && Config()->m_ClOverlayEntities && IsSwitchLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());

				CSwitchTile *pSwitchTiles = (CSwitchTile *)pLayers->Map()->GetData(pTMap->m_Switch);
				unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Switch);

				if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CSwitchTile)) || pTMap->m_Version >= 4)
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
					Graphics()->BlendNone();
					RenderTools()->RenderSwitchmap(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
					Graphics()->BlendNormal();
					RenderTools()->RenderSwitchmap(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
					RenderTools()->RenderSwitchOverlay(pSwitchTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Config()->m_ClOverlayEntities/100.0f);
				}
			}
			else if(Render && Config()->m_ClOverlayEntities && IsTeleLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());

				CTeleTile *pTeleTiles = (CTeleTile *)pLayers->Map()->GetData(pTMap->m_Tele);
				unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Tele);

				if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTeleTile)) || pTMap->m_Version >= 4)
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
					Graphics()->BlendNone();
					RenderTools()->RenderTelemap(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
					Graphics()->BlendNormal();
					RenderTools()->RenderTelemap(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
					RenderTools()->RenderTeleOverlay(pTeleTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Config()->m_ClOverlayEntities/100.0f);
				}
			}
			else if(Render && Config()->m_ClOverlayEntities && IsSpeedupLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());

				CSpeedupTile *pSpeedupTiles = (CSpeedupTile *)pLayers->Map()->GetData(pTMap->m_Speedup);
				unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Speedup);

				if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CSpeedupTile)) || pTMap->m_Version >= 4)
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
					Graphics()->BlendNone();
					RenderTools()->RenderSpeedupmap(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
					Graphics()->BlendNormal();
					RenderTools()->RenderSpeedupmap(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
					RenderTools()->RenderSpeedupOverlay(pSpeedupTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Config()->m_ClOverlayEntities/100.0f);
				}
			}
			else if(Render && Config()->m_ClOverlayEntities && IsTuneLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetEntities());

				CTuneTile *pTuneTiles = (CTuneTile *)pLayers->Map()->GetData(pTMap->m_Tune);
				unsigned int Size = pLayers->Map()->GetDataSize(pTMap->m_Tune);

				if((Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTuneTile)) || pTMap->m_Version >= 4)
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*Config()->m_ClOverlayEntities/100.0f);
					Graphics()->BlendNone();
					RenderTools()->RenderTunemap(pTuneTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE);
					Graphics()->BlendNormal();
					RenderTools()->RenderTunemap(pTuneTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT);
				}
			}
		}
		if(!Config()->m_GfxNoclip)
			Graphics()->ClipDisable();
	}

	if(!Config()->m_GfxNoclip)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

void CMapLayers::ConchainBackgroundMap(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CMapLayers*)pUserData)->BackgroundMapUpdate();
}

void CMapLayers::OnConsoleInit()
{
	Console()->Chain("cl_menu_map", ConchainBackgroundMap, this);
	Console()->Chain("cl_show_menu_map", ConchainBackgroundMap, this);
}

void CMapLayers::BackgroundMapUpdate()
{
	if(m_Type == TYPE_BACKGROUND && m_pMenuMap)
	{
		// unload map
		m_pMenuMap->Unload();

		LoadBackgroundMap();
	}
}
