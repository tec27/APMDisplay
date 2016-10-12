#include "./brood_war.h"

namespace apm {

void BroodWar::InjectHooks() {
  drawDetour.Inject();
  refreshScreenDetour.Inject();
  onActionDetour.Inject();
}

void BroodWar::RestoreHooks() {
  drawDetour.Restore();
  refreshScreenDetour.Restore();
  onActionDetour.Restore();
}

}  // namespace apm