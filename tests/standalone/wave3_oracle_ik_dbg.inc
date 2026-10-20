// ORACLE-W3-IK developer probe (W3K_PROBE=1): IKChain_c::MoveBonesToTarget of the port vs the exe vs "the port's algorithm composed from the exe's own RenderWare functions".
// Localises a difference to the glue (float expressions of the port) or to a call into the shim. Not a row: prints counts only.
namespace w3k {
static void ProbeMove() {
    unsigned pc = 0, tmp = 0;
    _controlfp_s(&pc, 0, 0); _controlfp_s(&tmp, _PC_24, _MCW_PC);
    int nPortExe = 0, nCompExe = 0, nPortComp = 0, shown = 0;
    static uint8_t save[0x1000], outP[0x1000], outE[0x1000], outC[0x1000];
    Rng rr(4242);
    for (int it = 0; it < 1500; ++it) {
        BuildMove(rr);
        std::memcpy(save, g_fx, 0x1000);
        CleanX87(); (Ch()->*W2Get(W3K_Move{}))(); std::memcpy(outP, g_fx, 0x1000);
        std::memcpy(g_fx, save, 0x1000);
        CleanX87(); ExeThis<void>(0x6178B0, Ch()); std::memcpy(outE, g_fx, 0x1000);
        std::memcpy(g_fx, save, 0x1000);
        CleanX87();
        {   // the port's algorithm, every call into RenderWare / the bone node an exe function
            float* offWS = &At<float>(kChain + cOffWS); const float* offPos = &At<float>(kChain + cOffPos); const float* eff = &At<float>(kChain + cEffPos);
            if (At<uint8_t>(kChain + cUpdTarget)) std::memcpy(offWS, offPos, 12);
            uint8_t* const root = NdB(0);
            for (int i = 0; i < g_n; ++i) {
                uint8_t* const bone = NdB(i);
                float t[3]; oracle::Fn<void* __cdecl(void*, const void*, const void*)>(0x7EDDC0)(t, eff, root + oWld);
                const float* rp = reinterpret_cast<const float*>(root + oWld + 0x30); const float* bp = reinterpret_cast<const float*>(bone + oWld + 0x30);
                float R[3] = { (t[0] + rp[0]) - bp[0], (t[1] + rp[1]) - bp[1], (t[2] + rp[2]) - bp[2] };
                if (!(oracle::Fn<float __cdecl(void*)>(0x7EDAC0)(R) > 0.00001f)) continue;
                oracle::Fn<float __cdecl(void*, void*)>(0x7ED9B0)(R, R);
                float O[3] = { offWS[0] - bp[0], offWS[1] - bp[1], offWS[2] - bp[2] };
                if (!(oracle::Fn<float __cdecl(void*)>(0x7EDAC0)(O) > 0.00001f)) continue;
                oracle::Fn<float __cdecl(void*, void*)>(0x7ED9B0)(O, O);
                const float dot = (O[1] * R[1] + O[0] * R[0]) + O[2] * R[2];
                if (!(dot < 0.997f)) continue;
                uint8_t* const par = Nf<uint8_t*>(i, oParent);
                const void* const pm = par ? (par + oWld) : P(kPivotMat);
                float q[4]; oracle::Fn<int __cdecl(void*, const void*)>(0x7EB5C0)(q, pm);   // imag.xyz, real
                const float mag = ((q[0] * q[0] + q[1] * q[1]) + q[2] * q[2]) + q[3] * q[3];
                if (mag > 0.f) { const float rc = 1.f / mag; q[3] *= rc; q[0] *= -rc; q[1] *= -rc; q[2] *= -rc; }
                float cr[3] = { O[2] * R[1] - O[1] * R[2], O[0] * R[2] - O[2] * R[0], O[1] * R[0] - O[0] * R[1] }, axis[3];
                oracle::Fn<void* __cdecl(void*, const void*, int, const void*)>(0x7EBBB0)(axis, cr, 1, q);
                const float sp = ExeThis<float>(0x616CB0, bone);
                const float ac = (float)ExeAcos((double)dot);
                static const int var = std::getenv("W3K_V") ? std::atoi(std::getenv("W3K_V")) : 0;
                const double acd = ExeAcos((double)dot);
                float angle = ((sp * ac) * At<float>(kChain + cSpeed)) * 57.2957763671875f;
                if (var == 1) angle = (float)((((double)sp * (double)ac) * (double)At<float>(kChain + cSpeed)) * 57.2957763671875);
                if (var == 2) angle = (float)((((double)sp * acd) * (double)At<float>(kChain + cSpeed)) * 57.2957763671875);
                if (var == 3) angle = (float)((((double)sp * acd) * (double)At<float>(kChain + cSpeed)));
                if (var == 3) angle = angle * 57.2957763671875f;
                if (var == 5) angle = ((float)((double)sp * acd) * At<float>(kChain + cSpeed)) * 57.2957763671875f;
                if (var == 4) angle = (float)(((double)sp * acd) * (double)At<float>(kChain + cSpeed)) * 57.2957763671875f;
                oracle::Fn<void* __cdecl(void*, const void*, float, int)>(0x7EB7C0)(bone + oQ, axis, angle, 2);
                ExeThis<void, float>(0x617650, bone, At<float>(kChain + cBlend));
                ExeThis<void, const void*>(0x616CD0, bone, pm);
            }
        }
        std::memcpy(outC, g_fx, 0x1000);
        std::memcpy(g_fx, save, 0x1000);
        const bool pe = std::memcmp(outP, outE, 0x740) != 0, ce = std::memcmp(outC, outE, 0x740) != 0, pc2 = std::memcmp(outP, outC, 0x740) != 0;
        nPortExe += pe; nCompExe += ce; nPortComp += pc2;
        if (pe && shown++ < 3) { uint32_t a[4], b[4], c[4]; std::memcpy(a, outP + oQ, 16); std::memcpy(b, outE + oQ, 16); std::memcpy(c, outC + oQ, 16); std::printf("  probe it %d  q port %08X %08X %08X %08X  exe %08X %08X %08X %08X  comp %08X %08X %08X %08X\n", it, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3], c[0], c[1], c[2], c[3]); }
    }
    _controlfp_s(&tmp, pc, _MCW_PC);
    std::printf("  MoveBonesToTarget probe (g_n=%d): port!=exe %d, composed-from-exe!=exe %d, port!=composed %d of 1500\n", g_n, nPortExe, nCompExe, nPortComp);
}
} // namespace w3k
