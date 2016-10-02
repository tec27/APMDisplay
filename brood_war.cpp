#include "./brood_war.h"

namespace apm {

void BroodWar::InjectHooks() {
  drawDetour.Inject();
}

void BroodWar::RestoreHooks() {
  drawDetour.Restore();
}

}  // namespace apm