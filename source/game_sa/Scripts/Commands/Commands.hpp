#pragma once

namespace notsa {
namespace script {
namespace commands {
namespace basic { void RegisterHandlers(); };
namespace camera { void RegisterHandlers(); };
namespace character { void RegisterHandlers(); };
namespace clock { void RegisterHandlers(); };
namespace conversation { void RegisterHandlers(); };
namespace comparasion { void RegisterHandlers(); };
namespace game { void RegisterHandlers(); };
namespace generic { void RegisterHandlers(); };
namespace math { void RegisterHandlers(); };
namespace mission { void RegisterHandlers(); };
namespace object { void RegisterHandlers(); };
namespace pad { void RegisterHandlers(); };
namespace path { void RegisterHandlers(); };
namespace ped { void RegisterHandlers(); };
namespace player { void RegisterHandlers(); };
namespace script { void RegisterHandlers(); };
namespace sequence { void RegisterHandlers(); };
namespace text { void RegisterHandlers(); };
namespace draw { void RegisterHandlers(); };
namespace unused { void RegisterHandlers(); };
namespace utility { void RegisterHandlers(); };
namespace vehicle { void RegisterHandlers(); };
namespace zone { void RegisterHandlers(); };
namespace stat { void RegisterHandlers(); };
// S6: handlers ported from the exe's per-100 group processors (script commands that had no handler)
namespace ported {
namespace g01_04 { void RegisterHandlers(); }; // S6-A part 1: ids 100..499
namespace g05_08 { void RegisterHandlers(); }; // S6-A part 2: ids 500..899
namespace g09_12 { void RegisterHandlers(); }; // S6-B: ids 900..1299
namespace g13_15 { void RegisterHandlers(); }; // S6-C: ids 1300..1599
namespace g20_21 { void RegisterHandlers(); }; // S6-F: ids 2000..2199
namespace g16 { void RegisterHandlers(); }; // S6-D part 1: ids 1600..1699
namespace g17b { void RegisterHandlers(); }; // S6-D part 2: ids 1700..1799 (1/2)
namespace g17c { void RegisterHandlers(); }; // S6-D part 2 (2/2)
}; // namespace ported
}; // namespace commands
}; // namespace notsa
}; // namespace script
