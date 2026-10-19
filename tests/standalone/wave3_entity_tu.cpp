// ORACLE-W3-ENTITY: Entity.cpp compiled into this TU so the file-static RenderWare callbacks its header declares (SetCompAlphaCB, SetAtomicAlpha, ...) can be called by the oracle rows through the exported wrappers below.
#include "game_sa/Entity/Entity.cpp"

RpMaterial* W3E_SetCompAlphaCB(RpMaterial* material, void* data) { return SetCompAlphaCB(material, data); }
