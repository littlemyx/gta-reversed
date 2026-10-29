# ASM-S30h (Camera.cpp CCamera::Constructor 0x51A450)

Result table: .notes/asm_sweep/S30h.result.tsv. MATCH, no code change.
Exe order: CPlaceable ctor, vtable, 3x CCam ctor (0x517730 just calls CCam::Init, port CCam() does too), 4x CCamPathSplines (ptr = 0), 5 trivial 0x10 elements (0x509800 is a bare ret), the six CMatrix members (+0x40 / +0x44 = 0, rest left as the zeroed global), m_matrix (+0x14) = &m_mCameraMatrix, m_nShakeType (+0xCA0) = 1, +0x53 = 0, duck factors (+0xC54 / +0xC58) = 0, InitialiseScriptableComponents. The port member initialisers plus the five stores cover all of it.
