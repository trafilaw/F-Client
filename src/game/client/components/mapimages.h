/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#define GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#include <game/client/component.h>

class CMapImages : public CComponent
{
	friend class CBackground;

	enum
	{
		MAX_TEXTURES=64,

		MAP_TYPE_GAME=0,
		MAP_TYPE_MENU,
		NUM_MAP_TYPES
	};
	struct
	{
		IGraphics::CTextureHandle m_aTextures[MAX_TEXTURES];
		int m_Count;
	} m_Info[NUM_MAP_TYPES];

	IGraphics::CTextureHandle m_EasterTexture;
	bool m_EasterIsLoaded;

	void LoadMapImages(class IMap *pMap, class CLayers *pLayers, int MapType);

	const char *m_pEntitiesGameType;

public:
	CMapImages();

	IGraphics::CTextureHandle Get(int Index) const;
	int Num() const;

	virtual void OnMapLoad();
	void OnMenuMapLoad(class IMap *pMap);
	void LoadBackground(class IMap *pMap);
	
	IGraphics::CTextureHandle GetEasterTexture();

	IGraphics::CTextureHandle GetEntities();

	bool m_EntitiesIsLoaded;
	IGraphics::CTextureHandle m_EntitiesTextures;
	IGraphics::CTextureHandle m_OverlayBottomTexture;
	IGraphics::CTextureHandle m_OverlayTopTexture;
	IGraphics::CTextureHandle m_OverlayCenterTexture;
};

#endif
