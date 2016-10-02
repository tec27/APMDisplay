#include "./brood_war.h"

namespace apm {

void BroodWar::InjectHooks() {
  drawDetour.Inject();
  refreshScreenDetour.Inject();
}

void BroodWar::RestoreHooks() {
  drawDetour.Restore();
  refreshScreenDetour.Restore();
}

}  // namespace apm