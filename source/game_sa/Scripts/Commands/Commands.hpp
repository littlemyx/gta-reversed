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
namespace g13_15 { void RegisterHandlers(); }; // S6-C: ids 1300..1599
}; // namespace ported
}; // namespace commands
}; // namespace notsa
}; // namespace script
