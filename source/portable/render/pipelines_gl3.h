#pragma once
// Stream R / R-6: the stock librw gl3 object pipelines (default, skin, matfx) as the portable build's renderer for SA atomics.
#include "fakerw.h"

//! True when librw's gl3 default / skin / matfx pipelines exist (they are created by Engine::open -> gl3 driver open and the Skin / MatFX plugin driver hooks).
bool NotsaGl3StockPipelinesReady();

//! R-10: logs the gl3 skin pipeline counters (geometries instanced / weights renormalised / bone indices clamped / over the 64-bone cap / draws without a hierarchy / hierarchy size mismatches).
void NotsaGl3LogSkinStats();
